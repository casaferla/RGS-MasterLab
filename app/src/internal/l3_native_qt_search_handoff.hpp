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
} // namespace rgsml::app::l3_automatch
