#include "mastering_preview_controller.hpp"

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QMetaObject>

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
        chainState_->execution_bindings()
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
                renderResult = render::render_preview(*req_res.value(), registry_);
            } else {
                renderResult = core::Result<render::RenderResult>::failure(*req_res.error());
            }
        }

        auto outcome = std::make_shared<core::Result<render::RenderResult>>(std::move(renderResult));
        const auto gen = job.generation;

        QMetaObject::invokeMethod(
            this,
            [this, gen, outcome] {
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
