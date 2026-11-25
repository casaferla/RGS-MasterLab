#pragma once
// Bind the actual measured solver winner to the M12 staged typed graph.
// This is a prepublication guard, never a materialization receipt.
#include "l3_m10_preserved_native_refinement.hpp"
#include "l3_m09_native_selected_search.hpp"
#include "l3_m10_original_definition_authority.hpp"
#include "l3_m12_ref_linked_native_stage.hpp"

namespace rgsml::app::l3_automatch::m12_frozen {
[[nodiscard]] inline bool matches_preserved_native_winner(
    const m10_frozen::MeasuredOrderKey& winner,
    const M10P2RealNativeProductJob& reconstructed,
    const std::string& exact_generation)
{
    if(!m10_frozen::valid_key(winner)||!winner.native_prediction||
       !reconstructed.preserves_user_terminal()||exact_generation.empty())
        return false;
    const auto& prediction=*winner.native_prediction;
    const m05m06_real::SourceIdentity identity{
        prediction.candidate_resource_identity,prediction.candidate_generation,true};
    return prediction.native_population.size()==m07_frozen::kBindings.size()&&
           prediction.exact_m07_generation==exact_generation&&
           prediction.original_definition_authority&&
           !prediction.exact_definition_authority_content.empty()&&
           m10_frozen::exact_source_owner_equal(
               prediction.original_definition_authority->m19.actual_candidate.owner,identity)&&
           m10_frozen::exact_original_native_definition_content(
               *prediction.original_definition_authority)==
                   prediction.exact_definition_authority_content&&
           prediction.exact_candidate_ticket==reconstructed.ticket_identity()&&
           authentic_product_candidate_identity(identity,reconstructed.owner_stamp(),
                                                reconstructed.ticket_identity())&&
           !prediction.exact_linked_graph_content.empty()&&
           !prediction.exact_execution_graph_content.empty()&&
           reconstructed.exact_linked_graph_content()==
               std::optional<m10_frozen::ExactCacheBytes>{prediction.exact_linked_graph_content}&&
           reconstructed.exact_execution_graph_content()==
               std::optional<m10_frozen::ExactCacheBytes>{prediction.exact_execution_graph_content};
}
[[nodiscard]] inline core::Result<StagedRefLinkedSegment>
stage_preserved_native_measured_winner(
    const AcceptedPlanAndOwner& accepted,
    const m10_frozen::MeasuredOrderKey& winner,
    const m10_frozen::P2NativeProductWorkerScope& owners,
    const m10_frozen::PreservedNativeGraph& exact_winning_graph,
    const std::string& exact_generation)
{
    using R=core::Result<StagedRefLinkedSegment>;
    const auto fail=[](const char* why){
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    if(!owners.current_qt_product_epoch_and_m09_m11_attested||
       !winner.native_prediction||accepted.no_correction_required||
       !accepted.current_source_reference_owner_and_plan_attested||
       exact_winning_graph.linked.revision()!=accepted.exact_accepted_plan_revision)
        return fail("L3_M12_PRESERVED_WINNER_OWNER_OR_REVISION_STALE");
    auto job=M10P2RealNativeProductJob::create_preserving_user_terminal(
        owners.current_owner,owners.current_prepared,owners.frozen_registry,
        owners.frozen_manual,owners.frozen_manual_bindings,
        exact_winning_graph.linked,exact_winning_graph.bindings,
        winner.native_prediction->exact_candidate_ticket,owners.maximum_block_frames,true);
    if(!job||!matches_preserved_native_winner(winner,*job.value(),exact_generation))
        return fail("L3_M12_PRESERVED_WINNER_GRAPH_MISMATCH");
    const auto instances=exact_winning_graph.linked.instances();
    std::vector<ExactLinkedNode> nodes;
    nodes.reserve(instances.size());
    for(std::size_t i=0;i<instances.size();++i) {
        const auto& node=instances[i];
        const auto semantic=node.semantic_node_id();
        if(!semantic)return fail("L3_M12_PRESERVED_WINNER_SEMANTIC_ID_MISSING");
        nodes.push_back({node.instance_id(),*semantic,std::string(node.module_type_id()),
                         exact_winning_graph.bindings[i].parameters,
                         node.enabled(),node.user_bypass(),node.controller_suspended(),
                         node.domain_suspended()});
    }
    auto stage=stage_exact_m12_native_ref_linked(
        accepted,owners.frozen_manual,owners.frozen_manual_bindings,
        job.value()->execution_registry_snapshot(),std::move(nodes));
    if(!stage)return stage;
    const auto staged=m10_frozen::exact_native_graph_content(
        stage.value()->chain,stage.value()->exact_bindings,*stage.value()->registry_owner);
    if(staged!=std::optional<m10_frozen::ExactCacheBytes>{
                  winner.native_prediction->exact_linked_graph_content})
        return fail("L3_M12_STAGED_GRAPH_DIFFERS_FROM_MEASURED_WINNER");
    return stage;
}
// Reconstruct precisely the winning coordinate vector, not a second
// independently chosen graph or a fresh solver score. The Qt owner
// still holds the exclusive M12 publication/receipt boundary.
template<class BuildTypedTrial>
[[nodiscard]] inline core::Result<StagedRefLinkedSegment>
stage_preserved_native_refinement_winner(
    const AcceptedPlanAndOwner& accepted,
    const m10_frozen::RefinementBatchResult& result,
    const m10_frozen::PreservedNativeSearchEnvironment& env,
    BuildTypedTrial&& build_trial)
{
    using R=core::Result<StagedRefLinkedSegment>;
    const auto fail=[](const char* why){
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    using Reason=m10_frozen::RefinementBatchReason;
    if((result.reason!=Reason::complete&&result.reason!=Reason::budget_exhausted)||
       !result.winning_refinement_index||
       *result.winning_refinement_index>=result.candidate_refinements.size())
        return fail("L3_M12_NATIVE_REFINEMENT_WINNER_UNAVAILABLE");
    const auto& refined=result.candidate_refinements[*result.winning_refinement_index];
    const auto& winner=refined.numerical.winner;
    if(refined.numerical.end==m10_frozen::End::cancelled||
       !winner.native_prediction||
       !m10_frozen::valid_key(winner))
        return fail("L3_M12_NATIVE_REFINEMENT_WINNER_INVALID");
    const auto& coords=refined.numerical.coordinates;
    std::vector<double> u;
    for(const auto& c:coords)u.push_back(c.normalized_u);
    const auto ticket=m10_frozen::m10_exact_native_trial_ticket(
        refined.exact_combined_ordinal,coords,u);
    if(!ticket||*ticket!=winner.native_prediction->exact_candidate_ticket)
        return fail("L3_M12_NATIVE_REFINEMENT_COORDINATE_TICKET_MISMATCH");
    auto trial=std::invoke(build_trial,refined.exact_combined_ordinal,coords,u);
    if(!trial||!trial->exact_current_typed_graph_preflight_passed||
       trial->key.canonical_current_tuple!=winner.canonical_current_tuple||
       trial->key.topology_evidence_identity!=winner.topology_evidence_identity)
        return fail("L3_M12_NATIVE_REFINEMENT_GRAPH_RECONSTRUCTION_FAILED");
    return stage_preserved_native_measured_winner(
        accepted,winner,env.owners,trial->graph,env.generation);
}
// The search retains the reconstructed winning graph with its original
// seed anchors. M12 does not rebuild it from the refined coordinate
// vector as if that vector were the immutable seed.
[[nodiscard]] inline core::Result<StagedRefLinkedSegment>
stage_current_native_search_winner(const AcceptedPlanAndOwner& accepted,
    const m09_frozen::NativeProductSearchResult& search,
    const m10_frozen::P2NativeProductWorkerScope& owners,
    const std::string& exact_generation)
{
    using R=core::Result<StagedRefLinkedSegment>;
    const auto fail=[](const char* reason){
        return R::failure(core::Error{core::ErrorCode::InvalidState,reason});
    };
    if(search.status!=m09_frozen::NativeProductSearchStatus::measured_candidate||
       !search.refinement||!search.winning_graph||
       (search.refinement->reason!=m10_frozen::RefinementBatchReason::complete&&
        search.refinement->reason!=m10_frozen::RefinementBatchReason::budget_exhausted)||
       !search.refinement->winning_refinement_index||
       *search.refinement->winning_refinement_index>=search.refinement->candidate_refinements.size())
        return fail("L3_M12_CURRENT_SEARCH_WINNER_MISSING");
    const auto& refined=search.refinement->candidate_refinements[*search.refinement->winning_refinement_index];
    const auto& owned=*search.winning_graph;
    if(refined.numerical.end==m10_frozen::End::cancelled||
       !m10_frozen::valid_key(refined.numerical.winner)||
       owned.registry!=owners.frozen_registry||
       owned.exact_combined_ordinal!=refined.exact_combined_ordinal||
       !refined.numerical.winner.native_prediction||
       owned.exact_ticket!=refined.numerical.winner.native_prediction->exact_candidate_ticket||
       owned.winning_values.size()!=refined.numerical.coordinates.size()||
       owned.immutable_seed_inventory.size()!=owned.winning_values.size())
        return fail("L3_M12_CURRENT_SEARCH_WINNER_IDENTITY_MISMATCH");
    for(std::size_t i=0;i<owned.winning_values.size();++i)
        if(owned.winning_values[i]!=refined.numerical.coordinates[i].normalized_u||
           owned.immutable_seed_inventory[i].semantic_node_id!=refined.numerical.coordinates[i].semantic_node_id||
           owned.immutable_seed_inventory[i].stable_parameter_id!=refined.numerical.coordinates[i].stable_parameter_id)
            return fail("L3_M12_CURRENT_SEARCH_WINNER_COORDINATE_MISMATCH");
    const auto ticket=m10_frozen::m10_exact_native_trial_ticket(owned.exact_combined_ordinal,
        owned.immutable_seed_inventory,owned.winning_values);
    if(!ticket||*ticket!=owned.exact_ticket)return fail("L3_M12_CURRENT_SEARCH_WINNER_TICKET_MISMATCH");
    return stage_preserved_native_measured_winner(accepted,refined.numerical.winner,
        owners,owned.graph,exact_generation);
}
} // namespace rgsml::app::l3_automatch::m12_frozen
