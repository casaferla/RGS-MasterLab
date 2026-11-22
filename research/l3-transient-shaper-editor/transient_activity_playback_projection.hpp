#pragma once
// Research-only pure binding from authentic C00-gain history to the original
// Product playback identity. No Qt dependency or independent playback clock.
// Refuse ambiguous handoff, stale chain revision and mismatched frame domains.
#include "transient_activity_collector.hpp"
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/realization_identity.hpp>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace rgsml::research::transient_ui {

struct TransientActivityRenderIdentity final {
    rgsml::core::RealizationId realization_id{};
    std::uint64_t chain_revision{};
    rgsml::audio::FrameDomainId frame_domain{
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE};
    std::uint32_t sample_rate_hz{};
    bool sidecar_valid{false};
};

enum class PlaybackActivityStatus : std::uint8_t {
    UNAVAILABLE,
    NOT_AUDITIONING_PROCESSED,
    BYPASS,
    STOPPED,
    TRANSITION,
    STALE_REALIZATION,
    STALE_REVISION,
    FRAME_DOMAIN_MISMATCH,
    SAMPLE_RATE_MISMATCH,
    INCOMPLETE_TELEMETRY,
    PAUSED,
    ACTIVE
};

struct PlaybackActivityProjection final {
    PlaybackActivityStatus status{PlaybackActivityStatus::UNAVAILABLE};
    std::optional<rgsml::core::RealizationId> realization_id;
    std::uint64_t audible_frame{};
    std::uint32_t sample_rate_hz{};
    std::vector<AppliedGainBucket> history;

    [[nodiscard]] bool available() const noexcept {
        return status == PlaybackActivityStatus::ACTIVE
            || status == PlaybackActivityStatus::PAUSED;
    }
};

// Exactly one accepted playback snapshot governs the projection. The
// real owner must supply frame_domain and sample_rate for the same audio
// timeline; NEVER convert source and output frames without explicit
// authoritative SRC mapping. A fully rendered immutable sidecar is required.
// PAUSED retains the same window. Seek/loop behavior is inherited from the
// real snapshot's absolute FrameIndex; there is no animation clock here.
[[nodiscard]] inline PlaybackActivityProjection project_transient_activity(
    const TransientActivityCollector& collector,
    const TransientActivityRenderIdentity& sidecar,
    const rgsml::core::PlaybackSnapshot& playback,
    rgsml::audio::FrameDomainId playback_frame_domain,
    std::uint32_t playback_sample_rate_hz,
    std::uint64_t accepted_chain_revision,
    bool auditioning_processed,
    bool actually_bypassed)
{
    using namespace rgsml;
    PlaybackActivityProjection out;
    if (!sidecar.sidecar_valid
        || collector.status() != ObservationStatus::OK
        || sidecar.realization_id.value == 0U
        || sidecar.sample_rate_hz == 0U)
        return out;
    if (!auditioning_processed) {
        out.status=PlaybackActivityStatus::NOT_AUDITIONING_PROCESSED;
        return out;
    }
    if (actually_bypassed) {
        out.status=PlaybackActivityStatus::BYPASS;
        return out;
    }
    if (playback.state != core::PlaybackState::PLAYING
        && playback.state != core::PlaybackState::PAUSED) {
        out.status=PlaybackActivityStatus::STOPPED;
        return out;
    }
    // A transition can expose OLD/NEW or an inconsistent audible PCM.
    // Show no chart unless the source positively asserts stable NEW.
    if (playback.audibleRealization.phase
        != core::AudibleHandoffPhase::NEW
        || !playback.audibleRealization.realizationId) {
        out.status=PlaybackActivityStatus::TRANSITION;
        return out;
    }
    if (*playback.audibleRealization.realizationId
        != sidecar.realization_id) {
        out.status=PlaybackActivityStatus::STALE_REALIZATION;
        return out;
    }
    if (sidecar.chain_revision != accepted_chain_revision) {
        out.status=PlaybackActivityStatus::STALE_REVISION;
        return out;
    }
    if (sidecar.frame_domain != playback_frame_domain) {
        out.status=PlaybackActivityStatus::FRAME_DOMAIN_MISMATCH;
        return out;
    }
    if (sidecar.sample_rate_hz != playback_sample_rate_hz) {
        out.status=PlaybackActivityStatus::SAMPLE_RATE_MISMATCH;
        return out;
    }
    if (!collector.finished() || playback.position.value() < 0) {
        out.status=PlaybackActivityStatus::INCOMPLETE_TELEMETRY;
        return out;
    }
    const auto frame=static_cast<std::uint64_t>(playback.position.value());
    out.history=collector.visible_window(
        frame, static_cast<std::uint64_t>(playback_sample_rate_hz) * 5U);
    if (out.history.empty()) {
        out.status=PlaybackActivityStatus::INCOMPLETE_TELEMETRY;
        return out;
    }
    out.audible_frame=frame;
    out.sample_rate_hz=playback_sample_rate_hz;
    out.realization_id=sidecar.realization_id;
    out.status=playback.state==core::PlaybackState::PAUSED
        ?PlaybackActivityStatus::PAUSED : PlaybackActivityStatus::ACTIVE;
    return out;
}
} // namespace rgsml::research::transient_ui
