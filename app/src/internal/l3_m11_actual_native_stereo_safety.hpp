#pragma once
// Bind the existing M11 raw safety predicate to the real independently
// measured PREPARED and candidate M20/native coherent stereo evidence.
#include "l3_m11_native_stereo_hard_safety.hpp"
#include "l3_m05m06_real_analysis_port.hpp"
#include "l3_m06_m20_coherent_stereo_band_sink.hpp"

namespace rgsml::app::l3_automatch::m11_frozen {
[[nodiscard]] inline NativeStereoRawSafety bind_actual_native_stereo_safety(
    const m05m06_real::SourceReadings& prepared,
    const m05m06_real::StereoCoherentNativeResult& prepared_coherent,
    const m05m06_real::SourceReadings& candidate,
    const m05m06_real::StereoCoherentNativeResult& candidate_coherent,
    bool exact_current_scope_and_safety_policy,
    bool no_forbidden_decorrelation_or_haas)
{
    NativeStereoRawSafety out;
    out.source_and_candidate_stereo=prepared.channels==2U&&candidate.channels==2U;
    out.no_forbidden_decorrelation_or_haas=no_forbidden_decorrelation_or_haas;
    if(!exact_current_scope_and_safety_policy||
       !out.source_and_candidate_stereo||
       !prepared.owner.full_track_owner_confirmed||
       !candidate.owner.full_track_owner_confirmed||
       !prepared.owner.generation||!candidate.owner.generation||
       prepared.owner.resource_identity.empty()||
       candidate.owner.resource_identity.empty()||
       prepared.owner.resource_identity==candidate.owner.resource_identity||
       prepared.sample_rate_hz<=0||
       prepared.sample_rate_hz!=candidate.sample_rate_hz||
       prepared.scope_frame_count<=0||
       prepared.scope_frame_count!=candidate.scope_frame_count||
       !prepared_coherent.numerical_source_valid||
       !candidate_coherent.numerical_source_valid||
       !prepared_coherent.low_end_gate_current_and_identical||
       !candidate_coherent.low_end_gate_current_and_identical)
        return out;
    out.exact_current_source_candidate_and_canonical_safety_revision=true;
    if(prepared.correlation_p10.ready())
        out.prepared_correlation_p10_raw=prepared.correlation_p10.unrounded;
    if(candidate.correlation_p10.ready())
        out.candidate_correlation_p10_raw=candidate.correlation_p10.unrounded;
    out.prepared_negative_correlation_occupancy=prepared.negative_correlation_occupancy;
    out.candidate_negative_correlation_occupancy=candidate.negative_correlation_occupancy;
    for(std::size_t i=0;i<3U;++i) {
        const auto& pw=prepared_coherent.low_end_width[i];
        const auto& cw=candidate_coherent.low_end_width[i];
        const auto& pr=prepared_coherent.low_end_correlation[i];
        const auto& cr=candidate_coherent.low_end_correlation[i];
        if(pw.transformed_z)out.prepared_low_end_width[i]=pw.raw_ratio_or_rho;
        if(cw.transformed_z)out.candidate_low_end_width[i]=cw.raw_ratio_or_rho;
        if(pr.transformed_z)out.prepared_low_end_rho[i]=pr.raw_ratio_or_rho;
        if(cr.transformed_z)out.candidate_low_end_rho[i]=cr.raw_ratio_or_rho;
        out.prepared_low_end_exact_zero_side[i]=pw.actual_zero_side_endpoint;
    }
    return out;
}
} // namespace rgsml::app::l3_automatch::m11_frozen
