#pragma once
// L3-M10 frozen coordinate search: one exact M09 P2 coordinate trial
// -> canonical Product graph preflight -> exact completed-cache key
// -> real FULL P2 render / M16 / M05-M07 / M08-M11 J.
// This is a worker-side adapter for the existing CoordinateRefiner
// and run_frozen_top2_refinements. It does NOT change frozen iterations
// (4 sweeps, minus-before-plus, 800 attempted), M17, or Project state.
#include "l3_m10_p2_bound_native_worker_score.hpp"
#include "l3_m10_p2_exact_cache_evaluator.hpp"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
struct RealP2LinkedCandidateGraph final {
    dsp::ProcessingChain exact_ref_linked;
    std::vector<dsp::ModuleExecutionBinding> exact_bindings;
    [[nodiscard]] constexpr bool authorizes_m12()const noexcept {
        return false;
    }
};

// The callbacks are AUTHENTICITY adapters, not alternative arithmetic:
// - decode is the original frozen M09 inverse coordinate transform;
// - key comes from the FULL immutable M07-M11 current generation;
// - preflight checks CURRENT Product/M09/M11 scope;
// - make_linked constructs exact typed REF_LINKED DSPs;
// - resolver returns the original real PREPARED/REFERENCE/candidate
//   native triples and immutable Phase6 feature-definition digests.
// No callback can hand in a precomputed 'J' to this function.
template<typename M09Decode,typename CanonicalKey,
         typename CurrentProductPreflight,typename MakeNativeLinked,
         typename ResolveOriginalTriple>
[[nodiscard]] inline Evaluation evaluate_real_m10_p2_native_cached(
    const P2NativeProductWorkerScope& owners,
    std::span<const Coordinate> frozen_inventory,
    std::span<const double> proposed_normalized_values,
    const ExactP2CoordinateNames& canonical_p2_fields,
    const dsp::ModuleInstanceId& exact_manual_p2_id,
    std::string candidate_ticket,
    ExactCompletedEvaluationCache& cache,
    bool allow_new_render_analysis_attempt,
    ProductNativeOnePassSpec full_native_spec,
    const M10P2ExactPrefix* optional_exact_upstream_cache,
    std::string exact_m07_phase6_generation,
    AuthoritativeCostInput current_m08_m11_constraints,
    double exact_m11_m16_safety_ceiling_db_tp,
    m05m06_real::SourceIdentity verified_candidate_owner,
    const OriginalPreparedReferenceNativeFacts*
        authentic_prepared_reference_native_pair,
    bool exact_file_lra_policy_attested,
    M09Decode&& m09_decode,
    CanonicalKey&& full_exact_execution_key,
    CurrentProductPreflight&& still_current_product_hard_preflight,
    MakeNativeLinked&& make_exact_linked_artistic,
    ResolveOriginalTriple&& resolve_original_native_triple,
    const std::atomic<bool>* cancellation=nullptr)
{
    if(!owners.current_qt_product_epoch_and_m09_m11_attested||
       !owners.current_owner.populated()||!owners.current_prepared||
       !owners.frozen_registry||
       !std::isfinite(exact_m11_m16_safety_ceiling_db_tp)||
       exact_m11_m16_safety_ceiling_db_tp>0.0||
       !verified_candidate_owner.full_track_owner_confirmed||
       !verified_candidate_owner.generation||
       verified_candidate_owner.resource_identity.empty()||
       !authentic_prepared_reference_native_pair||
       !original_pair_matches_product_owner(
           *authentic_prepared_reference_native_pair,
           owners.current_owner)||
       exact_m07_phase6_generation.empty()||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return {EvaluationKind::preflight_rejected,{}};

    // The prepared job is created during preflight, never inside the
    // cache-miss renderer. Thus malformed DSP/registry/ownership consumes
    // ZERO M10 attempted renders and cannot masquerade as a failed one.
    std::optional<M10P2RealNativeProductJob> prepared_product_job;
    return evaluate_m10_p2_with_exact_cache(
        frozen_inventory,proposed_normalized_values,
        canonical_p2_fields,owners.frozen_manual,
        owners.frozen_manual_bindings,exact_manual_p2_id,
        std::move(candidate_ticket),cache,
        allow_new_render_analysis_attempt,
        std::forward<M09Decode>(m09_decode),
        std::forward<CanonicalKey>(full_exact_execution_key),
        [&](const BoundM10P2Candidate& bound) {
            if(!bound.typed.still_matches(
                owners.frozen_manual,owners.frozen_manual_bindings)||
               !std::invoke(
                   still_current_product_hard_preflight,bound)||
               (cancellation&&
                   cancellation->load(std::memory_order_acquire)))
                return false;
            auto linked=std::invoke(make_exact_linked_artistic,bound);
            if(!linked)return false;
            auto job=M10P2RealNativeProductJob::create(
                owners.current_owner,owners.current_prepared,
                owners.frozen_registry,owners.frozen_manual,
                owners.frozen_manual_bindings,
                linked->exact_ref_linked,linked->exact_bindings,
                bound,owners.maximum_block_frames,true);
            if(!job)return false;
            prepared_product_job.emplace(std::move(*job.value()));
            return true;
        },
        [&](const BoundM10P2Candidate& bound)->FreshCompleteEvidence {
            if(!prepared_product_job||
               prepared_product_job->ticket_identity()!=
                   bound.typed.base.ticket_identity||
               (cancellation&&
                   cancellation->load(std::memory_order_acquire)))
                return {};
            const auto score=measure_and_score_m10_p2_full_native_candidate(
                *prepared_product_job,std::move(full_native_spec),
                optional_exact_upstream_cache,
                std::move(exact_m07_phase6_generation),true,
                std::move(current_m08_m11_constraints),
                exact_m11_m16_safety_ceiling_db_tp,
                std::move(verified_candidate_owner),
                authentic_prepared_reference_native_pair,
                exact_file_lra_policy_attested,
                std::forward<ResolveOriginalTriple>(
                    resolve_original_native_triple),cancellation);
            return score.as_exact_cache_evidence();
        });
}
} // namespace rgsml::app::l3_automatch::m10_frozen
