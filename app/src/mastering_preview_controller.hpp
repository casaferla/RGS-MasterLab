#pragma once

#include "mastering_chain_state.hpp"

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_result.hpp>

#include <QObject>
#include <QString>

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace rgsml::app {

class MasteringPreviewController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(quint64 previewGeneration READ preview_generation NOTIFY changed)
    Q_PROPERTY(QString previewStatus READ preview_status NOTIFY changed)
    Q_PROPERTY(QString previewError READ preview_error NOTIFY changed)

public:
    using PreparedSnapshotProvider = std::function<std::shared_ptr<const render::RenderResult>()>;
    using ProcessedRealizationPublisher = std::function<core::Status(render::RenderResult)>;

    struct PreviewJob final {
        std::uint64_t generation{0};
        std::shared_ptr<const render::RenderResult> preparedSnapshot;
        dsp::ProcessingChain chain;
        std::vector<dsp::ModuleExecutionBinding> bindings;
    };

    using PreviewExecutor = std::function<core::Result<render::RenderResult>(const PreviewJob& job)>;

    explicit MasteringPreviewController(
        MasteringChainState* chainState = nullptr,
        PreparedSnapshotProvider snapshotProvider = nullptr,
        ProcessedRealizationPublisher publisher = nullptr,
        QObject* parent = nullptr);
    ~MasteringPreviewController() override;

    MasteringPreviewController(const MasteringPreviewController&) = delete;
    MasteringPreviewController& operator=(const MasteringPreviewController&) = delete;

    void set_chain_state(MasteringChainState* chainState) noexcept;
    void set_snapshot_provider(PreparedSnapshotProvider snapshotProvider);
    void set_publisher(ProcessedRealizationPublisher publisher);
    void set_preview_executor(PreviewExecutor executor);

    [[nodiscard]] const PreparedSnapshotProvider& snapshot_provider() const noexcept { return snapshotProvider_; }
    [[nodiscard]] std::uint64_t preview_generation() const noexcept;
    [[nodiscard]] QString preview_status() const;
    [[nodiscard]] QString preview_error() const;
    [[nodiscard]] std::uint64_t stale_results_discarded() const noexcept;

    Q_INVOKABLE void request_preview();

signals:
    void changed();

private:
    void worker_loop();
    void publish_preview_result(
        std::uint64_t generation,
        std::shared_ptr<core::Result<render::RenderResult>> outcome);

    MasteringChainState* chainState_{nullptr};
    PreparedSnapshotProvider snapshotProvider_;
    ProcessedRealizationPublisher publisher_;
    PreviewExecutor previewExecutor_;
    dsp::ModuleRegistry registry_{*dsp::ModuleRegistry::create_dsp_package_v1().value()};

    std::uint64_t previewGeneration_{0};
    std::uint64_t staleResultsDiscarded_{0};
    QString previewStatus_{QStringLiteral("IDLE")};
    QString previewError_;

    std::mutex workerMutex_;
    std::condition_variable workerCond_;
    std::optional<PreviewJob> pendingJob_;
    bool workerStopping_{false};
    std::jthread workerThread_;
};

}  // namespace rgsml::app
