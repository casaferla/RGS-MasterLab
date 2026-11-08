// L1-M22 REAL complete-source/manual/Q0/M17/M16 integrity observation.
// NONRELEASE: never creates an authoritative Product identity, preview,
// export ticket, registry, second SRC, PCM, Q1-Q6 or FeatureStatus.
#pragma once
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace rgsml::m22_private {
enum class FullChainWitnessFailure : std::uint8_t {
    NONE,
    NO_MANUAL_OWNER,
    PARTIAL_OR_STALE_MANUAL,
    INVALID_SOURCE_RATE_OR_CHANNELS,
    INVALID_OUTPUT_DOMAIN_OR_CLOCK,
    INVALID_M17_OR_M16,
    INCORRECT_FROZEN_SRC_PROVENANCE
};

struct FullChainWitness final {
    std::int64_t manual_source_hz{};
    std::int64_t final_master_hz{};
    std::int64_t complete_source_frames{};
    std::int64_t actual_master_frames{};
    std::size_t channels{};
    std::uint64_t manual_chain_revision{};
    std::uint64_t terminal_chain_revision{};
    bool used_original_q0{};
    std::string frozen_original_q0_sha256;
    [[nodiscard]] bool authorizes_release() const noexcept { return false; }
};

// A returned witness is strictly a *value copy* of verified native-stage
// observations. Ownership and Product epoch are independently required.
// In particular the actual OUTPUT frame count is READ from M17 PCM, never
// inferred from source/target rational arithmetic or a UI rate selection.
[[nodiscard]] inline std::optional<FullChainWitness>
observe_real_complete_chain_candidate(
    const rgsml::render::TruePeakLimiterOwnedSourceCandidate& realized,
    FullChainWitnessFailure& failure)
{
    using rgsml::audio::FrameDomainId;
    failure=FullChainWitnessFailure::NONE;
    const auto deny=[&](FullChainWitnessFailure value)
        ->std::optional<FullChainWitness> {
        failure=value;
        return std::nullopt;
    };
    if(!realized.manual_owner)
        return deny(FullChainWitnessFailure::NO_MANUAL_OWNER);
    const auto source=realized.manual_owner->view();
    if(realized.manual_owner->frame_domain_id()
            !=FrameDomainId::SOURCE_PROCESSING_RATE
       ||source.timebase().frame_domain_id()
            !=FrameDomainId::SOURCE_PROCESSING_RATE
       ||source.absolute_start_frame().value()!=0
       ||source.frame_count().value()<=0
       ||realized.manual_owner->render_window()!=source.absolute_range()
       ||realized.programme_frames!=source.frame_count()
       ||realized.manual_chain_revision
            !=realized.manual_owner->chain_revision())
        return deny(FullChainWitnessFailure::PARTIAL_OR_STALE_MANUAL);
    const auto sourceHz=source.format().sample_rate().value();
    const auto outputHz=realized.output_rate.value();
    constexpr auto accepted=[](std::int64_t hz) {
        return hz==44100||hz==48000||hz==88200
            ||hz==96000||hz==176400||hz==192000;
    };
    const auto channels=source.format().channel_count();
    if(!accepted(sourceHz)||!accepted(outputHz)
       ||(channels!=1U&&channels!=2U))
        return deny(FullChainWitnessFailure::INVALID_SOURCE_RATE_OR_CHANNELS);
    const auto& checked=realized.terminal.checked;
    const auto result=checked.terminal.compensated_pcm.view();
    if(result.timebase().frame_domain_id()!=FrameDomainId::OUTPUT_RATE
       ||result.absolute_start_frame().value()!=0
       ||result.frame_count().value()<=0
       ||result.format().sample_rate().value()!=outputHz
       ||result.format().channel_layout()!=source.format().channel_layout())
        return deny(FullChainWitnessFailure::INVALID_OUTPUT_DOMAIN_OR_CLOCK);
    if(checked.terminal.bypassed
       ||!checked.final_check
       ||checked.final_check->qualification_status
          ==rgsml::analysis::TruePeakQualificationStatus::UNSUPPORTED
       ||realized.terminal.terminal_chain_revision==0
       ||checked.terminal.applied_gain_frames.size()
            !=static_cast<std::size_t>(result.frame_count().value()))
        return deny(FullChainWitnessFailure::INVALID_M17_OR_M16);
    const bool crossRate=sourceHz!=outputHz;
    std::string pinned;
    if(crossRate) {
        const auto divisor=std::gcd(sourceHz,outputHz);
        if(divisor<=0)
            return deny(FullChainWitnessFailure::INCORRECT_FROZEN_SRC_PROVENANCE);
        const auto* pin=rgsml::audio::internal::phase11_frozen_original_fir_pin(
            static_cast<std::uint32_t>(outputHz/divisor),
            static_cast<std::uint32_t>(sourceHz/divisor));
        if(!pin||!realized.used_candidate_src
           ||realized.src_kernel_sha256!=std::string_view{pin->sha256})
            return deny(FullChainWitnessFailure::INCORRECT_FROZEN_SRC_PROVENANCE);
        pinned=pin->sha256;
    } else if(realized.used_candidate_src
              ||!realized.src_kernel_sha256.empty())
        return deny(FullChainWitnessFailure::INCORRECT_FROZEN_SRC_PROVENANCE);
    return FullChainWitness{
        sourceHz,outputHz,source.frame_count().value(),
        result.frame_count().value(),channels,
        realized.manual_chain_revision,
        realized.terminal.terminal_chain_revision,crossRate,
        std::move(pinned)};
}
} // namespace rgsml::m22_private
