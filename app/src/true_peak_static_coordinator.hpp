#pragma once

#include "true_peak_static_binding.hpp"

#include <rgsml/core/resource_reference.hpp>

#include <QObject>
#include <QString>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace rgsml::app {

// C1-only lifecycle binder. Data are always measured on a worker and are
// only accepted by the GUI thread if the source/render generation still matches.
// No live peak bucket, UI DSP or release QUALIFIED status is produced here.
class TruePeakStaticCoordinator final : public QObject {
    Q_OBJECT
public:
    enum class State {
        NO_SOURCE, NO_PROCESSED, MEASURING, READY, UNAVAILABLE
    };

    explicit TruePeakStaticCoordinator(QObject* parent = nullptr);
    ~TruePeakStaticCoordinator() override;
    TruePeakStaticCoordinator(const TruePeakStaticCoordinator&) = delete;
    TruePeakStaticCoordinator& operator=(const TruePeakStaticCoordinator&) = delete;

    // Call BEFORE the selector invalidates/switches to a new source.
    void invalidate_source();
    // Call only after SourceSelectionViewModel and selector both committed.
    void source_committed(core::ResourceReference reference,
                          TruePeakExpectedSource expected);
    // Call from the existing AuditionSourceSelector changed() signal.
    void processed_changed(std::shared_ptr<const render::RenderResult> render,
                           std::optional<core::RealizationId> id);

    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] const BoundStaticTruePeak* ready() const noexcept
    { return completed_ ? &*completed_ : nullptr; }
    [[nodiscard]] const core::ResourceReference* source_reference() const noexcept
    { return source_ ? &*source_ : nullptr; }
    [[nodiscard]] std::uint64_t stale_discarded() const noexcept
    { return staleDiscarded_; }

signals:
    void changed();

private:
    struct Job {
        std::uint64_t generation;
        std::shared_ptr<const render::RenderResult> render;
        core::RealizationId realizationId;
        TruePeakExpectedSource expected;
    };
    void worker_loop();
    void complete_job(std::uint64_t generation,
                      std::shared_ptr<core::Result<BoundStaticTruePeak>> result);

    std::optional<core::ResourceReference> source_;
    std::optional<TruePeakExpectedSource> expected_;
    std::shared_ptr<const render::RenderResult> currentRender_;
    std::optional<core::RealizationId> currentId_;
    std::optional<BoundStaticTruePeak> completed_;
    State state_{State::NO_SOURCE};
    std::uint64_t generation_{0};
    std::uint64_t staleDiscarded_{0};

    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Job> pending_;
    bool stopping_{false};
    std::jthread worker_;
};

} // namespace rgsml::app
