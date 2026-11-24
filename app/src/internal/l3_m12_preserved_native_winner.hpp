#pragma once
// Bind the actual measured solver winner to the M12 staged typed graph.
// This is a prepublication guard, never a materialization receipt.
#include "l3_m10_preserved_native_refinement.hpp"
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
} // namespace rgsml::app::l3_automatch::m12_frozen
