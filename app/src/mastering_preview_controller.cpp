#include "mastering_preview_controller.hpp"

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QMetaObject>
#include <QFile>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace rgsml::app {
namespace {

// Real Product candidate: one immutable PCM prefix per worker thread,
// bounded to 256 MiB of audio storage. SOURCE ownership is a strong
// shared_ptr<const RenderResult>; its PCM cannot mutate in place. All
// upstream module instances, typed parameters, registry and block size must
// match exactly. Nothing is shared with the GUI or WASAPI callback.
struct P2PrefixCache final {
    std::shared_ptr<const render::RenderResult> preparedOwner;
    std::shared_ptr<const dsp::ModuleRegistry> registry;
    std::vector<dsp::ModuleInstance> upstreamInstances;
    std::vector<dsp::ModuleExecutionBinding> upstreamBindings;
    std::shared_ptr<const render::RenderResult> fourDspResult;
    std::int64_t blockFrames{0};
    std::size_t bytes{0};
};

[[nodiscard]] bool same_pcm_and_provenance(
    const render::RenderResult& fresh,
    const render::RenderResult& incremental)
{
    if (fresh.render_window()!=incremental.render_window()
        || fresh.frame_domain_id()!=incremental.frame_domain_id()
        || fresh.chain_revision()!=incremental.chain_revision()
        || fresh.signatures()!=incremental.signatures()
        || fresh.compressor_telemetry_sidecar()
            !=incremental.compressor_telemetry_sidecar()
        // Stereo/M-S captures do not expose a structural equality
        // operator. An incremental candidate is admissible only when
        // BOTH complete render paths have NO M15 stage sidecars.
        // Never substitute or discard any genuine M15 evidence.
        || !fresh.stereo_ms_stage_output_sidecars().empty()
        || !incremental.stereo_ms_stage_output_sidecars().empty())
        return false;
    const auto a=fresh.view();
    const auto b=incremental.view();
    if(a.format()!=b.format() || a.absolute_range()!=b.absolute_range())
        return false;
    for(std::size_t ch=0;ch<a.format().channel_count();++ch) {
        const auto x=a.channel(ch);
        const auto y=b.channel(ch);
        if(!x||!y||x.value()->size()!=y.value()->size())return false;
        for(std::size_t i=0;i<x.value()->size();++i) {
            if(std::bit_cast<std::uint64_t>((*x.value())[i])
                !=std::bit_cast<std::uint64_t>((*y.value())[i]))
                return false;
        }
    }
    return true;
}

[[nodiscard]] core::Result<render::RenderResult>
render_with_bounded_p2_prefix(
    const MasteringPreviewController::PreviewJob& job,
    const dsp::ModuleRegistry& registry,
    P2PrefixCache& cache,
    bool& cacheHit)
{
    cacheHit=false;
    const auto view=job.preparedSnapshot->view();
    const auto block=*core::FrameCount::create(4096).value();
    auto request=render::RenderRequest::create(
        view,view.absolute_range(),job.chain,job.bindings,block);
    if(!request)return core::Result<render::RenderResult>::failure(
        *request.error());
    const auto fresh=[&]() {
        return render::render_preview(*request.value(),registry);
    };
    const auto items=job.chain.instances();
    constexpr auto maxPcmBytes=std::size_t{256}*1024U*1024U;
    if(items.size()!=5U || job.bindings.size()!=5U
        || !job.registry
        || items[0].module_type_id()!="rgsml.dsp.gain"
        || items[1].module_type_id()!="rgsml.dsp.parametric-eq"
        || items[2].module_type_id()!="rgsml.dsp.compressor"
        || items[3].module_type_id()!="rgsml.dsp.stereo-ms"
        || items[4].module_type_id()
            !="rgsml.dsp.streaming-true-peak-limiter"
        || items[2].active() || items[3].active()
        || view.timebase().frame_domain_id()
            !=audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return fresh();
    for(std::size_t i=0;i<5U;++i)
        if(job.bindings[i].instance_id!=items[i].instance_id())
            return fresh();
    const auto frames=view.frame_count().value();
    const auto channels=view.format().channel_count();
    if(frames<=0 || channels==0 ||
        static_cast<std::uint64_t>(frames)
            >maxPcmBytes/(sizeof(double)*channels))
        return fresh();

    std::vector<dsp::ModuleInstance> upstream{
        items.begin(),items.begin()+4};
    std::vector<dsp::ModuleExecutionBinding> upstreamBindings{
        job.bindings.begin(),job.bindings.begin()+4};
    if(cache.fourDspResult && cache.preparedOwner==job.preparedSnapshot
        &&cache.registry==job.registry
        &&cache.upstreamInstances==upstream
        &&cache.upstreamBindings==upstreamBindings
        &&cache.blockFrames==block.value()) {
        auto reused=render::render_preview_p2_cached_prefix(
            *request.value(),*cache.fourDspResult,registry);
        if(reused) {
            cacheHit=true;
            return reused;
        }
        // Unknown or now-incompatible metadata: fail closed to fresh render.
        cache={};
        return fresh();
    }

    // Cold calibration is deliberately an independent fresh oracle. An
    // unsuccessful, non-bit-exact or sidecar-bearing split NEVER enters
    // this Product cache. Cold time may be slower; changed P2 settings
    // reuse ONLY the previously verified immutable prefix.
    auto oracle=fresh();
    if(!oracle)return oracle;
    auto prefixChain=dsp::ProcessingChain::restore(
        registry,job.chain.context(),job.chain.revision(),
        std::move(upstream));
    if(!prefixChain)return oracle;
    auto prefixRequest=render::RenderRequest::create(
        view,view.absolute_range(),*prefixChain.value(),
        std::move(upstreamBindings),block);
    if(!prefixRequest)return oracle;
    auto produced=render::render_preview(*prefixRequest.value(),registry);
    if(!produced || produced.value()->compressor_telemetry_sidecar()
        || !produced.value()->stereo_ms_stage_output_sidecars().empty())
        return oracle;
    auto recomposed=render::render_preview_p2_cached_prefix(
        *request.value(),*produced.value(),registry);
    if(!recomposed
        || !same_pcm_and_provenance(*oracle.value(),*recomposed.value()))
        return oracle;
    // Cache admission is after a complete exact sample + sidecar oracle.
    // Strong SOURCE ownership, exact upstream typed key and one PCM budget.
    cache.preparedOwner=job.preparedSnapshot;
    cache.registry=job.registry;
    cache.upstreamInstances.assign(
        items.begin(),items.begin()+4);
    cache.upstreamBindings.assign(
        job.bindings.begin(),job.bindings.begin()+4);
    cache.fourDspResult=std::make_shared<const render::RenderResult>(
        std::move(*produced.value()));
    cache.blockFrames=block.value();
    cache.bytes=static_cast<std::size_t>(frames)*channels*sizeof(double);
    return oracle;
}

} // namespace

MasteringPreviewController::MasteringPreviewController(
    MasteringChainState* chainState,
    PreparedSnapshotProvider snapshotProvider,
    ProcessedRealizationPublisher publisher,
    QObject* parent)
    : QObject(parent)
    , chainState_(chainState)
    , snapshotProvider_(std::move(snapshotProvider))
    , publisher_(std::move(publisher))
{
    workerThread_ = std::jthread([this] { worker_loop(); });
}

MasteringPreviewController::~MasteringPreviewController()
{
    {
        const std::lock_guard<std::mutex> lock(workerMutex_);
        workerStopping_ = true;
    }
    workerCond_.notify_all();
}

void MasteringPreviewController::set_chain_state(MasteringChainState* chainState) noexcept
{
    chainState_ = chainState;
}

void MasteringPreviewController::set_snapshot_provider(PreparedSnapshotProvider snapshotProvider)
{
    snapshotProvider_ = std::move(snapshotProvider);
}

void MasteringPreviewController::set_publisher(ProcessedRealizationPublisher publisher)
{
    publisher_ = std::move(publisher);
}

void MasteringPreviewController::set_preview_executor(PreviewExecutor executor)
{
    previewExecutor_ = std::move(executor);
}

void MasteringPreviewController::set_research_p2_timing_log_path(QString path)
{
    // Only research startup calls this, before the first worker job.
    const std::lock_guard<std::mutex> lock(workerMutex_);
    researchP2TimingLogPath_ = std::move(path);
}

std::uint64_t MasteringPreviewController::preview_generation() const noexcept
{
    return previewGeneration_;
}

QString MasteringPreviewController::preview_status() const
{
    return previewStatus_;
}

QString MasteringPreviewController::preview_error() const
{
    return previewError_;
}

std::uint64_t MasteringPreviewController::stale_results_discarded() const noexcept
{
    return staleResultsDiscarded_;
}

void MasteringPreviewController::request_preview()
{
    if (!chainState_) {
        return;
    }

    ++previewGeneration_;

    std::shared_ptr<const render::RenderResult> preparedSnapshot;
    if (snapshotProvider_) {
        preparedSnapshot = snapshotProvider_();
    }

    if (!preparedSnapshot) {
        previewStatus_ = QStringLiteral("IDLE");
        previewError_.clear();
        emit changed();
        return;
    }

    previewStatus_ = QStringLiteral("RENDERING");
    previewError_.clear();

    PreviewJob job{
        previewGeneration_,
        std::move(preparedSnapshot),
        chainState_->chain(),
        chainState_->execution_bindings(),
        chainState_->registry_snapshot()
    };

    {
        const std::lock_guard<std::mutex> lock(workerMutex_);
        pendingJob_ = std::move(job);
    }
    workerCond_.notify_all();
    emit changed();
}

void MasteringPreviewController::worker_loop()
{
    // No lock or cache operations on the callback / GUI thread.
    P2PrefixCache p2Cache;
    while (true) {
        std::optional<PreviewJob> jobOpt;
        {
            std::unique_lock<std::mutex> lock(workerMutex_);
            workerCond_.wait(lock, [this] {
                return workerStopping_ || pendingJob_.has_value();
            });

            if (workerStopping_) {
                return;
            }

            jobOpt = std::move(pendingJob_);
            pendingJob_.reset();
        }

        PreviewJob job = std::move(*jobOpt);
        const auto perfStart = std::chrono::steady_clock::now();

        core::Result<render::RenderResult> renderResult = core::Result<render::RenderResult>::failure(
            core::Error{core::ErrorCode::InvalidState, "No renderer."});

        bool cacheHit=false;
        if (previewExecutor_) {
            renderResult = previewExecutor_(job);
        } else {
            // Immutable registry corresponding to the exact accepted
            // five-DSP snapshot: no fallback to an unrelated P2 factory.
            const auto& jobRegistry = job.registry ? *job.registry : registry_;
            renderResult = render_with_bounded_p2_prefix(
                job,jobRegistry,p2Cache,cacheHit);
        }

        const auto renderMs = std::chrono::duration_cast<
            std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - perfStart).count();
        // Research only; no log I/O on GUI thread, DSP process, or WASAPI
        // callback. Record wall time even for errors/cancelled generations.
        if (!researchP2TimingLogPath_.isEmpty()) {
            const auto view = job.preparedSnapshot->view();
            unsigned activeCount = 0;
            bool p2Active = false;
            bool p2Present = false;
            for (const auto& node : job.chain.instances()) {
                if (node.active()) ++activeCount;
                if (node.module_type_id()
                    == "rgsml.dsp.streaming-true-peak-limiter") {
                    p2Present = true;
                    p2Active = node.active();
                }
            }
            QFile log{researchP2TimingLogPath_};
            if (log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                const auto row = QStringLiteral(
                    "generation=%1\tframes=%2\trate_hz=%3\tdsp_active=%4\tp2_present=%5\tp2_active=%6\trender_ms=%7\tresult=%8\tcache_hit=%9\n")
                    .arg(job.generation)
                    .arg(view.frame_count().value())
                    .arg(view.format().sample_rate().value())
                    .arg(activeCount)
                    .arg(p2Present ? 1 : 0)
                    .arg(p2Active ? 1 : 0)
                    .arg(renderMs)
                    .arg(renderResult ? QStringLiteral("OK") : QStringLiteral("ERROR"))
                    .arg(cacheHit?1:0);
                log.write(row.toUtf8());
            }
        }
        auto outcome = std::make_shared<core::Result<render::RenderResult>>(std::move(renderResult));
        const auto gen = job.generation;
        const auto preparedForJob = job.preparedSnapshot;

        QMetaObject::invokeMethod(
            this,
            [this, gen, outcome, preparedForJob] {
                // A source switch may not trigger a new DSP preview; check
                // the ORIGINAL prepared owner before publishing stale audio.
                if (!snapshotProvider_ || snapshotProvider_() != preparedForJob) {
                    ++staleResultsDiscarded_;
                    return;
                }
                publish_preview_result(gen, outcome);
            },
            Qt::QueuedConnection);
    }
}

void MasteringPreviewController::publish_preview_result(
    std::uint64_t generation,
    std::shared_ptr<core::Result<render::RenderResult>> outcome)
{
    if (generation != previewGeneration_) {
        ++staleResultsDiscarded_;
        return;
    }

    if (!outcome || !*outcome) {
        previewStatus_ = QStringLiteral("ERROR");
        if (outcome && outcome->error()) {
            previewError_ = QString::fromStdString(outcome->error()->message());
        } else {
            previewError_ = QStringLiteral("Render failed.");
        }
        emit changed();
        return;
    }

    render::RenderResult result = std::move(*outcome->value());

    if (publisher_) {
        const auto status = publisher_(std::move(result));
        if (!status) {
            previewStatus_ = QStringLiteral("ERROR");
            previewError_ = QString::fromStdString(status.error()->message());
            emit changed();
            return;
        }
    }

    previewStatus_ = QStringLiteral("READY");
    previewError_.clear();
    emit changed();
}

}  // namespace rgsml::app
