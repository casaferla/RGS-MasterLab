#pragma once
// Native true M20 raw safety facts + actual S05/S06 per-cutoff coherent
// source -> original frozen M11 hard veto. Not a new UI M21
// safety proxy; no Fisher transform is passed to the raw safety gate.
#include "l3_m06_m20_coherent_stereo_band_sink.hpp"
#include "l3_m11_native_stereo_hard_safety.hpp"
#include "l3_m05m06_real_analysis_port.hpp"
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace rgsml::app::l3_automatch::m05m06_real {
struct NativeStereoSafetyPair final {
    SourceIdentity prepared_owner,candidate_owner;
    analysis::TonalStereoAnalysisResult prepared_m20,candidate_m20;
    StereoCoherentNativeResult prepared_low,candidate_low;
    bool exact_current_phase6_frame_scopes_and_owners{};
    bool exact_native_low_end_frame_gate_equal_to_m19{};
    bool no_forbidden_haas_or_decorrelation_in_candidate{};
};
enum class StereoSafetyBridgeFailure:std::uint8_t {
    none,missing_or_stale_owner,missing_raw_local_corr,
    invalid_coherent_low_end
};
struct StereoSafetyBridgeResult final {
    std::optional<m11_frozen::NativeStereoRawSafety> raw_evidence;
    StereoSafetyBridgeFailure failure{
        StereoSafetyBridgeFailure::missing_or_stale_owner};
    [[nodiscard]] constexpr bool authorizes_release()
        const noexcept{return false;}
};
[[nodiscard]] inline std::optional<double>
measured_negative_occupancy_from_real_m20(
    const analysis::TonalStereoAnalysisResult& input) {
    using State=analysis::TonalStereoMetricState;
    std::uint64_t denominator=0U,numerator=0U;
    for(const auto& window:input.correlation_grid100) {
        if(window.correlation.state!=State::AVAILABLE)continue;
        const auto raw=window.correlation.value;
        if(!raw||!std::isfinite(*raw)||*raw< -1.0||*raw>1.0)
            return std::nullopt;
        ++denominator;
        if(*raw<0.0)++numerator;
    }
    if(!denominator)return std::nullopt;
    return static_cast<double>(numerator)/
           static_cast<double>(denominator);
}
[[nodiscard]] inline StereoSafetyBridgeResult
bind_actual_m20_stereo_safety_to_m11(
    const NativeStereoSafetyPair& input)
{
    const auto& p=input.prepared_m20;
    const auto& c=input.candidate_m20;
    const auto& a=input.prepared_owner;
    const auto& b=input.candidate_owner;
    const bool matching_rates=p.sample_rate_hz>0&&
        c.sample_rate_hz>0; // Prepared and candidate may be different Fs.
    if(!input.exact_current_phase6_frame_scopes_and_owners||
       !a.full_track_owner_confirmed||!b.full_track_owner_confirmed||
       a.resource_identity.empty()||b.resource_identity.empty()||
       a.resource_identity==b.resource_identity||
       !a.generation||!b.generation||!matching_rates||
       p.scope_begin_frame!=0||c.scope_begin_frame!=0||
       p.scope_frame_count<=0||c.scope_frame_count<=0||
       p.channel_count!=2U||c.channel_count!=2U)
        return {};
    if(!input.exact_native_low_end_frame_gate_equal_to_m19||
       !input.prepared_low.numerical_source_valid||
       !input.candidate_low.numerical_source_valid||
       !input.prepared_low.low_end_gate_current_and_identical||
       !input.candidate_low.low_end_gate_current_and_identical)
        return {{},StereoSafetyBridgeFailure::invalid_coherent_low_end};
    using State=analysis::TonalStereoMetricState;
    if(p.correlation_p10.state!=State::AVAILABLE||
       c.correlation_p10.state!=State::AVAILABLE||
       !p.correlation_p10.value||!c.correlation_p10.value)
        return {{},StereoSafetyBridgeFailure::missing_raw_local_corr};
    const auto po=measured_negative_occupancy_from_real_m20(p);
    const auto co=measured_negative_occupancy_from_real_m20(c);
    if(!po||!co)
        return {{},StereoSafetyBridgeFailure::missing_raw_local_corr};
    m11_frozen::NativeStereoRawSafety safety;
    safety.exact_current_source_candidate_and_canonical_safety_revision=true;
    safety.source_and_candidate_stereo=true;
    safety.no_forbidden_decorrelation_or_haas=
        input.no_forbidden_haas_or_decorrelation_in_candidate;
    safety.prepared_correlation_p10_raw=p.correlation_p10.value;
    safety.candidate_correlation_p10_raw=c.correlation_p10.value;
    safety.prepared_negative_correlation_occupancy=po;
    safety.candidate_negative_correlation_occupancy=co;
    for(std::size_t i=0;i<3U;++i) {
        const auto& pl=input.prepared_low;
        const auto& cl=input.candidate_low;
        safety.prepared_low_end_width[i]=
            pl.low_end_width[i].raw_ratio_or_rho;
        safety.candidate_low_end_width[i]=
            cl.low_end_width[i].raw_ratio_or_rho;
        safety.prepared_low_end_rho[i]=
            pl.low_end_correlation[i].raw_ratio_or_rho;
        safety.candidate_low_end_rho[i]=
            cl.low_end_correlation[i].raw_ratio_or_rho;
        safety.prepared_low_end_exact_zero_side[i]=
            pl.low_end_width[i].actual_zero_side_endpoint;
    }
    return {safety,StereoSafetyBridgeFailure::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
