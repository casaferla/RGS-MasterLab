#pragma once
// C01 post-L3 research: authoritative audible-realization projection.
// Does not infer Product playback timing, SRC mapping or rendered identity.
// It cannot be used until a real accepted PCM/sidecar provenance is supplied.
#include "dynamic_eq_activity_history.hpp"
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/realization_identity.hpp>
#include <cstdint>
#include <optional>
#include <vector>

namespace rgsml::research::dynamic_ui {
struct C01ActivityRenderProvenance final {
    rgsml::core::RealizationId realization_id{};
    std::uint64_t accepted_chain_revision{};
    rgsml::audio::FrameDomainId frame_domain{
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE};
    std::uint32_t sample_rate_hz{};
    bool sidecar_accepted{false};
};
enum class C01ProjectionStatus : std::uint8_t {
    UNAVAILABLE,NOT_PROCESSED,BYPASS,STOPPED,TRANSITION,
    STALE_REALIZATION,STALE_REVISION,FRAME_DOMAIN_MISMATCH,
    SAMPLE_RATE_MISMATCH,HISTORY_INCOMPLETE,PAUSED,ACTIVE
};
struct C01ActivityProjection final {
    C01ProjectionStatus status{C01ProjectionStatus::UNAVAILABLE};
    std::optional<rgsml::core::RealizationId> realization_id;
    std::uint64_t audible_frame{};
    std::uint32_t sample_rate_hz{};
    std::vector<C01ActivityBucket> history;
    [[nodiscard]] bool available() const noexcept {
        return status==C01ProjectionStatus::PAUSED
            ||status==C01ProjectionStatus::ACTIVE;
    }
};

// The caller MUST obtain one genuine PlaybackSnapshot and accepted PCM
// provenance from Product. A test snapshot alone is not audible truth.
// Absolute sample positions must share the SAME frame domain and rate.
// Never infer SRC or use a second, QML-owned animated clock.
[[nodiscard]] inline C01ActivityProjection project_c01_activity(
    const C01ActivityHistoryCollector& collector,
    const C01ActivityRenderProvenance& sidecar,
    const rgsml::core::PlaybackSnapshot& audible,
    rgsml::audio::FrameDomainId audible_frame_domain,
    std::uint32_t audible_sample_rate_hz,
    std::uint64_t accepted_chain_revision,
    bool auditioning_processed,
    bool actually_bypassed)
{
    using namespace rgsml;
    C01ActivityProjection out;
    if(!sidecar.sidecar_accepted
       ||sidecar.realization_id.value==0U
       ||sidecar.sample_rate_hz==0U
       ||collector.status()!=C01HistoryStatus::OK)
        return out;
    if(!auditioning_processed) {
        out.status=C01ProjectionStatus::NOT_PROCESSED;return out;
    }
    if(actually_bypassed) {
        out.status=C01ProjectionStatus::BYPASS;return out;
    }
    if(audible.state!=core::PlaybackState::PLAYING
       &&audible.state!=core::PlaybackState::PAUSED) {
        out.status=C01ProjectionStatus::STOPPED;return out;
    }
    if(audible.audibleRealization.phase!=core::AudibleHandoffPhase::NEW
       ||!audible.audibleRealization.realizationId) {
        out.status=C01ProjectionStatus::TRANSITION;return out;
    }
    if(*audible.audibleRealization.realizationId!=sidecar.realization_id) {
        out.status=C01ProjectionStatus::STALE_REALIZATION;return out;
    }
    if(sidecar.accepted_chain_revision!=accepted_chain_revision) {
        out.status=C01ProjectionStatus::STALE_REVISION;return out;
    }
    if(sidecar.frame_domain!=audible_frame_domain) {
        out.status=C01ProjectionStatus::FRAME_DOMAIN_MISMATCH;return out;
    }
    if(sidecar.sample_rate_hz!=audible_sample_rate_hz) {
        out.status=C01ProjectionStatus::SAMPLE_RATE_MISMATCH;return out;
    }
    if(!collector.finished() || audible.position.value()<0) {
        out.status=C01ProjectionStatus::HISTORY_INCOMPLETE;return out;
    }
    const auto frame=static_cast<std::uint64_t>(audible.position.value());
    out.history=collector.visible_window(
        frame,static_cast<std::uint64_t>(audible_sample_rate_hz)*5U);
    if(out.history.empty()) {
        out.status=C01ProjectionStatus::HISTORY_INCOMPLETE;return out;
    }
    out.status=audible.state==core::PlaybackState::PAUSED
        ?C01ProjectionStatus::PAUSED:C01ProjectionStatus::ACTIVE;
    out.realization_id=sidecar.realization_id;
    out.audible_frame=frame;
    out.sample_rate_hz=audible_sample_rate_hz;
    return out;
}
} // namespace rgsml::research::dynamic_ui
