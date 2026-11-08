// M22 strictly private REAL-MASTER-to-output-plan binding.
// Authoritative facts come ONLY from actual complete Manual -> original Q0
// -> active M17 + M16 full checked PCM. The old metadata-only planner
// cannot imply that an SRC ratio or nominal frame length was rendered.
// This read-only bridge never authorizes M18 export, UI, Phase-6 or FQ.
#pragma once
#include "m22_full_chain_witness_candidate.hpp"
#include "m22_checked_output_clock_candidate.hpp"
#include "m22_checked_output_pcm_candidate.hpp"
#include "m22_final_src_frozen_pin_candidate.hpp"
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <cstdint>
#include <cstddef>
#include <numeric>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::m22_private {

enum class RealFullOutputPlanFailure : std::uint8_t {
    NONE, NO_VERIFIED_NATIVE_FULL_M17, INVALID_NATIVE_CLOCK,
    MISSING_OR_SUBSTITUTED_ORIGINAL_FIR, INVALID_REAL_OUTPUT_PCM,
    INVALID_ABSOLUTE_OUTPUT_WINDOW
};

struct RealFullOutputPlanCandidate final {
    FullChainWitness native;
    CheckedOutputClockCandidate clock;
    FrozenFirSelection original_q0;
    OutputWindowCandidate output;
    // This is only a native *observation*. The caller must separately own
    // the immutable M17 stage and revalidate the actual M18 Product epoch.
    [[nodiscard]] bool authorizes_release() const noexcept {return false;}
};

[[nodiscard]] inline std::optional<RealFullOutputPlanCandidate>
observe_real_full_output_plan_candidate(
    const rgsml::render::TruePeakLimiterOwnedSourceCandidate& realized,
    Mode requested_mode,Window output_window,
    RealFullOutputPlanFailure& failure)
{
    failure=RealFullOutputPlanFailure::NONE;
    const auto deny=[&](RealFullOutputPlanFailure code)
        ->std::optional<RealFullOutputPlanCandidate> {
        failure=code;
        return std::nullopt;
    };
    FullChainWitnessFailure source_reason{};
    auto genuine=observe_real_complete_chain_candidate(
        realized,source_reason);
    if(!genuine)
        return deny(RealFullOutputPlanFailure::NO_VERIFIED_NATIVE_FULL_M17);
    // Existing native M17 PCM, NOT source_frames * output_rate/source_rate.
    const auto source_hz=static_cast<std::uint32_t>(genuine->manual_source_hz);
    const auto master_hz=static_cast<std::uint32_t>(genuine->final_master_hz);
    const auto common=std::gcd(source_hz,master_hz);
    if(common==0)
        return deny(RealFullOutputPlanFailure::INVALID_NATIVE_CLOCK);
    std::string actual_original_asset;
    if(genuine->used_original_q0)
        actual_original_asset="RGSMSR1_L"
            +std::to_string(master_hz/common)+"_M"
            +std::to_string(source_hz/common)+".bin";
    // Ephemeral checked-owner label used only by the legacy metadata
    // preflight; it is NOT a cryptographic ID, persisted key or Product
    // authorization. The only trusted provenance is genuine + M18 epoch.
    const auto local_observation="native-checked-m17:"
        +std::to_string(reinterpret_cast<std::uintptr_t>(&realized));
    const OutputClockDeclaration observed{
        source_hz,master_hz,
        static_cast<std::uint64_t>(genuine->complete_source_frames),
        static_cast<std::uint64_t>(genuine->actual_master_frames),
        local_observation,std::move(actual_original_asset)};
    OutputClockFailure clock_failure{};
    auto clock=CheckedOutputClockCandidate::freeze(observed,clock_failure);
    if(!clock)
        return deny(RealFullOutputPlanFailure::INVALID_NATIVE_CLOCK);
    FrozenFirFailure pin_failure{};
    auto pin=bind_frozen_original_q0_identity_candidate(
        *clock,pin_failure);
    if(!pin ||pin->identity_bypass==genuine->used_original_q0
       ||(genuine->used_original_q0
          &&pin->pinned_sha256!=genuine->frozen_original_q0_sha256)
       ||(!genuine->used_original_q0
          &&!genuine->frozen_original_q0_sha256.empty()))
        return deny(RealFullOutputPlanFailure::MISSING_OR_SUBSTITUTED_ORIGINAL_FIR);
    const auto full_pcm=realized.terminal.checked.terminal.compensated_pcm.view();
    if(check_real_full_output_pcm_candidate(*clock,full_pcm)
        !=PcmClockFailure::NONE
       ||full_pcm.format().channel_count()!=genuine->channels
       ||full_pcm.format().channel_layout()
            !=realized.manual_owner->view().format().channel_layout())
        return deny(RealFullOutputPlanFailure::INVALID_REAL_OUTPUT_PCM);
    auto planned=clock->plan_window(
        requested_mode,output_window,clock_failure);
    if(!planned)
        return deny(RealFullOutputPlanFailure::INVALID_ABSOLUTE_OUTPUT_WINDOW);
    if(planned->required_source_true_eos
            !=static_cast<std::uint64_t>(genuine->complete_source_frames)
       ||planned->required_output_true_eos
            !=static_cast<std::uint64_t>(genuine->actual_master_frames)
       ||planned->replay_source_first!=0
       ||planned->absolute_dither_first!=output_window.first)
        return deny(RealFullOutputPlanFailure::INVALID_ABSOLUTE_OUTPUT_WINDOW);
    return RealFullOutputPlanCandidate{
        std::move(*genuine),std::move(*clock),
        std::move(*pin),*planned};
}

} // namespace rgsml::m22_private
