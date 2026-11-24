#pragma once
// L3-M06 D02 LRA: real unrounded native M19 ST 3s/Grid100 to the
// frozen EBU -70 LUFS / relative -20 LU gating and DISCRETE nearest
// percentile, not Type7. File-based trailing 1.5s / active duration
// are independently Phase6-owned; no invented virtual observations.
#include "l3_m05m06_real_analysis_port.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
enum class LraNativeReason:std::uint8_t {
    ready,computed_but_reference_grade_unattested,
    insufficient_active_duration_or_retained,
    missing_native_short_term,invalid_grid_or_numeric,
    no_absolute_or_relative_retained
};
struct LraNativeObservation final {
    std::optional<double> lra_lu;
    std::optional<double> absolute_gated_mean_lufs;
    std::optional<double> relative_gate_lufs;
    std::uint64_t absolute_gated_windows{};
    std::uint64_t relative_retained_windows{};
    LraNativeReason reason{LraNativeReason::missing_native_short_term};
    std::string exact_gate_population_identity{};
    [[nodiscard]] bool is_reference_grade() const noexcept {
        return reason==LraNativeReason::ready&&lra_lu.has_value();
    }
    [[nodiscard]] constexpr bool authorizes_m10() const noexcept {
        return false;
    }
};
[[nodiscard]] inline std::optional<double> discrete_ebu_percentile(
    const std::vector<double>& sorted,double p) {
    if(sorted.empty()||!std::isfinite(p)||p<0.0||p>1.0)
        return std::nullopt;
    for(std::size_t i=0;i<sorted.size();++i)
        if(!std::isfinite(sorted[i])||
           (i>0U&&sorted[i]<sorted[i-1U]))
            return std::nullopt;
    const auto ix=static_cast<std::size_t>(
        std::floor(static_cast<double>(sorted.size()-1U)*p+0.5));
    return sorted[ix];
}
// Exact active frames and file-based trailing policy are verified by
// the independent Phase6 producer, not inferred from file duration.
// On an unattested policy, a computed scalar is diagnostic only.
[[nodiscard]] inline LraNativeObservation observe_lra_from_real_m19(
    const SourceReadings& src,std::int64_t exact_active_frames,
    bool exact_ebu_file_based_trailing_policy_attested)
{
    LraNativeObservation out;
    if(!src.owner.full_track_owner_confirmed||
       src.owner.resource_identity.empty()||!src.owner.generation||
       src.sample_rate_hz<=0||src.scope_frame_count<=0||
       src.short_term_complete_windows==0U||
       src.short_term_complete_windows!=src.short_term_3s_grid100.size()||
       exact_active_frames<0||exact_active_frames>src.scope_frame_count)
        return out;
    if(src.sample_rate_hz%10!=0)
        return out;
    std::vector<double> absolute_gate;
    absolute_gate.reserve(src.short_term_3s_grid100.size());
    std::uint64_t valid_native=0U;
    for(std::size_t i=0;i<src.short_term_3s_grid100.size();++i) {
        const auto& w=src.short_term_3s_grid100[i];
        if(w.absolute_start_frame!=
                static_cast<std::int64_t>(i)*src.sample_rate_hz/10||
           w.absolute_start_frame+3*src.sample_rate_hz>
                src.scope_frame_count) {
            out.reason=LraNativeReason::invalid_grid_or_numeric;
            return out;
        }
        if(!w.lufs.ready())continue;
        const double value=*w.lufs.unrounded;
        if(!std::isfinite(value)) {
            out.reason=LraNativeReason::invalid_grid_or_numeric;
            return out;
        }
        ++valid_native;
        if(value> -70.0)absolute_gate.push_back(value);
    }
    if(valid_native!=src.short_term_valid_windows)
        return out;
    out.absolute_gated_windows=absolute_gate.size();
    if(absolute_gate.empty()) {
        out.reason=LraNativeReason::no_absolute_or_relative_retained;
        return out;
    }
    // Mean of LINEAR energies at the absolute gate, with max-offset
    // scaling to avoid huge temporary 10^(lufs/10) overflow.
    const double maximum=*std::max_element(
        absolute_gate.begin(),absolute_gate.end());
    double relative_linear_sum=0.0;
    for(const auto v:absolute_gate) {
        relative_linear_sum+=std::pow(10.0,(v-maximum)/10.0);
    }
    // Volatile checked cardinality avoids MSVC C4723 speculation while
    // preserving the original relative-gate mean-energy arithmetic.
    const volatile double positive_absolute_count=
        static_cast<double>(absolute_gate.size());
    const double average=relative_linear_sum/positive_absolute_count;
    if(!std::isfinite(average)||average<=0.0) {
        out.reason=LraNativeReason::invalid_grid_or_numeric;
        return out;
    }
    const double mean_loudness=maximum+10.0*std::log10(average);
    const double relative_gate=mean_loudness-20.0;
    if(!std::isfinite(mean_loudness)||!std::isfinite(relative_gate)) {
        out.reason=LraNativeReason::invalid_grid_or_numeric;
        return out;
    }
    out.absolute_gated_mean_lufs=mean_loudness;
    out.relative_gate_lufs=relative_gate;
    std::vector<double> retained;
    retained.reserve(absolute_gate.size());
    for(const auto v:absolute_gate)
        if(v>relative_gate)retained.push_back(v);
    out.relative_retained_windows=retained.size();
    NativeMembershipIdentity membership{"D02-EBU-discrete-gates/1"};
    membership.append(static_cast<std::uint64_t>(src.sample_rate_hz));
    membership.append(static_cast<std::uint64_t>(src.scope_frame_count));
    for(const auto& window:src.short_term_3s_grid100) {
        membership.append(static_cast<std::uint64_t>(window.absolute_start_frame));
        const bool valid=window.lufs.ready();
        const bool absolute=valid&&*window.lufs.unrounded> -70.0;
        const bool relative=absolute&&*window.lufs.unrounded>relative_gate;
        membership.append((valid?1U:0U)|(absolute?2U:0U)|(relative?4U:0U));
    }
    out.exact_gate_population_identity=membership.finish();
    if(retained.empty()) {
        out.reason=LraNativeReason::no_absolute_or_relative_retained;
        return out;
    }
    std::sort(retained.begin(),retained.end());
    const auto q10=discrete_ebu_percentile(retained,0.10);
    const auto q95=discrete_ebu_percentile(retained,0.95);
    if(!q10||!q95) {
        out.reason=LraNativeReason::invalid_grid_or_numeric;
        return out;
    }
    out.lra_lu=*q95-*q10;
    if(!std::isfinite(*out.lra_lu)) {
        out.lra_lu.reset();
        out.reason=LraNativeReason::invalid_grid_or_numeric;
        return out;
    }
    // Both ≥60s active and ≥100 EBU retained are required before
    // M06 D02 may become reference-grade; scope length alone is not.
    if(exact_active_frames<60*src.sample_rate_hz||
       out.relative_retained_windows<100U) {
        out.reason=LraNativeReason::
            insufficient_active_duration_or_retained;
        return out;
    }
    out.reason=exact_ebu_file_based_trailing_policy_attested
        ?LraNativeReason::ready
        :LraNativeReason::computed_but_reference_grade_unattested;
    return out;
}
} // namespace rgsml::app::l3_automatch::m05m06_real
