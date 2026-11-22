#pragma once
// L3-M12 accepts true M16 measurement from an authentic full candidate.
// M17 Export TP Limiter and P2 Streaming TP Limiter are fully independent.
// No ceiling or state is read from, or written into, either limiter.
#include "l3_m12_ref_linked_native_stage.hpp"
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cmath>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m12_frozen {
[[nodiscard]] inline core::Result<StagedRefLinkedSegment>
stage_m12_with_independent_m16_safety(
    AcceptedPlanAndOwner accepted,
    const dsp::ProcessingChain& protected_manual,
    std::span<const dsp::ModuleExecutionBinding> protected_bindings,
    const audio::AudioBuffer& real_current_candidate_pcm,
    double frozen_automatch_true_peak_max_db_tp,
    bool current_product_owner_and_m11_safety,
    std::shared_ptr<const dsp::ModuleRegistry> live_registry,
    std::vector<ExactLinkedNode> exact_winner_nodes)
{
    using R=core::Result<StagedRefLinkedSegment>;
    if(!current_product_owner_and_m11_safety||
       !std::isfinite(frozen_automatch_true_peak_max_db_tp)||
       real_current_candidate_pcm.view().absolute_start_frame().value()!=0||
       real_current_candidate_pcm.view().frame_count().value()<=0||
       real_current_candidate_pcm.view().timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "L3_M12_CURRENT_M16_CANDIDATE_OWNER_INVALID"});
    const auto measured=analysis::measure_true_peak_full_resource(
        real_current_candidate_pcm);
    if(!measured)return R::failure(*measured.error());
    const double max_linear=std::pow(
        10.0,frozen_automatch_true_peak_max_db_tp/20.0);
    if(!std::isfinite(max_linear)||max_linear<=0.0||
       !std::isfinite(measured.value()->global_linear_peak())||
       measured.value()->global_linear_peak()>max_linear)
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "L3_M12_REAL_M16_MATCH_SAFETY_BOUND_FAILED"});
    // Numerical M16 can be used without claiming global M16 Gate-G
    // official qualification or manufacturing a master export.
    accepted.independent_m16_measurement_and_m11_safety_attested=true;
    return stage_exact_m12_native_ref_linked(
        accepted,protected_manual,protected_bindings,
        std::move(live_registry),std::move(exact_winner_nodes));
}
} // namespace rgsml::app::l3_automatch::m12_frozen
