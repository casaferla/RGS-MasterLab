#pragma once
// L3 M12 Product readback of the EXACT transaction after-state.
// This is a NON-AUTHORIZING predicate: the Product Qt owner must first
// atomically adopt and persist a validated candidate with its M18
// exclusive epoch lease; a candidate ProjectSnapshot itself is NEVER
// evidence of a completed transaction. M13 requires a separate
// owner-issued materialization receipt and fresh verification.
#include "l3_m12_project_transaction_draft.hpp"
#include "canonical_json.hpp"
#include <rgsml/project/project_snapshot.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace rgsml::app::l3_automatch::m12_frozen {
enum class PostCommitReadbackReason:std::uint8_t {
    exact_after_state,
    missing_real_owner_transaction,
    stale_or_wrong_project,
    incomplete_or_noncanonical_readback,
    unrelated_optional_entries_changed,
    before_and_after_indistinguishable
};
struct M12PostCommitReadbackAudit final {
    PostCommitReadbackReason reason{
        PostCommitReadbackReason::missing_real_owner_transaction};
    core::Uuid plan_id;
    core::Uuid controller_id;
    [[nodiscard]] constexpr bool exact_after_state_observed()
        const noexcept {
        return reason==PostCommitReadbackReason::exact_after_state;
    }
    [[nodiscard]] constexpr bool is_product_materialization_receipt()
        const noexcept{return false;}
    [[nodiscard]] constexpr bool authorizes_m13_verification()
        const noexcept{return false;}
};
[[nodiscard]] inline M12PostCommitReadbackAudit
audit_exact_m12_postcommit_project_readback(
    const project::ProjectSnapshot& exact_before,
    const M12PreparedProjectTransaction& prepared,
    const project::ProjectSnapshot& actual_fresh_owner_readback,
    bool real_exclusive_product_transaction_committed_and_epoch_rechecked)
{
    const auto reject=[&](PostCommitReadbackReason why){
        return M12PostCommitReadbackAudit{
            why,prepared.plan_id,prepared.controller_id};
    };
    if(!real_exclusive_product_transaction_committed_and_epoch_rechecked)
        return reject(PostCommitReadbackReason::
            missing_real_owner_transaction);
    const auto& expected=prepared.candidate_not_yet_committed;
    const auto& exp=expected.document();
    const auto& got=actual_fresh_owner_readback.document();
    const auto& prior=exact_before.document();
    if(prepared.plan_id.is_nil()||prepared.controller_id.is_nil()||
       prepared.ref_linked_chain_id.is_nil()||
       prepared.accepted_plan_checksum.empty()||
       prepared.pre_commit_project_revision_dependency.empty()||
       exp.projectId.is_nil()||
       exp.projectId!=got.projectId||
       exp.projectId!=prior.projectId||
       exp.sourceResourceId!=got.sourceResourceId||
       exp.sourceResourceId!=prior.sourceResourceId||
       exp.references.activeReferenceId!=got.references.activeReferenceId||
       exp.pipeline.masteringChainId!=got.pipeline.masteringChainId)
        return reject(PostCommitReadbackReason::stale_or_wrong_project);
    // Use the EXISTING project canonical serializer, including all
    // opaque metadata. No new canonicalization/checksum authority.
    const auto desired=project::internal::write_project_json(exp);
    const auto actual=project::internal::write_project_json(got);
    const auto before=project::internal::write_project_json(prior);
    if(!desired||!actual||!before)
        return reject(PostCommitReadbackReason::
            incomplete_or_noncanonical_readback);
    if(*before.value()==*desired.value())
        return reject(PostCommitReadbackReason::
            before_and_after_indistinguishable);
    if(*actual.value()!=*desired.value())
        return reject(PostCommitReadbackReason::
            incomplete_or_noncanonical_readback);
    // Optional entries are not in the ProjectDocument JSON and must
    // be checked separately; a "successful" receipt cannot lose
    // unrelated project attachments/history bundles.
    const auto& a=expected.optional_entries();
    const auto& b=actual_fresh_owner_readback.optional_entries();
    if(a.size()!=b.size())
        return reject(PostCommitReadbackReason::
            unrelated_optional_entries_changed);
    for(std::size_t i=0;i<a.size();++i)
        if(a[i].path!=b[i].path||a[i].mediaType!=b[i].mediaType||
           a[i].bytes!=b[i].bytes)
            return reject(PostCommitReadbackReason::
                unrelated_optional_entries_changed);
    return reject(PostCommitReadbackReason::exact_after_state);
}
} // namespace rgsml::app::l3_automatch::m12_frozen
