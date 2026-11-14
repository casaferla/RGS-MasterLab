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
#include <list>
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

// Small worker-local LRU: multiple exact upstream configurations can be
// revisited by P2 tuning / future AutoMatch sweeps without recomputation.
// The 256 MiB cap applies to cached prefix PCM, not the independently owned
// SOURCE or published/audible render lifetimes. All operations are off-thread.
class BoundedP2PrefixLru final {
public:
    static constexpr std::size_t kMaxPcmBytes=256U*1024U*1024U;

    void invalidate_other_owner(
        const std::shared_ptr<const render::RenderResult>& prepared,
        const std::shared_ptr<const dsp::ModuleRegistry>& registry)
    {
        if(!entries_.empty()
            &&(entries_.front().preparedOwner!=prepared
               ||entries_.front().registry!=registry))
            clear();
    }

    [[nodiscard]] const P2PrefixCache* lookup(
        const std::shared_ptr<const render::RenderResult>& prepared,
        const std::shared_ptr<const dsp::ModuleRegistry>& registry,
        const std::vector<dsp::ModuleInstance>& instances,
        const std::vector<dsp::ModuleExecutionBinding>& bindings,
        std::int64_t blockFrames)
    {
        auto it=std::find_if(entries_.begin(),entries_.end(),
            [&](const P2PrefixCache& candidate) {
                return candidate.fourDspResult
                    &&candidate.preparedOwner==prepared
                    &&candidate.registry==registry
                    &&candidate.upstreamInstances==instances
                    &&candidate.upstreamBindings==bindings
                    &&candidate.blockFrames==blockFrames;
            });
        if(it==entries_.end())return nullptr;
        entries_.splice(entries_.begin(),entries_,it);
        return std::addressof(entries_.front());
    }

    void erase_mru() noexcept {
        if(entries_.empty())return;
        bytes_-=entries_.front().bytes;
        entries_.pop_front();
    }

    void store(P2PrefixCache entry) {
        if(entry.bytes>kMaxPcmBytes)return;
        invalidate_other_owner(entry.preparedOwner,entry.registry);
        while(bytes_>kMaxPcmBytes-entry.bytes && !entries_.empty()) {
            bytes_-=entries_.back().bytes;
            entries_.pop_back();
            ++evictions_;
        }
        entries_.push_front(std::move(entry));
        bytes_+=entries_.front().bytes;
    }

    void clear() noexcept {entries_.clear();bytes_=0;}
    [[nodiscard]] std::size_t size() const noexcept {return entries_.size();}
    [[nodiscard]] std::size_t bytes() const noexcept {return bytes_;}
    [[nodiscard]] std::uint64_t evictions() const noexcept {return evictions_;}
private:
    std::list<P2PrefixCache> entries_;
    std::size_t bytes_{0};
    std::uint64_t evictions_{0};
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
    BoundedP2PrefixLru& cache,
    bool& cacheHit)
{
    cacheHit=false;
    // No previous source/registry owner may remain pinned by the LRU after
    // a user SOURCE or Project transition, even on an ineligible chain.
    cache.invalidate_other_owner(job.preparedSnapshot,job.registry);
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
            >BoundedP2PrefixLru::kMaxPcmBytes/(sizeof(double)*channels))
        return fresh();

    std::vector<dsp::ModuleInstance> upstream{
        items.begin(),items.begin()+4};
    std::vector<dsp::ModuleExecutionBinding> upstreamBindings{
        job.bindings.begin(),job.bindings.begin()+4};
    if(const auto* hit=cache.lookup(
            job.preparedSnapshot,job.registry,upstream,upstreamBindings,
            block.value())) {
        auto reused=render::render_preview_p2_cached_prefix(
            *request.value(),*hit->fourDspResult,registry);
        if(reused) {
            cacheHit=true;
            return reused;
        }
        // Unknown or now-incompatible metadata: fail closed to fresh render.
        cache.erase_mru();
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
    P2PrefixCache admitted;
    admitted.preparedOwner=job.preparedSnapshot;
    admitted.registry=job.registry;
    admitted.upstreamInstances.assign(items.begin(),items.begin()+4);
    admitted.upstreamBindings.assign(
        job.bindings.begin(),job.bindings.begin()+4);
    admitted.fourDspResult=std::make_shared<const render::RenderResult>(
        std::move(*produced.value()));
    admitted.blockFrames=block.value();
    admitted.bytes=static_cast<std::size_t>(frames)*channels*sizeof(double);
    cache.store(std::move(admitted));
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
    BoundedP2PrefixLru p2Cache;
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
                    "generation=%1\tframes=%2\trate_hz=%3\tdsp_active=%4\tp2_present=%5\tp2_active=%6\trender_ms=%7\tresult=%8\tcache_hit=%9\tcache_entries=%10\tcache_pcm_bytes=%11\tcache_evictions=%12\n")
                    .arg(job.generation)
                    .arg(view.frame_count().value())
                    .arg(view.format().sample_rate().value())
                    .arg(activeCount)
                    .arg(p2Present ? 1 : 0)
                    .arg(p2Active ? 1 : 0)
                    .arg(renderMs)
                    .arg(renderResult ? QStringLiteral("OK") : QStringLiteral("ERROR"))
                    .arg(cacheHit?1:0)
                    .arg(p2Cache.size())
                    .arg(p2Cache.bytes())
                    .arg(p2Cache.evictions());
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
