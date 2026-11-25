#pragma once
// First Product AutoMatch operation. Owner-thread capture and private
// worker data are deliberately separate from eventual user adoption.
#include "l3_native_qt_product_policy.hpp"
#include "l3_native_qt_search_handoff.hpp"
#include <set>
#include <limits>

namespace rgsml::app::l3_automatch {
struct NativeQtProductSession final {
    project::ProjectSnapshot baseline;
    NativeQtSearchCapture capture;
    NativeQtSearchPolicy policy;
    m12_frozen::M12ProjectTransactionExpected expected;
    std::vector<core::Uuid> protected_ids;
    core::Uuid plan_id,controller_id,linked_chain_id;
};
[[nodiscard]] inline core::Result<NativeQtProductSession>
begin_native_qt_product_session_on_owner(
    ProjectSessionViewModel& session,const ProductQtP2NativeAdmission::Controllers& product,
    const ProductQtP2NativeAdmission::Fanout& fanout,const AuditionSourceSelector& selector,
    const GoldSelectionViewModel& gold,const m08_frozen::AmountConfig& amount,
    const std::array<bool,4>& requested,double independent_ceiling_db_tp,
    core::FrameCount block,core::Uuid plan_id,core::Uuid controller_id,core::Uuid linked_chain_id,
    bool allow_existing_replacement=false)
{
    using R=core::Result<NativeQtProductSession>;
    const auto fail=[](const char* why){return R::failure(core::Error{core::ErrorCode::InvalidState,why});};
    if(QThread::currentThread()!=session.thread()||
       &session!=&product.upstream.project||
       plan_id.is_nil()||controller_id.is_nil()||linked_chain_id.is_nil())
        return fail("L3_PRODUCT_SESSION_INVALID_OWNER");
    auto baseline=session.capture_private_m12_search_baseline(allow_existing_replacement);
    if(!baseline)return R::failure(*baseline.error());
    const auto& doc=baseline.value()->document();
    if(!doc.references.activeReferenceId||!doc.pipeline.masteringChainId)
        return fail("L3_PRODUCT_SESSION_MISSING_SOURCE_REFERENCE_MANUAL");
    const auto manual=std::find_if(doc.chains.begin(),doc.chains.end(),[&](const auto& c){
        return c.chainId==*doc.pipeline.masteringChainId&&c.stage=="MASTER"&&c.segment=="MANUAL";
    });
    if(manual==doc.chains.end())return fail("L3_PRODUCT_SESSION_MANUAL_MISSING");
    const auto prior=std::find_if(doc.chains.begin(),doc.chains.end(),[](const auto& c){
        return c.stage=="MASTER"&&c.segment=="REF_LINKED";
    });
    const bool replacing=prior!=doc.chains.end();
    std::uint64_t revision=1U;
    if(replacing) {
        if(!allow_existing_replacement)
            return fail("L3_PRODUCT_SESSION_EXISTING_LINKED_REQUIRES_REPLACEMENT");
        if(!prior->revision||prior->revision==std::numeric_limits<std::uint64_t>::max())
            return fail("L3_PRODUCT_SESSION_LINKED_REVISION_EXHAUSTED");
        revision=prior->revision+1U;
    }
    std::set<core::Uuid> used;
    used.insert(doc.projectId);used.insert(doc.sourceResourceId);
    for(const auto& r:doc.resources)used.insert(r.resourceId);
    for(const auto& r:doc.references.items)used.insert(r.referenceId);
    for(const auto& r:doc.timeSelections.auditionRegions)used.insert(r.regionId);
    for(const auto& c:doc.chains) {
        used.insert(c.chainId);
        for(const auto& node:c.modules) {
            used.insert(node.instanceId);
            if(node.semanticNodeId)used.insert(*node.semanticNodeId);
        }
    }
    if(!used.insert(plan_id).second)return fail("L3_PRODUCT_SESSION_ID_COLLISION");
    if(!replacing) {
        for(const auto& id:{controller_id,linked_chain_id})
            if(!used.insert(id).second)return fail("L3_PRODUCT_SESSION_ID_COLLISION");
    }
    auto capture=capture_native_qt_search_on_owner(product,fanout,selector,gold,
        plan_id.to_string()+"/fixed",revision,block,true);
    if(!capture)return R::failure(*capture.error());
    if(replacing) {
        const auto restored=m12_frozen::decode_exact_m12_ref_linked_product_state(
            doc,capture.value()->execution_registry);
        if(!restored)return R::failure(*restored.error());
        const auto& exact=*restored.value();
        if(exact.plan_id==plan_id||exact.controller_id==plan_id||
           exact.persisted_linked_chain_id!=prior->chainId||
           exact.exact_native_chain.revision()!=prior->revision)
            return fail("L3_PRODUCT_SESSION_PRIOR_OWNER_MISMATCH");
        controller_id=exact.controller_id;
        linked_chain_id=exact.persisted_linked_chain_id;
        used.insert(controller_id);
        used.insert(exact.plan_id);
    }
    auto policy=build_current_native_qt_product_policy(*capture.value(),amount,requested,independent_ceiling_db_tp);
    if(!policy)return fail("L3_PRODUCT_SESSION_POLICY_UNAVAILABLE");
    m12_frozen::M12ProjectTransactionExpected expected{doc.sourceResourceId,
        *doc.references.activeReferenceId,*doc.pipeline.masteringChainId,*manual,
        replacing?std::optional<project::Chain>{*prior}:std::nullopt,true,true};
    std::vector<core::Uuid> protected_ids(used.begin(),used.end());
    return R::success(NativeQtProductSession{std::move(*baseline.value()),
        std::move(*capture.value()),std::move(*policy),std::move(expected),
        std::move(protected_ids),plan_id,controller_id,linked_chain_id});
}
// Worker entry point for a single admitted Product operation. All of
// its dependencies and reserved identities travel with the session.
[[nodiscard]] inline NativeQtSearchWorkerResult
run_native_qt_product_session_worker(
    const NativeQtProductSession& operation,
    m09_frozen::NativeOwnerAllocations::AllocateUuid allocate_uuid,
    const std::atomic<bool>* cancellation=nullptr)
{
    return run_native_qt_search_worker(operation.capture,operation.policy,
        std::move(allocate_uuid),operation.protected_ids,cancellation);
}

// Review is not publication. A GUI may retain the immutable result,
// but a changed Product owner cannot offer it as a current candidate.
[[nodiscard]] inline std::optional<m12_frozen::AcceptedPlanAndOwner>
review_native_qt_product_session_on_owner(
    ProjectSessionViewModel& session,const ProductQtP2NativeAdmission::Controllers& product,
    const ProductQtP2NativeAdmission::Fanout& fanout,const AuditionSourceSelector& selector,
    const GoldSelectionViewModel& gold,const NativeQtProductSession& operation,
    const NativeQtSearchWorkerResult& completed)
{
    if(QThread::currentThread()!=session.thread()||&session!=&product.upstream.project||
       !native_qt_search_still_current_on_owner(product,fanout,selector,gold,operation.capture))
        return {};
    return build_native_qt_measured_plan(operation.capture,operation.policy,completed,
        operation.plan_id,operation.controller_id);
}

// Adoption requires the explicit reviewed plan and the original baseline;
// it cannot accidentally substitute a newer snapshot or a new chain ID.
[[nodiscard]] inline core::Result<m12_frozen::M12ProductMaterializationReceipt>
apply_native_qt_product_session_on_owner(
    ProjectSessionViewModel& session,const ProductQtP2NativeAdmission::Controllers& product,
    const ProductQtP2NativeAdmission::Fanout& fanout,const AuditionSourceSelector& selector,
    const GoldSelectionViewModel& gold,const NativeQtProductSession& operation,
    const NativeQtSearchWorkerResult& completed,const m12_frozen::AcceptedPlanAndOwner& reviewed)
{
    using R=core::Result<m12_frozen::M12ProductMaterializationReceipt>;
    if(QThread::currentThread()!=session.thread()||&session!=&product.upstream.project||
       reviewed.plan_id!=operation.plan_id||
       reviewed.reference_match_controller_id!=operation.controller_id)
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "L3_PRODUCT_SESSION_REVIEW_IDENTITY_MISMATCH"});
    return apply_native_qt_search_on_owner(session,product,fanout,selector,gold,
        operation.capture,operation.policy,completed,reviewed,operation.baseline,
        operation.linked_chain_id,operation.expected);
}
// A positive no-correction result is not Amount zero. Only a freshly
// measured, fully safe Fixed graph may propose removing old REF nodes.
[[nodiscard]] inline bool native_qt_positive_no_correction(
    const NativeQtProductSession& operation,const NativeQtSearchWorkerResult& completed)
{
    const auto& policy=operation.policy;
    const auto& worker=completed.result;
    const auto owners=operation.capture.native_worker_scope();
    if(!completed.originals||!worker.fixed||
       worker.phase!=NativeAutoMatchWorkerPhase::search_completed||
       worker.search.status!=m09_frozen::NativeProductSearchStatus::no_correction_required||
       worker.search.fixed_gate.status!=m10_frozen::NativeFixedSearchStatus::no_correction_required||
       worker.fixed->exact_m07_generation!=policy.exact_generation||
       worker.fixed->exact_fixed_ticket!=operation.capture.current.job.ticket_identity()||
       !original_pair_matches_product_owner(*completed.originals,owners.current_owner))
        return false;
    bool positive=false;
    for(std::size_t i=0;i<4U;++i)
        positive=positive||(policy.requested[i]&&m08_frozen::effective_amount(policy.amount,i).rounded>0.0);
    if(!positive)return false;
    m10_frozen::ExactCompletedEvaluationCache cache{policy.generation_and_policy};
    const m10_frozen::PreservedNativeSearchEnvironment env{owners,*completed.originals,
        *worker.fixed,operation.capture.common_spec.prepared,policy.exact_generation,cache,
        policy.independent_ceiling_db_tp,policy.file_based_lra_policy_attested};
    return m10_frozen::assess_native_fixed_search_gate(env,policy.amount,policy.requested,
        policy.exact_current_endpoint_safety_attested).status==
        m10_frozen::NativeFixedSearchStatus::no_correction_required;
}
[[nodiscard]] inline core::Status apply_native_qt_no_correction_on_owner(
    ProjectSessionViewModel& session,const ProductQtP2NativeAdmission::Controllers& product,
    const ProductQtP2NativeAdmission::Fanout& fanout,const AuditionSourceSelector& selector,
    const GoldSelectionViewModel& gold,const NativeQtProductSession& operation,
    const NativeQtSearchWorkerResult& completed)
{
    if(QThread::currentThread()!=session.thread()||&session!=&product.upstream.project||
       !operation.expected.exact_existing_linked_snapshot||
       !native_qt_search_still_current_on_owner(product,fanout,selector,gold,operation.capture)||
       !native_qt_positive_no_correction(operation,completed))
        return core::Status::failure(core::Error{core::ErrorCode::InvalidState,
            "L3_PRODUCT_NO_CORRECTION_NOT_CURRENT"});
    return session.clear_private_m12_owned_match(operation.baseline,operation.controller_id,
        operation.linked_chain_id,operation.capture.current.source.prepared);
}
// M13 follows explicit adoption. Its new owner is captured through
// Project, while the original session still authenticates the winner.
[[nodiscard]] inline std::optional<m13_frozen::M13ActualNativeWorkerInputs>
capture_native_qt_product_session_verification_on_owner(
    ProjectSessionViewModel& session,const GoldSelectionViewModel& gold,
    const NativeQtProductSession& operation,const NativeQtSearchWorkerResult& completed,
    const m12_frozen::AcceptedPlanAndOwner& reviewed,
    bool independent_scopes_current,
    m13_frozen::MandatoryHardPredicate other_hard_safety)
{
    if(reviewed.plan_id!=operation.plan_id||
       reviewed.reference_match_controller_id!=operation.controller_id)
        return {};
    return capture_native_qt_fresh_verification_on_owner(session,gold,
        operation.capture,operation.policy,completed,reviewed,
        independent_scopes_current,other_hard_safety);
}
} // namespace rgsml::app::l3_automatch
