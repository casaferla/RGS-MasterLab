#pragma once
// L3-M10 real P2 top-two refiner. Joins the FROZEN numerical solver,
// canonical M09 coordinate order, full Product immutable P/G source
// owners, EXACT cache, and ONE actual M10/P2 native scoring attempt.
// M09's topology interpreter / original M07-M11 feature-definition
// provenance remain independently owner-attested callbacks. The
// callbacks can supply DSP graphs and ORIGINAL feature definitions,
// NEVER a fabricated precomputed M10 score or M21 display metric.
// This numerical stage does not materialize M12 or certify M13.
#include "l3_m10_frozen_refinement_batch.hpp"
#include "l3_m10_p2_native_exact_cache_search.hpp"
#include "l3_m10_p2_canonical_trial_ticket.hpp"
#include "l3_m05m06_product_native_identity.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
struct RealP2Top2ProductEnvironment final {
    // All borrowed objects must outlive this complete worker run.
    const P2NativeProductWorkerScope& owners;
    const OriginalPreparedReferenceNativeFacts& native_prepared_reference;
    const ExactP2CoordinateNames& canonical_p2_fields;
    const dsp::ModuleInstanceId& original_manual_p2_id;
    ExactCompletedEvaluationCache& completed_cache;
    const ProductNativeOnePassSpec& exact_native_measurement_spec;
    const std::string& frozen_m07_phase6_generation;
    double exact_m11_final_tp_ceiling_db_tp{};
    bool exact_phase6_file_based_lra_policy_attested{};
    bool exact_current_m09_m10_m11_inventory_authority_attested{};
};

// M09Decode(ordinal, Coordinate, u) -> optional<double> EXACT physical.
// FullKey(ordinal, BoundM10P2Candidate) -> optional<ExactExecutionCacheKey>.
// HardPreflight(ordinal, BoundM10P2Candidate) -> bool.
// LinkedGraph(ordinal, BoundM10P2Candidate)
//                           -> optional<RealP2LinkedCandidateGraph>.
// CostConstraints(ordinal, proposed u[]) -> optional<AuthoritativeCostInput>.
// OriginalTriple(ordinal, measured real candidate P2 evidence)
//                           -> optional<P2OriginalRealNativeTriple>.
// Only the real native Product scorer constructs an authoritative J.
template<typename M09Decode,typename FullKey,
         typename HardPreflight,typename LinkedGraph,
         typename CostConstraints,typename OriginalTriple>
[[nodiscard]] inline RefinementBatchResult
run_real_m10_p2_frozen_top2_refinements(
    std::span<const SeedTrial> ordered_combined_seed_trials,
    std::span<const std::vector<Coordinate>> original_inventories,
    std::uint32_t attempted_before_seed_trials,
    const RealP2Top2ProductEnvironment& environment,
    M09Decode&& canonical_m09_inverse,
    FullKey&& exact_full_execution_key,
    HardPreflight&& still_current_product_hard_preflight,
    LinkedGraph&& create_real_linked_graph,
    CostConstraints&& current_m08_m11_constraints,
    OriginalTriple&& original_27_feature_source_definitions,
    const std::atomic<bool>* cancellation=nullptr)
{
    const auto invalid=[]{
        RefinementBatchResult result;
        result.reason=RefinementBatchReason::invalid_inventory;
        return result;
    };
    const auto& owner=environment.owners;
    if(!environment.exact_current_m09_m10_m11_inventory_authority_attested||
       !owner.current_qt_product_epoch_and_m09_m11_attested||
       !owner.current_owner.populated()||!owner.current_prepared||
       !owner.frozen_registry||
       !original_pair_matches_product_owner(
           environment.native_prepared_reference,owner.current_owner)||
       environment.frozen_m07_phase6_generation.empty()||
       !std::isfinite(environment.exact_m11_final_tp_ceiling_db_tp)||
       environment.exact_m11_final_tp_ceiling_db_tp>0.0||
       ordered_combined_seed_trials.size()!=original_inventories.size())
        return invalid();

    // M10::CoordinateRefiner::run sorts all incoming coordinates.
    // The Product bridge must supply u[] against that SAME order,
    // not against a formerly unsorted M09 topology inventory.
    std::vector<std::vector<Coordinate>> sorted_inventories;
    sorted_inventories.reserve(original_inventories.size());
    for(const auto& inventory:original_inventories) {
        auto sorted=m10_sort_exact_inventory(inventory);
        if(!sorted)return invalid();
        sorted_inventories.push_back(std::move(*sorted));
    }
    return run_frozen_top2_refinements(
        ordered_combined_seed_trials,sorted_inventories,
        attempted_before_seed_trials,true,
        [&](std::size_t ordinal,const std::vector<double>& proposed,
            bool allow_new)->Evaluation {
            // Lookup by exact M09 combined ordinal, NOT vector index.
            std::optional<std::size_t> index;
            for(std::size_t i=0;i<ordered_combined_seed_trials.size();++i)
                if(ordered_combined_seed_trials[i].
                    exact_combined_top32_ordinal==ordinal) {
                    if(index)return {EvaluationKind::preflight_rejected,{}};
                    index=i;
                }
            if(!index)return {EvaluationKind::preflight_rejected,{}};
            const auto& coords=sorted_inventories[*index];
            auto ticket=m10_exact_native_trial_ticket(
                ordinal,coords,proposed);
            if(!ticket)return {EvaluationKind::preflight_rejected,{}};
            auto m11=std::invoke(
                current_m08_m11_constraints,ordinal,proposed);
            if(!m11)return {EvaluationKind::preflight_rejected,{}};
            const auto candidate_owner=native_product_candidate_identity(
                owner.current_owner,*ticket);
            return evaluate_real_m10_p2_native_cached(
                owner,coords,proposed,
                environment.canonical_p2_fields,
                environment.original_manual_p2_id,
                std::move(*ticket),environment.completed_cache,
                allow_new,
                environment.exact_native_measurement_spec,
                nullptr /* conservative complete cold render */,
                environment.frozen_m07_phase6_generation,
                std::move(*m11),
                environment.exact_m11_final_tp_ceiling_db_tp,
                candidate_owner,
                &environment.native_prepared_reference,
                environment.exact_phase6_file_based_lra_policy_attested,
                [&](const Coordinate& coord,double u){
                    return std::invoke(canonical_m09_inverse,
                                       ordinal,coord,u);
                },
                [&](const BoundM10P2Candidate& bound){
                    return std::invoke(
                        exact_full_execution_key,ordinal,bound);
                },
                [&](const BoundM10P2Candidate& bound){
                    return std::invoke(
                        still_current_product_hard_preflight,
                        ordinal,bound);
                },
                [&](const BoundM10P2Candidate& bound){
                    return std::invoke(
                        create_real_linked_graph,ordinal,bound);
                },
                [&](const M10P2FullNativeEvidence& measured){
                    return std::invoke(
                        original_27_feature_source_definitions,
                        ordinal,measured);
                },cancellation);
        },cancellation);
}
} // namespace rgsml::app::l3_automatch::m10_frozen
