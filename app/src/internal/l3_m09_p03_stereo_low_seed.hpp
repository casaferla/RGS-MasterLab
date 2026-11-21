#pragma once
// L3-M09 P03 ACCEPTED_FROZEN: only the Stereo/Mono-Bass discrete
// alternatives and ideal low-lane analytical seed composition.
// No real crossover response, S05/S06 target attainment, no correlated
// safety / M11 fixed effects, plan or master publication are inferred.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rgsml::app::l3_automatch::m09_frozen {
enum class LowBassMode:std::uint8_t {off,lr12,lr24};
struct LowBassTopology final {
    LowBassMode mode{LowBassMode::off};
    std::uint16_t cutoff_hz{120}; // historical OFF documentary default
    double stored_width_percent{100.0}; // OFF non-effective default
    // The candidate consumer must independently check selected S05
    // intent, S06 constraints, M11 ownership and actual factory/codec.
    [[nodiscard]] constexpr bool active() const noexcept {
        return mode!=LowBassMode::off;
    }
};
inline constexpr std::array<LowBassTopology,7> kLowBassAlternatives{{
    {LowBassMode::off,120,100.0},
    {LowBassMode::lr12,80,100.0},
    {LowBassMode::lr12,120,100.0},
    {LowBassMode::lr12,200,100.0},
    {LowBassMode::lr24,80,100.0},
    {LowBassMode::lr24,120,100.0},
    {LowBassMode::lr24,200,100.0}
}};
[[nodiscard]] constexpr bool structural_low_alternatives_valid() noexcept {
    if(kLowBassAlternatives.size()!=7U||
       kLowBassAlternatives.front().mode!=LowBassMode::off)
        return false;
    for(std::size_t i=1;i<kLowBassAlternatives.size();++i) {
        if(!kLowBassAlternatives[i].active())return false;
        if(kLowBassAlternatives[i].cutoff_hz!=80 &&
           kLowBassAlternatives[i].cutoff_hz!=120 &&
           kLowBassAlternatives[i].cutoff_hz!=200)
            return false;
        for(std::size_t j=0;j<i;++j)
            if(kLowBassAlternatives[j].mode==kLowBassAlternatives[i].mode
               &&kLowBassAlternatives[j].cutoff_hz
                   ==kLowBassAlternatives[i].cutoff_hz)
                return false;
    }
    return true;
}
static_assert(structural_low_alternatives_valid());

enum class LowSeedReason:std::uint8_t {
    ready,limited_to_legal_range,one_channel_not_applicable,
    missing_exact_insertion_baseline,missing_bounded_gain_authority,
    observed_zero_side_cannot_widen,already_mono_satisfied,
    exact_global_side_mute_separate_branch,nonfinite_or_illegal_operand,
    numerical_positive_width_underflow
};
struct LowSeedEvidence final {
    bool exact_insertion_baseline_and_m11_ownership{};
    bool stereo_input{};
    bool observed_zero_side{};
    bool target_requires_positive_side{};
    bool both_observed_and_target_mono{};
    bool global_exact_side_mute{};
    // Caller must provide positive finite/log2-equivalent S05
    // coordinates, with exact endpoint provenance from M07/M08.
    double baseline_low_width_log2{};
    double optimization_target_low_width_log2{};
    double bounded_mid_gain_db{};
    double bounded_side_gain_db{};
    bool gains_frozen_schema_legal{};
};
struct LowWidthAnalyticalSeed final {
    std::optional<double> raw_percent;
    std::optional<double> legal_percent;
    LowSeedReason reason{LowSeedReason::missing_exact_insertion_baseline};
    [[nodiscard]] constexpr bool authorizes_stereo_safety_or_publication()
        const noexcept {return false;}
};
[[nodiscard]] inline LowWidthAnalyticalSeed derive_low_width_seed(
    const LowSeedEvidence& evidence,bool combined_broadband)
{
    if(!evidence.stereo_input)
        return {{},{},LowSeedReason::one_channel_not_applicable};
    if(!evidence.exact_insertion_baseline_and_m11_ownership)
        return {{},{},LowSeedReason::missing_exact_insertion_baseline};
    if(evidence.global_exact_side_mute)
        return {{},{},LowSeedReason::exact_global_side_mute_separate_branch};
    if(evidence.both_observed_and_target_mono)
        return {{},{},LowSeedReason::already_mono_satisfied};
    if(evidence.observed_zero_side&&evidence.target_requires_positive_side)
        return {{},{},LowSeedReason::observed_zero_side_cannot_widen};
    if(combined_broadband&&!evidence.gains_frozen_schema_legal)
        return {{},{},LowSeedReason::missing_bounded_gain_authority};
    if(!std::isfinite(evidence.baseline_low_width_log2)||
       !std::isfinite(evidence.optimization_target_low_width_log2)||
       (combined_broadband&&
        (!std::isfinite(evidence.bounded_mid_gain_db)||
         !std::isfinite(evidence.bounded_side_gain_db))))
        return {{},{},LowSeedReason::nonfinite_or_illegal_operand};
    const double target_delta=
        evidence.optimization_target_low_width_log2-
        evidence.baseline_low_width_log2;
    // Combined correction is expressed IN log2(Side/Mid) units;
    // bounded broadcaster gains MUST have separate schema authority.
    const double gain_log2=combined_broadband?
        (evidence.bounded_side_gain_db-evidence.bounded_mid_gain_db)/
        (20.0*std::log10(2.0)):0.0;
    const double exponent=target_delta-gain_log2;
    const double raw_percent=100.0*std::exp2(exponent);
    if(!std::isfinite(target_delta)||!std::isfinite(gain_log2)
       ||!std::isfinite(exponent)||!std::isfinite(raw_percent)
       ||raw_percent<0.0)
        return {{},{},LowSeedReason::nonfinite_or_illegal_operand};
    // A positive partial Side target rounding numerically to zero
    // must never be reinterpreted as an exact mono/side-mute endpoint.
    if(raw_percent==0.0)
        return {{},{},LowSeedReason::numerical_positive_width_underflow};
    if(raw_percent>100.0)
        return {raw_percent,100.0,LowSeedReason::limited_to_legal_range};
    return {raw_percent,raw_percent,LowSeedReason::ready};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
