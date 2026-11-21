#pragma once
// L3 read-only in-memory SCALAR feasibility probe, NOT the frozen M05-M10
// 27-feature solver and NOT a Product M21 publication. This reuses the
// exact M19/M20 -> M21 cell converter on real worker measurements instead
// of minting a fraudulent "current PROCESSED" Snapshot for each evaluation.
#include "l3_product_offline_trial_job.hpp"
#include "l3_automatch_offline_search.hpp"
#include "m21_real_product_metrics_port.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace rgsml::app::l3_automatch {
struct PrivateScalarProbe final {
    double normalized_weighted_squared_error{};
    std::size_t evaluated_metric_count{};
    std::int64_t candidate_frames{};
    std::uint32_t candidate_rate{};
    // No M21 generation, no Phase-6/current/verified flag on this carrier.
};

[[nodiscard]] inline core::Result<PrivateScalarProbe>
probe_unpublished_candidate(
    const OfflineProductCandidateMeasurement& candidate,
    const m21_private::Snapshot& reference,
    std::span<const Objective> selected)
{
    using R=core::Result<PrivateScalarProbe>;
    const auto fail=[](const char* why) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidArgument,why});
    };
    const auto& l=candidate.levels;
    const auto& s=candidate.tonal;
    const auto& view=candidate.rendered.view();
    if(selected.empty()||selected.size()>9
       ||reference.column!=m21_private::Column::gold
       ||reference.lifecycle!=m21_private::Lifecycle::current
       ||reference.generation_token==0||reference.resource_identity.empty()
       ||reference.analysis_scope_label!="Full Track"
       ||reference.absolute_scope_begin!=0||reference.scope_frames==0
       ||!m21_private::supported_private_rate(reference.sample_rate_hz)
       ||l.scope_begin_frame!=0||l.scope_frame_count<=0
       ||l.scope_begin_frame!=s.scope_begin_frame
       ||l.scope_frame_count!=s.scope_frame_count
       ||l.sample_rate_hz!=s.sample_rate_hz
       ||l.channel_count!=s.channel_count
       ||l.frame_domain!=s.frame_domain
       ||view.absolute_start_frame().value()!=0
       ||view.frame_count().value()!=l.scope_frame_count
       ||view.format().sample_rate().value()!=l.sample_rate_hz
       ||view.format().channel_count()!=l.channel_count
       ||view.timebase().frame_domain_id()!=l.frame_domain
       ||l.sample_rate_hz<=0
       ||l.sample_rate_hz>std::numeric_limits<std::uint32_t>::max()
       ||!m21_private::supported_private_rate(
             static_cast<std::uint32_t>(l.sample_rate_hz)))
        return fail("L3_PRIVATE_PROBE_INVALID_REAL_FULL_SCOPES");

    // The M21 semantic table is reused directly; do not implement any
    // alternative True Peak, tonal shape or sample-rate alignment.
    const std::array<m21_private::Cell,9> real{{
        m21_private::basic_level_cell(l.integrated_lufs,"No gated LUFS"),
        m21_private::basic_level_cell(l.full_scope_rms_dbfs,"RMS missing"),
        m21_private::basic_level_cell(l.sample_peak_dbfs,"Sample peak missing"),
        m21_private::basic_level_cell(l.true_peak_db_tp,"True Peak missing"),
        m21_private::basic_level_cell(l.sample_peak_crest_db,"Crest missing"),
        m21_private::tonal_stereo_cell(s.width_ratio,"Width missing"),
        m21_private::tonal_stereo_cell(s.correlation_p10,"P10 missing"),
        m21_private::tonal_stereo_cell(s.correlation_p50,"P50 missing"),
        m21_private::tonal_stereo_cell(s.correlation_p90,"P90 missing")
    }};
    std::array<bool,9> used{};
    double weighted=0.0,total_weight=0.0;
    for(const auto& target:selected) {
        const auto at=static_cast<std::size_t>(target.metric);
        if(at>=real.size()||used[at]
           ||!std::isfinite(target.tolerance)||target.tolerance<=0.0
           ||!std::isfinite(target.weight)||target.weight<=0.0)
            return fail("L3_PRIVATE_PROBE_INVALID_OBJECTIVE");
        used[at]=true;
        const auto& a=real[at];
        const auto& gold=reference.metrics[at];
        // Low-confidence M16 True Peak, mono width, silence and absent
        // reference support are NOT arbitrary zeros or admissible scores.
        if(a.quality!=m21_private::Quality::present||
           gold.quality!=m21_private::Quality::present||
           !a.unrounded||!gold.unrounded||
           !std::isfinite(*a.unrounded)||
           !std::isfinite(*gold.unrounded))
            return fail("L3_PRIVATE_PROBE_OBJECTIVE_NOT_SUPPORTED");
        const double d=(*gold.unrounded-*a.unrounded)/target.tolerance;
        const double term=target.weight*d*d;
        if(!std::isfinite(term))
            return fail("L3_PRIVATE_PROBE_NONFINITE_OBJECTIVE");
        weighted+=term;
        total_weight+=target.weight;
    }
    if(!std::isfinite(weighted)||!std::isfinite(total_weight)
       ||total_weight<=0.0||!std::isfinite(weighted/total_weight))
        return fail("L3_PRIVATE_PROBE_NONFINITE_SCORE");
    return R::success(PrivateScalarProbe{
        weighted/total_weight,selected.size(),l.scope_frame_count,
        static_cast<std::uint32_t>(l.sample_rate_hz)});
}
} // namespace rgsml::app::l3_automatch
