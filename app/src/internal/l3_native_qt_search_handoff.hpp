#pragma once
#include "l3_native_qt_search_worker.hpp"
#include "l3_m12_current_search_transaction.hpp"
#include "l3_m13_current_search_worker.hpp"
#include "project_session_view_model.hpp"
#include <rgsml/core/sha256.hpp>
#include <bit>

namespace rgsml::app::l3_automatch {
// Construct the review identity from the actual measured winner, not
// from placeholder strings supplied by the UI. This does not apply it.
[[nodiscard]] inline std::optional<m12_frozen::AcceptedPlanAndOwner>
build_native_qt_measured_plan(
    const NativeQtSearchCapture& captured,const NativeQtSearchPolicy& policy,
    const NativeQtSearchWorkerResult& completed,core::Uuid plan_id,core::Uuid controller_id)
{
    namespace m10=m10_frozen;
    namespace m11=m11_frozen;
    const auto& worker=completed.result;
    const auto& search=worker.search;
    const auto owners=captured.native_worker_scope();
    if(plan_id.is_nil()||controller_id.is_nil()||plan_id==controller_id||
       !completed.originals||!original_pair_matches_product_owner(*completed.originals,owners.current_owner)||
       worker.phase!=NativeAutoMatchWorkerPhase::search_completed||
       search.status!=m09_frozen::NativeProductSearchStatus::measured_candidate||
       !worker.fixed||!search.winning_graph||!search.refinement||
       !search.refinement->winning_refinement_index||
       *search.refinement->winning_refinement_index>=search.refinement->candidate_refinements.size()||
       !policy.exact_current_endpoint_safety_attested||
       !m10::valid_native_definition_policy(policy.definitions)||
       policy.exact_generation.empty()||policy.generation_and_policy.empty()||
       policy.accepted_dependencies.empty())return {};
    const auto& fixed=*worker.fixed;
    const auto& key=search.refinement->candidate_refinements[
        *search.refinement->winning_refinement_index].numerical.winner;
    if(!m10::valid_key(key)||!key.native_prediction||
       fixed.exact_m07_generation!=policy.exact_generation||
       fixed.exact_fixed_graph_content.empty()||
       fixed.exact_definition_authority_content.empty())return {};
    const auto& prediction=*key.native_prediction;
    if(!m10::native_prediction_passes_current_m16(prediction,policy.independent_ceiling_db_tp)||
       prediction.requested_reporting_domains!=std::optional<std::array<bool,4>>{policy.requested})
        return {};
    const auto bound=m11::bind_native_fixed_to_candidate(fixed,prediction.native_population,policy.amount);
    if(!bound||*bound!=prediction.protected_k)return {};
    std::vector<m11::ProtectedComponent> required;
    for(const auto& c:*bound)if(c.required_in_frozen_k)required.push_back(c);
    if(!m11::verify_frozen_protected_no_regression(required,true,true).is_admissible())return {};
    using namespace m10::p2_cache_detail;
    const auto hash=[](const m10::ExactCacheBytes& v){
        return core::sha256_hex(std::as_bytes(std::span{v}));
    };
    const auto add_real=[](m10::ExactCacheBytes& v,double n){
        append_exact_field(v,std::to_string(std::bit_cast<std::uint64_t>(n)));
    };
    m10::ExactCacheBytes scope,winner,protection,plan;
    append_exact_field(scope,"rgsml.l3.accepted-scope/1");
    const auto& stamp=owners.current_owner;
    for(auto n:{stamp.m18_owner_epoch,stamp.source_generation,stamp.reference_selection_revision,
                captured.linked_revision,policy.amount.independent_amount_revision})
        append_exact_field(scope,std::to_string(n));
    for(const auto* field:{&stamp.source_provider_id,&stamp.source_locator,
                           &stamp.reference_provider_id,&stamp.reference_locator,&policy.exact_generation})
        append_exact_field(scope,*field);
    append_exact_byte_field(scope,policy.generation_and_policy);
    append_exact_byte_field(scope,policy.accepted_dependencies);
    add_real(scope,policy.amount.global);
    for(auto n:policy.amount.domains)add_real(scope,n);
    for(auto n:policy.requested)scope.push_back(n?1U:0U);
    add_real(scope,policy.independent_ceiling_db_tp);
    append_exact_field(winner,"rgsml.l3.accepted-winner/1");
    append_exact_field(winner,prediction.exact_candidate_ticket);
    append_exact_byte_field(winner,key.canonical_current_tuple);
    append_exact_byte_field(winner,key.topology_evidence_identity);
    append_exact_byte_field(winner,prediction.exact_execution_graph_content);
    append_exact_byte_field(winner,prediction.exact_definition_authority_content);
    add_real(winner,key.total_j);add_real(winner,key.target_j);add_real(winner,key.processing_r);
    add_real(winner,*prediction.independent_full_m16_linear_peak);
    append_exact_field(protection,"rgsml.l3.accepted-protection/1");
    append_exact_field(protection,fixed.exact_fixed_ticket);
    append_exact_byte_field(protection,fixed.exact_fixed_graph_content);
    append_exact_byte_field(protection,fixed.exact_definition_authority_content);
    for(const auto& c:*bound) {
        protection.push_back(c.required_in_frozen_k?1U:0U);
        protection.push_back(c.protected_by_m11_mask?1U:0U);
        protection.push_back(c.exact_current?1U:0U);
        for(double n:{c.prepared_z,c.gold_z,c.fixed_z,c.candidate_z,c.frozen_deadband})
            add_real(protection,n);
    }
    const auto scope_hash=hash(scope),winner_hash=hash(winner),protection_hash=hash(protection);
    append_exact_field(plan,"rgsml.l3.accepted-plan/1");
    for(const auto& field:{plan_id.to_string(),controller_id.to_string(),scope_hash,winner_hash,protection_hash})
        append_exact_field(plan,field);
    m12_frozen::AcceptedPlanAndOwner accepted{plan_id,controller_id,hash(plan),
        winner_hash,protection_hash,scope_hash,captured.linked_revision,true,true,true,false};
    if(!m12_frozen::stage_current_native_search_winner(accepted,search,owners,policy.exact_generation))return {};
    return accepted;
}
// Real owner-thread handoff. The worker cannot issue a receipt and a
// completed search cannot replace a changed source, REFERENCE or project.
[[nodiscard]] inline core::Result<m12_frozen::M12ProductMaterializationReceipt>
apply_native_qt_search_on_owner(
    ProjectSessionViewModel& session,
    const ProductQtP2NativeAdmission::Controllers& product,
    const ProductQtP2NativeAdmission::Fanout& fanout,
    const AuditionSourceSelector& selector,const GoldSelectionViewModel& gold,
    const NativeQtSearchCapture& captured,const NativeQtSearchPolicy& policy,
    const NativeQtSearchWorkerResult& completed,
    const m12_frozen::AcceptedPlanAndOwner& accepted,
    const project::ProjectSnapshot& pre_search_project,core::Uuid linked_chain_id,
    const m12_frozen::M12ProjectTransactionExpected& expected)
{
    using R=core::Result<m12_frozen::M12ProductMaterializationReceipt>;
    if(QThread::currentThread()!=session.thread()||
       !native_qt_search_still_current_on_owner(product,fanout,selector,gold,captured)||
       !completed.originals||
       !original_pair_matches_product_owner(*completed.originals,captured.current.job.owner_stamp())||
       accepted.exact_accepted_plan_revision!=captured.linked_revision)
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "L3_QT_SEARCH_MATERIALIZATION_OWNER_STALE"});
    // Re-admit the retained physical M16 result against the exact
    // current policy. Do not accept a flag or a scoped L03 proxy alone.
    const auto& search=completed.result.search;
    if(!search.refinement||!search.refinement->winning_refinement_index||
       *search.refinement->winning_refinement_index>=search.refinement->candidate_refinements.size())
        return R::failure(core::Error{core::ErrorCode::InvalidState,"L3_QT_SEARCH_WINNER_MISSING"});
    const auto& winner=search.refinement->candidate_refinements[
        *search.refinement->winning_refinement_index].numerical.winner;
    if(!winner.native_prediction||
       !m10_frozen::native_prediction_passes_current_m16(*winner.native_prediction,policy.independent_ceiling_db_tp))
        return R::failure(core::Error{core::ErrorCode::InvalidState,"L3_QT_SEARCH_INDEPENDENT_M16_STALE"});
    const auto owners=captured.native_worker_scope();
    return session.apply_private_m12_current_native_search(completed.result,accepted,owners,
        policy.exact_generation,pre_search_project,linked_chain_id,expected);
}
// Post-publication verification deliberately captures the new M12 owner,
// rather than reusing the pre-search epoch. The independent original pair
// is retained, but the candidate must be rendered and measured anew.
[[nodiscard]] inline std::optional<m13_frozen::M13ActualNativeWorkerInputs>
capture_native_qt_fresh_verification_on_owner(
    ProjectSessionViewModel& session,const GoldSelectionViewModel& gold,
    const NativeQtSearchCapture& captured,const NativeQtSearchPolicy& policy,
    const NativeQtSearchWorkerResult& completed,
    const m12_frozen::AcceptedPlanAndOwner& accepted,
    bool independent_scopes_current,
    m13_frozen::MandatoryHardPredicate other_hard_safety)
{
    if(QThread::currentThread()!=session.thread()||
       QThread::currentThread()!=gold.thread()||
       gold.selection_revision()!=captured.current.job.owner_stamp().reference_selection_revision||
       !completed.originals||!independent_scopes_current)return {};
    auto spec=m13_frozen::M13OwnedNativeSpec::capture(captured.common_spec.prepared);
    if(!spec)return {};
    auto request=session.capture_private_m13_fresh_postmaterialization_probe(policy.independent_ceiling_db_tp);
    if(!request||
       request.value()->exact_materialized_preview_owner().exact_prepared_owner!=captured.current.source.prepared)
        return {};
    return m13_frozen::capture_current_search_verification_inputs(
        completed.result,accepted,std::move(*request.value()),std::move(*spec),
        completed.originals,captured.current.job.owner_stamp(),policy.exact_generation,
        independent_scopes_current,policy.file_based_lra_policy_attested,
        policy.exact_current_endpoint_safety_attested,other_hard_safety);
}
} // namespace rgsml::app::l3_automatch
