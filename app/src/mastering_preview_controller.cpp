#include "mastering_preview_controller.hpp"

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QMetaObject>
#include <QFile>

#include <chrono>
#include <utility>

namespace rgsml::app {

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

        if (previewExecutor_) {
            renderResult = previewExecutor_(job);
        } else {
            auto req_res = render::RenderRequest::create(
                job.preparedSnapshot->view(),
                job.preparedSnapshot->view().absolute_range(),
                job.chain,
                job.bindings,
                *core::FrameCount::create(4096).value());

            if (req_res) {
                // Never render an opt-in P2 chain against the default registry.
                const auto& jobRegistry = job.registry ? *job.registry : registry_;
                renderResult = render::render_preview(*req_res.value(), jobRegistry);
            } else {
                renderResult = core::Result<render::RenderResult>::failure(*req_res.error());
            }
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
                    "generation=%1\tframes=%2\trate_hz=%3\tdsp_active=%4\tp2_present=%5\tp2_active=%6\trender_ms=%7\tresult=%8\n")
                    .arg(job.generation)
                    .arg(view.frame_count().value())
                    .arg(view.format().sample_rate().value())
                    .arg(activeCount)
                    .arg(p2Present ? 1 : 0)
                    .arg(p2Active ? 1 : 0)
                    .arg(renderMs)
                    .arg(renderResult ? QStringLiteral("OK") : QStringLiteral("ERROR"));
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
