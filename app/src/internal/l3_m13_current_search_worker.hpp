#pragma once
#include "l3_m12_current_search_transaction.hpp"
#include "l3_m13_actual_native_worker.hpp"

namespace rgsml::app::l3_automatch::m13_frozen {
// Only a genuine Product-issued fresh-probe request can enter this
// adapter. Never substitute the M10 prediction for the fresh render.
[[nodiscard]] inline std::optional<M13ActualNativeWorkerInputs>
capture_current_search_verification_inputs(
    const NativeAutoMatchWorkerResult& worker,
    const m12_frozen::AcceptedPlanAndOwner& accepted,
    M13ImmutableProductFreshProbeRequest request,
    M13OwnedNativeSpec spec,
    std::shared_ptr<const OriginalPreparedReferenceNativeFacts> originals,
    const OfflineProductOwnerStamp& source_owner,
    const std::string& exact_generation,
    bool exact_independent_scopes_current,bool file_based_lra_policy_attested,
    bool exact_endpoint_and_independent_safety_attested,
    MandatoryHardPredicate other_hard_safety)
{
    if(worker.phase!=NativeAutoMatchWorkerPhase::search_completed||
       worker.search.status!=m09_frozen::NativeProductSearchStatus::measured_candidate||
       !worker.search.winning_graph||!worker.search.refinement||!worker.fixed||
       !originals||!original_pair_matches_product_owner(*originals,source_owner)||
       !accepted.current_source_reference_owner_and_plan_attested||
       !accepted.m11_protected_and_full_candidate_safety_passed||
       !accepted.independent_m16_measurement_and_m11_safety_attested||
       accepted.no_correction_required||!exact_independent_scopes_current||
       !exact_endpoint_and_independent_safety_attested)return {};
    const auto& batch=*worker.search.refinement;
    if((batch.reason!=m10_frozen::RefinementBatchReason::complete&&
        batch.reason!=m10_frozen::RefinementBatchReason::budget_exhausted)||
       !batch.winning_refinement_index||
       *batch.winning_refinement_index>=batch.candidate_refinements.size())return {};
    const auto& refined=batch.candidate_refinements[*batch.winning_refinement_index];
    const auto& winner=refined.numerical.winner;
    const auto& owned=*worker.search.winning_graph;
    const auto& replay=request.exact_materialized_preview_owner().replay;
    if(refined.numerical.end==m10_frozen::End::cancelled||
       !m10_frozen::valid_key(winner)||!winner.native_prediction||!replay||
       replay->plan_id!=accepted.plan_id||
       replay->controller_id!=accepted.reference_match_controller_id||
       replay->exact_plan_checksum!=accepted.exact_plan_content_checksum||
       replay->exact_m10_winner_checksum!=accepted.exact_m10_result_and_topology_checksum||
       replay->exact_m11_protection_checksum!=accepted.exact_m11_fixed_and_protection_checksum||
       replay->exact_source_reference_scope_revision!=accepted.exact_source_reference_amount_scope_revision||
       replay->reference_linked.revision()!=accepted.exact_accepted_plan_revision||
       owned.exact_combined_ordinal!=refined.exact_combined_ordinal)return {};
    const auto& prediction=*winner.native_prediction;
    if(!m10_frozen::native_prediction_passes_current_m16(prediction,
           request.frozen_independent_m16_ceiling_db_tp())||
       prediction.exact_candidate_ticket!=owned.exact_ticket||
       prediction.exact_m07_generation!=exact_generation||
       !prediction.original_definition_authority||
       m10_frozen::exact_original_native_definition_content(*prediction.original_definition_authority)!=
           prediction.exact_definition_authority_content||
       !authentic_product_candidate_identity(
           {prediction.candidate_resource_identity,prediction.candidate_generation,true},
           source_owner,prediction.exact_candidate_ticket))return {};
    return M13ActualNativeWorkerInputs{
        std::move(request),std::move(spec),std::move(originals),source_owner,
        *prediction.original_definition_authority,exact_generation,
        accepted.exact_source_reference_amount_scope_revision,winner,
        accepted.exact_m10_result_and_topology_checksum,accepted.exact_m11_fixed_and_protection_checksum,
        true,exact_independent_scopes_current,file_based_lra_policy_attested,
        exact_endpoint_and_independent_safety_attested,other_hard_safety};
}
} // namespace rgsml::app::l3_automatch::m13_frozen
