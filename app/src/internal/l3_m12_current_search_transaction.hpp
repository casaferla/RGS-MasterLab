#pragma once
#include "l3_native_automatch_worker.hpp"
#include "l3_m12_preserved_native_winner.hpp"
#include "l3_m12_project_transaction_draft.hpp"

namespace rgsml::app::l3_automatch::m12_frozen {
// The worker cannot publish a project. Build the existing immutable
// transaction from the exact retained graph, never from a second graph
// supplied alongside the worker result.
[[nodiscard]] inline core::Result<M12PreparedProjectTransaction>
prepare_current_native_search_transaction(
    const NativeAutoMatchWorkerResult& worker,
    const AcceptedPlanAndOwner& accepted,
    const m10_frozen::P2NativeProductWorkerScope& owners,
    const std::string& exact_generation,
    const project::ProjectSnapshot& current_project,
    core::Uuid linked_chain_id,M12ProjectTransactionExpected expected)
{
    using R=core::Result<M12PreparedProjectTransaction>;
    if(worker.phase!=NativeAutoMatchWorkerPhase::search_completed||
       worker.search.status!=m09_frozen::NativeProductSearchStatus::measured_candidate||
       !worker.fixed||!worker.search.winning_graph||
       accepted.no_correction_required||linked_chain_id.is_nil())
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "L3_M12_CURRENT_WORKER_WINNER_NOT_MATERIALIZABLE"});
    auto stage=stage_current_native_search_winner(accepted,worker.search,owners,exact_generation);
    if(!stage)return R::failure(*stage.error());
    auto carrier=encode_exact_ref_linked_project_carrier(*stage.value(),linked_chain_id);
    if(!carrier)return R::failure(*carrier.error());
    return prepare_exact_m12_project_transaction(current_project,*carrier.value(),
        *stage.value(),std::move(expected));
}
} // namespace rgsml::app::l3_automatch::m12_frozen
