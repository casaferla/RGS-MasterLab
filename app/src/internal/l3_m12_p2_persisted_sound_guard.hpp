#pragma once
// L3-M12 exact P2 sound-persistence boundary. Frozen Product M12
// P01/P04 require ordinary persisted ModuleInstance/parameters to be
// the execution truth and MANUAL/UNLINKED modules to be protected.
// An M10 candidate may measure alternate P2 Ceiling/Drive while
// searching; it MUST NOT claim materialization if those values live
// only in a transient render override. M17 WAV Export is independent.
// This preflight never changes a Project or creates a receipt.
#include "l3_m12_p2_exact_winner_preflight.hpp"
#include "l3_m12_project_transaction_draft.hpp"
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/project/opaque_json_value.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace rgsml::app::l3_automatch::m12_frozen {
enum class P2PersistedSoundReason : std::uint8_t {
    keep_fixed_manual_p2,
    missing_or_stale_m10_m12_winner,
    manual_p2_sound_conflict_requires_user_decision,
    persisted_manual_p2_conflicts_with_live_product
};
struct P2PersistedSoundAudit final {
    P2PersistedSoundReason reason{
        P2PersistedSoundReason::missing_or_stale_m10_m12_winner};
    [[nodiscard]] constexpr bool keeps_exact_frozen_sound() const noexcept {
        return reason==P2PersistedSoundReason::keep_fixed_manual_p2;
    }
    // The only valid result here is KEEP_FIXED_P2; neither the
    // successful audit nor a prepared transaction is a committed M12
    // receipt, M13 VERIFIED, or license to overwrite MANUAL parameters.
    [[nodiscard]] constexpr bool authorizes_manual_p2_mutation() const noexcept {
        return false;
    }
    [[nodiscard]] constexpr bool authorizes_project_commit() const noexcept {
        return false;
    }
};
[[nodiscard]] inline P2PersistedSoundAudit
audit_exact_m12_p2_persisted_sound(
    const StagedRefLinkedSegment& staged,
    const StagedExactP2Winner& winner,
    const dsp::ProcessingChain& live_manual,
    std::span<const dsp::ModuleExecutionBinding> live_bindings,
    const project::Chain& exact_persisted_manual,
    bool current_product_owner_and_accepted_diff_rechecked)
{
    using E=P2PersistedSoundReason;
    if(!current_product_owner_and_accepted_diff_rechecked||
       !winner.matches(staged,live_manual,live_bindings)||
       exact_persisted_manual.stage!="MASTER"||
       exact_persisted_manual.segment!="MANUAL"||
       exact_persisted_manual.revision!=live_manual.revision()||
       exact_persisted_manual.modules.size()!=live_bindings.size()||
       live_bindings.empty())
        return {E::missing_or_stale_m10_m12_winner};
    const auto& final=exact_persisted_manual.modules.back();
    const auto& live_node=live_manual.instances().back();
    if(final.instanceId!=winner.exact_p2_instance_id.uuid()||
       final.instanceId!=live_node.instance_id().uuid()||
       final.typeId!="rgsml.dsp.streaming-true-peak-limiter"||
       final.provenance!="MANUAL"||
       final.owner!="USER"||
       final.linkState!="UNLINKED"||
       !final.enabled||final.userBypass||
       final.controllerSuspended||final.domainSuspended||
       !live_node.active()||
       live_bindings.back().instance_id!=winner.exact_p2_instance_id)
        return {E::persisted_manual_p2_conflicts_with_live_product};
    const auto* frozen=std::get_if<
        dsp::StreamingTruePeakLimiterParameters>(
            &live_bindings.back().parameters);
    if(!frozen)
        return {E::persisted_manual_p2_conflicts_with_live_product};
    // Inspect the actual persisted ordinary DSP payload: checking
    // only the working C++ binding would allow a fake on-disk success.
    const auto encoded=dsp::encode_module_parameters_json(
        live_bindings.back().parameters);
    if(!encoded)return {E::persisted_manual_p2_conflicts_with_live_product};
    const auto payload=project::OpaqueJsonValue::parse(*encoded.value());
    if(!payload||
       final.parameters.canonical_utf8()!=payload.value()->canonical_utf8())
        return {E::persisted_manual_p2_conflicts_with_live_product};
    if(*frozen!=winner.winning_parameters)
        return {E::manual_p2_sound_conflict_requires_user_decision};
    for(const auto& linked:staged.chain.instances())
        if(linked.module_type_id()==
           "rgsml.dsp.streaming-true-peak-limiter")
            return {E::persisted_manual_p2_conflicts_with_live_product};
    return {E::keep_fixed_manual_p2};
}

// Safe M12 Product path for a winning topology that contains a
// protected MANUAL P2. In the ONLY non-conflicting case, preserve the
// exact original P2 sound and prepare an UNCOMMITTED artistic stage.
// A changed Ceiling/Drive is a frozen-law conflict: no second P2,
// no manual rewrite, no fake "MATERIALIZED" metadata or success receipt.
[[nodiscard]] inline core::Result<M12PreparedProjectTransaction>
prepare_exact_m12_project_transaction_with_p2_sound_guard(
    const project::ProjectSnapshot& current,
    const FrozenProjectLinkedCarrier& linked,
    const StagedRefLinkedSegment& accepted,
    M12ProjectTransactionExpected expected,
    const StagedExactP2Winner& exact_winner,
    const dsp::ProcessingChain& live_manual,
    std::span<const dsp::ModuleExecutionBinding> live_bindings)
{
    const auto guard=audit_exact_m12_p2_persisted_sound(
        accepted,exact_winner,live_manual,live_bindings,
        expected.exact_protected_manual_snapshot,
        expected.current_product_epoch_and_accepted_diff_attested);
    if(!guard.keeps_exact_frozen_sound()) {
        const char* error=guard.reason==
            P2PersistedSoundReason::
                manual_p2_sound_conflict_requires_user_decision
            ?"L3_M12_MANUAL_P2_CONFLICT_REQUIRES_USER_DECISION"
            :"L3_M12_P2_WINNER_OR_PERSISTED_MANUAL_STALE";
        return core::Result<M12PreparedProjectTransaction>::failure(
            core::Error{core::ErrorCode::InvalidState,error});
    }
    return prepare_exact_m12_project_transaction(
        current,linked,accepted,std::move(expected));
}
} // namespace rgsml::app::l3_automatch::m12_frozen
