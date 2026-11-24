#pragma once
// L3-M10: real Product worker bridge from ONE M09 canonical coordinate
// candidate to ONE owned DSP P2 render + ORIGINAL native M07/M08/M11
// authoritative cost. The immutable Qt owner has already captured the
// PREPARED / REFERENCE epoch, while this worker never reads a QObject.
// No proxy score, no M17 Export TP Limiter, no M12 Project mutation.
#include "l3_m10_p2_authoritative_native_score.hpp"
#include "l3_m10_p2_canonical_coordinate_port.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m10_frozen {
// This is a borrowed, worker-lifetime view of an owner-verified snapshot.
// The caller MUST preserve the pointed-to immutable objects until every
// candidate worker has joined. No QApplication/QObject/GUI pointer here.
struct P2NativeProductWorkerScope final {
    const OfflineProductOwnerStamp& current_owner;
    const std::shared_ptr<const render::RenderResult>& current_prepared;
    const std::shared_ptr<const dsp::ModuleRegistry>& frozen_registry;
    const dsp::ProcessingChain& frozen_manual;
    std::span<const dsp::ModuleExecutionBinding> frozen_manual_bindings;
    core::FrameCount maximum_block_frames;
    bool current_qt_product_epoch_and_m09_m11_attested{};
};

// Late callback is deliberately limited to an ORIGINAL 3-way P/G/C
// native producer record: it may not provide an already-scored J.
// The actual candidate's ten numerical families are then replaced
// from the real PCM by measure_and_score_m10_p2_full_native_candidate.
template<typename ResolveOriginalThreeWayNative>
[[nodiscard]] inline P2NativeCostResult
measure_and_score_bound_m10_p2_product_worker(
    const P2NativeProductWorkerScope& current,
    const dsp::ProcessingChain& exact_ref_linked_artistic,
    std::span<const dsp::ModuleExecutionBinding> linked_bindings,
    const BoundM10P2Candidate& canonical_m09_m10_p2,
    ProductNativeOnePassSpec exact_m05m06_native_spec,
    const M10P2ExactPrefix* optional_exact_upstream_cache,
    std::string frozen_m07_phase6_dependency_generation,
    AuthoritativeCostInput protected_m08_m11_cost,
    double exact_m11_independent_tp_safety_ceiling_db_tp,
    m05m06_real::SourceIdentity exact_candidate_source_owner,
    const OriginalPreparedReferenceNativeFacts*
        real_prepared_reference_native_pair,
    bool exact_phase6_file_based_lra_policy_attested,
    ResolveOriginalThreeWayNative&& resolve_original_three_way_native,
    const std::atomic<bool>* cancellation=nullptr)
{
    const auto refused=[]{
        return P2NativeCostResult{
            P2NativeCostReason::stale_or_unattested_product,{},{},{}};
    };
    if(!current.current_qt_product_epoch_and_m09_m11_attested||
       !current.current_owner.populated()||!current.current_prepared||
       !current.frozen_registry||
       current.maximum_block_frames.value()<=0||
       !canonical_m09_m10_p2.typed.still_matches(
          current.frozen_manual,current.frozen_manual_bindings)||
       frozen_m07_phase6_dependency_generation.empty()||
       !authentic_product_candidate_identity(
           exact_candidate_source_owner,current.current_owner,
           canonical_m09_m10_p2.typed.base.ticket_identity)||
       !real_prepared_reference_native_pair||
       !original_pair_matches_product_owner(
           *real_prepared_reference_native_pair,current.current_owner)||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return refused();

    // One genuine M10 Product candidate: fixed MANUAL prefix,
    // REF_LINKED corrections, then the UNIQUE MANUAL P2 with typed
    // winning Ceiling/Drive. This fresh job may use an exact prefix
    // only if its complete owner+DSP dependency matches bit-for-bit.
    auto job=M10P2RealNativeProductJob::create(
        current.current_owner,current.current_prepared,
        current.frozen_registry,current.frozen_manual,
        current.frozen_manual_bindings,exact_ref_linked_artistic,
        linked_bindings,canonical_m09_m10_p2,
        current.maximum_block_frames,true);
    if(!job)return refused();
    return measure_and_score_m10_p2_full_native_candidate(
        *job.value(),std::move(exact_m05m06_native_spec),
        optional_exact_upstream_cache,
        std::move(frozen_m07_phase6_dependency_generation),true,
        std::move(protected_m08_m11_cost),
        exact_m11_independent_tp_safety_ceiling_db_tp,
        std::move(exact_candidate_source_owner),
        real_prepared_reference_native_pair,
        exact_phase6_file_based_lra_policy_attested,
        std::forward<ResolveOriginalThreeWayNative>(
            resolve_original_three_way_native),cancellation);
}
// Product path: only REF_LINKED parameters participate in the solver.
// The original user terminal is preserved whether active, bypassed or absent.
template<class ResolveOriginalThreeWayNative>
[[nodiscard]] inline P2NativeCostResult
measure_and_score_preserved_terminal_product_worker(
    const P2NativeProductWorkerScope& current,
    const dsp::ProcessingChain& exact_ref_linked_artistic,
    std::span<const dsp::ModuleExecutionBinding> linked_bindings,
    std::string exact_candidate_ticket,
    ProductNativeOnePassSpec exact_m05m06_native_spec,
    std::string frozen_m07_phase6_dependency_generation,
    AuthoritativeCostInput protected_m08_m11_cost,
    double exact_m11_independent_tp_safety_ceiling_db_tp,
    m05m06_real::SourceIdentity exact_candidate_source_owner,
    const OriginalPreparedReferenceNativeFacts* real_prepared_reference_native_pair,
    bool exact_phase6_file_based_lra_policy_attested,
    ResolveOriginalThreeWayNative&& resolve_original_three_way_native,
    const std::atomic<bool>* cancellation=nullptr)
{
    if(!current.current_qt_product_epoch_and_m09_m11_attested||
       !protected_m08_m11_cost.independently_measured_fixed||
       !current.current_owner.populated()||!current.current_prepared||
       !current.frozen_registry||exact_candidate_ticket.empty()||
       frozen_m07_phase6_dependency_generation.empty()||
       !authentic_product_candidate_identity(exact_candidate_source_owner,
           current.current_owner,exact_candidate_ticket)||
       !real_prepared_reference_native_pair||
       !original_pair_matches_product_owner(*real_prepared_reference_native_pair,
                                            current.current_owner)||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return {P2NativeCostReason::stale_or_unattested_product,{},{},{}};
    auto job=M10P2RealNativeProductJob::create_preserving_user_terminal(
        current.current_owner,current.current_prepared,current.frozen_registry,
        current.frozen_manual,current.frozen_manual_bindings,exact_ref_linked_artistic,
        linked_bindings,std::move(exact_candidate_ticket),
        current.maximum_block_frames,true);
    if(!job)return {P2NativeCostReason::stale_or_unattested_product,{},{},{}};
    return measure_and_score_m10_p2_full_native_candidate(
        *job.value(),std::move(exact_m05m06_native_spec),nullptr,
        std::move(frozen_m07_phase6_dependency_generation),true,
        std::move(protected_m08_m11_cost),exact_m11_independent_tp_safety_ceiling_db_tp,
        std::move(exact_candidate_source_owner),real_prepared_reference_native_pair,
        exact_phase6_file_based_lra_policy_attested,
        std::forward<ResolveOriginalThreeWayNative>(resolve_original_three_way_native),
        cancellation);
}
} // namespace rgsml::app::l3_automatch::m10_frozen
