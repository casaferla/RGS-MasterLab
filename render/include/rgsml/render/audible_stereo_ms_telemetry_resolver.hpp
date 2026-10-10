#pragma once

#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/render/render_result.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace rgsml::render {

// B4c2a: portable, observational binding only. No UI or audible DSP mutations.
enum class StereoMsAudibleStatus : std::uint8_t {
    UNAVAILABLE,
    NOT_AUDITIONED,
    STOPPED,
    PAUSED,
    TRANSITION,
    BYPASS,
    ACTIVE,
};

class AudibleStereoMsTelemetryResolver final {
public:
    static constexpr std::size_t kMaxHistoryBuckets = 6U;

    // Sidecars are immutable and share the lifetime of the accepted render.
    // Registration by itself NEVER makes a candidate audible.
    void register_sidecar(std::shared_ptr<const StereoMsStageOutputSidecar> sidecar);
    // BYPASS must be taken from the accepted realization's actual execution
    // disposition, never from the editor's current (possibly newer) draft.
    void register_bypass(rgsml::core::RealizationId realization_id);
    void reset() noexcept;

    void update(const rgsml::core::PlaybackSnapshot& snapshot,
                bool auditioning_processed);

    [[nodiscard]] StereoMsAudibleStatus status() const noexcept { return status_; }
    [[nodiscard]] const std::vector<StereoMsDensityBucket>& density_history() const noexcept {
        return history_;
    }
    [[nodiscard]] const std::optional<StereoMsCorrelationWindow>& correlation() const noexcept {
        return correlation_;
    }
    [[nodiscard]] const std::optional<StereoMsSideLowWindow>& side_low() const noexcept {
        return side_low_;
    }
    [[nodiscard]] bool gap_detected() const noexcept { return gap_detected_; }
    [[nodiscard]] std::optional<rgsml::core::RealizationId> active_realization_id() const noexcept {
        return active_realization_id_;
    }
    [[nodiscard]] std::size_t registered_count() const noexcept { return registered_.size(); }

private:
    struct Entry final {
        std::shared_ptr<const StereoMsStageOutputSidecar> sidecar;
        bool bypass{false};
    };

    void clear_observation() noexcept;
    void insert(rgsml::core::RealizationId id, Entry entry);

    std::map<std::uint64_t, Entry> registered_;
    std::vector<StereoMsDensityBucket> history_;
    std::optional<StereoMsCorrelationWindow> correlation_;
    std::optional<StereoMsSideLowWindow> side_low_;
    StereoMsAudibleStatus status_{StereoMsAudibleStatus::UNAVAILABLE};
    std::optional<rgsml::core::RealizationId> active_realization_id_;
    std::optional<std::uint64_t> traversal_serial_;
    std::uint64_t seek_serial_{0};
    std::uint64_t loop_wrap_count_{0};
    std::optional<std::int64_t> eligible_begin_;
    std::optional<std::int64_t> last_cursor_;
    bool gap_detected_{false};
    bool previous_processed_{false};
};

}  // namespace rgsml::render
