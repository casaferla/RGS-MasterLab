#pragma once
// L3-M11 frozen independent S02/S05/S06 raw Stereo SAFETY only.
// No artistic mass, no Fisher-Z substitution, no tolerance beyond
// original explicit 0.02 raw negative-correlation/occupancy policies.
// This does NOT supersede the full endpoint/TP/M11/M13 gates.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rgsml::app::l3_automatch::m11_frozen {
struct NativeStereoRawSafety final {
    // Source and candidate are independently measured on current
    // original canonical M19/M20/Phase6 scope, not from GUI proxies.
    bool exact_current_source_candidate_and_canonical_safety_revision{};
    bool source_and_candidate_stereo{};
    bool no_forbidden_decorrelation_or_haas{};
    std::optional<double> prepared_correlation_p10_raw;
    std::optional<double> candidate_correlation_p10_raw;
    std::optional<double> prepared_negative_correlation_occupancy;
    std::optional<double> candidate_negative_correlation_occupancy;
    std::array<std::optional<double>,3> prepared_low_end_width;
    std::array<std::optional<double>,3> candidate_low_end_width;
    std::array<std::optional<double>,3> prepared_low_end_rho;
    std::array<std::optional<double>,3> candidate_low_end_rho;
    std::array<bool,3> prepared_low_end_exact_zero_side{};
};
enum class NativeStereoSafetyReason:std::uint8_t {
    pass,missing_or_stale_safety_evidence,nonfinite_or_invalid,
    artificial_decorrelation_disallowed,global_p10_regressed,
    negative_occupancy_regressed,forbidden_low_end_widening,
    low_end_rho_regressed
};
struct NativeStereoSafetyResult final {
    NativeStereoSafetyReason reason{
        NativeStereoSafetyReason::missing_or_stale_safety_evidence};
    std::optional<std::size_t> low_end_cutoff_ordinal;
    [[nodiscard]] constexpr bool passes_hard_gate() const noexcept {
        return reason==NativeStereoSafetyReason::pass;
    }
    [[nodiscard]] constexpr bool authorizes_export()
        const noexcept{return false;}
};
[[nodiscard]] inline NativeStereoSafetyResult
verify_m11_native_stereo_hard_safety(const NativeStereoRawSafety& e)
{
    if(!e.exact_current_source_candidate_and_canonical_safety_revision||
       !e.source_and_candidate_stereo)
        return {};
    if(!e.no_forbidden_decorrelation_or_haas)
        return {NativeStereoSafetyReason::
            artificial_decorrelation_disallowed,{}};
    const auto valid_rho=[](std::optional<double> x) {
        return x&&std::isfinite(*x)&&*x>= -1.0&&*x<=1.0;
    };
    const auto valid_occupancy=[](std::optional<double> x) {
        return x&&std::isfinite(*x)&&*x>=0.0&&*x<=1.0;
    };
    if(!valid_rho(e.prepared_correlation_p10_raw)||
       !valid_rho(e.candidate_correlation_p10_raw)||
       !valid_occupancy(e.prepared_negative_correlation_occupancy)||
       !valid_occupancy(e.candidate_negative_correlation_occupancy))
        return {NativeStereoSafetyReason::nonfinite_or_invalid,{}};
    constexpr double raw_slack=0.02;
    const double p=*e.prepared_correlation_p10_raw;
    const double c=*e.candidate_correlation_p10_raw;
    if((p>=0.0&&c<0.0)||
       (p<0.0&&c<p-raw_slack))
        return {NativeStereoSafetyReason::global_p10_regressed,{}};
    if(*e.candidate_negative_correlation_occupancy>
       *e.prepared_negative_correlation_occupancy+raw_slack)
        return {NativeStereoSafetyReason::
            negative_occupancy_regressed,{}};
    for(std::size_t i=0;i<3U;++i) {
        const auto pw=e.prepared_low_end_width[i];
        const auto cw=e.candidate_low_end_width[i];
        const auto pr=e.prepared_low_end_rho[i];
        const auto cr=e.candidate_low_end_rho[i];
        if(!pw||!cw||!std::isfinite(*pw)||!std::isfinite(*cw)||
           *pw<0.0||*cw<0.0||
           !valid_rho(pr)||!valid_rho(cr))
            return {NativeStereoSafetyReason::nonfinite_or_invalid,i};
        // No automatic low-end widening at ANY cutoff, including
        // effects caused indirectly by broadband EQ or M/S changes.
        if(*cw>*pw||
           (e.prepared_low_end_exact_zero_side[i]&&*cw!=0.0))
            return {NativeStereoSafetyReason::
                forbidden_low_end_widening,i};
        if(*cr<*pr-raw_slack)
            return {NativeStereoSafetyReason::low_end_rho_regressed,i};
    }
    return {NativeStereoSafetyReason::pass,{}};
}
} // namespace rgsml::app::l3_automatch::m11_frozen
