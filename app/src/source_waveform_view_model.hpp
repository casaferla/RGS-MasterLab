#pragma once

#include "waveform_presentation.hpp"

#include <rgsml/audio/waveform_summary.hpp>
#include <rgsml/core/resource_reference.hpp>
#include <rgsml/core/result.hpp>

#include <QObject>

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

namespace rgsml::app {

class SourceWaveformViewModel final : public QObject {
    Q_OBJECT

public:
    using BuildFunction = std::function<core::Result<audio::WaveformSummary>(
        const core::ResourceReference&,
        std::stop_token)>;

    explicit SourceWaveformViewModel(
        ui::WaveformPresentation* presentation,
        QObject* parent = nullptr);
    SourceWaveformViewModel(
        ui::WaveformPresentation* presentation,
        BuildFunction buildFunction,
        QObject* parent = nullptr);
    ~SourceWaveformViewModel() override;

    SourceWaveformViewModel(const SourceWaveformViewModel&) = delete;
    SourceWaveformViewModel& operator=(const SourceWaveformViewModel&) = delete;

    void source_committed(const core::ResourceReference& source);
    Q_INVOKABLE void retry();

    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] std::uint64_t source_replacement_cancellations() const noexcept;
    [[nodiscard]] std::uint64_t stale_results_discarded() const noexcept;

private:
    struct Job final {
        std::uint64_t generation;
        core::ResourceReference reference;
    };

    static core::Result<audio::WaveformSummary> build_from_windows_source(
        const core::ResourceReference& source,
        std::stop_token stopToken);
    void enqueue(const core::ResourceReference& source);
    void worker_loop();
    void publish_result(
        std::uint64_t generation,
        std::shared_ptr<core::Result<audio::WaveformSummary>> outcome,
        bool cancelled);

    ui::WaveformPresentation* presentation_;
    BuildFunction buildFunction_;
    std::shared_ptr<const audio::WaveformSummary> currentSummary_;
    std::optional<core::ResourceReference> currentReference_;
    std::uint64_t generation_{0U};
    std::uint64_t sourceReplacementCancellations_{0U};
    std::uint64_t staleResultsDiscarded_{0U};

    std::mutex mutex_;
    std::condition_variable condition_;
    std::optional<Job> pending_;
    std::stop_source activeStop_;
    bool workerActive_{false};
    bool stopping_{false};
    std::jthread worker_;
};

}  // namespace rgsml::app
