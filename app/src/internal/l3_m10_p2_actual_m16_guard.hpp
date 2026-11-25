#pragma once
// L3 M10 P2: independent physical M16 final True Peak hard guard.
// This uses the actual entire-resource PCM from the real P2 candidate,
// and the SAME already measured M19 scope. M17 WAV Export is not involved.
// The M11 Product owner supplies the exact authoritative safety ceiling;
// there is no invented "-1 dBTP" or tolerance and no GUI M21 surrogate.
#include "l3_m10_p2_full_native_evidence.hpp"
#include "l3_m10_native_prediction_snapshot.hpp"
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <cmath>
#include <cstdint>

namespace rgsml::app::l3_automatch::m10_frozen {
enum class P2ActualM16SafetyReason:std::uint8_t {
    within_exact_m11_ceiling,unbound_or_invalid_source,
    invalid_independent_ceiling,exceeded
};
struct P2ActualM16SafetyResult final {
    P2ActualM16SafetyReason reason{
        P2ActualM16SafetyReason::unbound_or_invalid_source};
    [[nodiscard]] constexpr bool passes()const noexcept {
        return reason==P2ActualM16SafetyReason::within_exact_m11_ceiling;
    }
    [[nodiscard]] constexpr bool authorizes_m12_or_m13()const noexcept {
        return false;
    }
};
[[nodiscard]] inline P2ActualM16SafetyResult
check_actual_p2_m16_linear_peak(
    double genuinely_measured_m16_linear_peak,
    double independent_current_m11_ceiling_db_tp) noexcept
{
    if(!std::isfinite(genuinely_measured_m16_linear_peak)||
       genuinely_measured_m16_linear_peak<0.0)
        return {P2ActualM16SafetyReason::unbound_or_invalid_source};
    if(!std::isfinite(independent_current_m11_ceiling_db_tp)||
       independent_current_m11_ceiling_db_tp>0.0)
        return {P2ActualM16SafetyReason::invalid_independent_ceiling};
    const double allowed=std::pow(
        10.0,independent_current_m11_ceiling_db_tp/20.0);
    if(!std::isfinite(allowed)||allowed<=0.0)
        return {P2ActualM16SafetyReason::invalid_independent_ceiling};
    return {genuinely_measured_m16_linear_peak<=allowed
        ?P2ActualM16SafetyReason::within_exact_m11_ceiling
        :P2ActualM16SafetyReason::exceeded};
}
[[nodiscard]] inline bool native_prediction_passes_current_m16(
    const NativePredictionSnapshot& prediction,double ceiling) noexcept
{
    return prediction.independent_full_m16_linear_peak&&
        prediction.independent_m11_ceiling_db_tp&&
        *prediction.independent_m11_ceiling_db_tp==ceiling&&
        check_actual_p2_m16_linear_peak(*prediction.independent_full_m16_linear_peak,ceiling).passes();
}
[[nodiscard]] inline P2ActualM16SafetyResult
check_m10_p2_native_actual_m16(
    const M10P2FullNativeEvidence& native,
    double independent_current_m11_ceiling_db_tp) noexcept
{
    const auto& observed=native.native.actual;
    const auto& pcm=observed.rendered.view();
    const auto rate=pcm.format().sample_rate().value();
    const auto channels=pcm.format().channel_count();
    const auto frames=pcm.frame_count().value();
    if(rate<=0||frames<=0||channels<1U||channels>2U||
       pcm.absolute_start_frame().value()!=0||
       pcm.timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE||
       native.m16_actual_p2_output.sample_rate_hz()!=rate||
       native.m16_actual_p2_output.channel_count()!=channels||
       native.m16_actual_p2_output.qualification_status()==
           analysis::TruePeakQualificationStatus::UNSUPPORTED||
       observed.levels.sample_rate_hz!=rate||
       observed.levels.channel_count!=channels||
       observed.levels.scope_begin_frame!=0||
       observed.levels.scope_frame_count!=frames||
       observed.tonal.sample_rate_hz!=rate||
       observed.tonal.channel_count!=channels||
       observed.tonal.scope_begin_frame!=0||
       observed.tonal.scope_frame_count!=frames)
        return {P2ActualM16SafetyReason::unbound_or_invalid_source};
    return check_actual_p2_m16_linear_peak(
        native.m16_actual_p2_output.global_linear_peak(),
        independent_current_m11_ceiling_db_tp);
}
} // namespace rgsml::app::l3_automatch::m10_frozen
