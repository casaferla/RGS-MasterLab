#pragma once
#include "l3_native_qt_search_worker.hpp"
#include "l3_m12_current_search_transaction.hpp"
#include "l3_m13_current_search_worker.hpp"
#include "project_session_view_model.hpp"

namespace rgsml::app::l3_automatch {
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
