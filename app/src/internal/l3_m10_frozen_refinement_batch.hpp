#pragma once
// L3-M10 ACCEPTED_FROZEN numerical orchestration of measured top2 seeds
// through the existing canonical deterministic CoordinateRefiner.
// All actual validated DSP binding, M05/M06 render/analysis, M11 safety,
// exact completed cache and Product owner staleness checks remain at the
// supplied evaluator boundary; no proxy, M21 scalar or synthetic audio.
#include "l3_m10_measured_seed_shortlist.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
enum class RefinementBatchReason:std::uint8_t {
    complete,budget_exhausted,cancelled,invalid_inventory,
    no_complete_seed_or_stale_source,invalid_evaluation
};
struct RefinedSeed final {
    std::size_t original_seed_trial_index{};
    std::size_t exact_combined_ordinal{};
    Outcome numerical;
};
struct RefinementBatchResult final {
    RefinementBatchReason reason{
        RefinementBatchReason::no_complete_seed_or_stale_source};
    std::vector<RefinedSeed> candidate_refinements;
    std::optional<std::size_t> winning_refinement_index;
    std::uint32_t total_attempted{};
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
};
// The two inventories are FULL exact owned active reference coordinates,
// indexed by original seed-trial ordinal. Caller must attest corresponding
// immutable M09 semanticNode/field IDs, bounds/transforms and M11 fixed
// reference separately before allowing any real evaluator attempt.
template<typename Evaluate>
[[nodiscard]] inline RefinementBatchResult run_frozen_top2_refinements(
    std::span<const SeedTrial> ordered_combined_seed_trials,
    std::span<const std::vector<Coordinate>> full_exact_active_inventories,
    std::uint32_t attempts_before_seed_trials,
    bool owner_current_and_inventory_authority_attested,
    Evaluate&& evaluate_typed_dsp_and_fresh_m05m06,
    const std::atomic<bool>* cancellation=nullptr)
{
    RefinementBatchResult result;
    if(!owner_current_and_inventory_authority_attested||
       full_exact_active_inventories.size()!=
           ordered_combined_seed_trials.size()) {
        result.reason=RefinementBatchReason::invalid_inventory;
        return result;
    }
    const auto shortlist=select_measured_seed_top2(
        ordered_combined_seed_trials,attempts_before_seed_trials);
    result.total_attempted=shortlist.attempted_total;
    if(shortlist.reason!=ShortlistReason::ready) {
        result.reason=RefinementBatchReason::
            no_complete_seed_or_stale_source;
        return result;
    }
    for(const auto index:shortlist.best_trial_indices) {
        if(cancellation&&cancellation->load(
           std::memory_order_acquire)) {
            result.reason=RefinementBatchReason::cancelled;
            return result;
        }
        const auto& seed=ordered_combined_seed_trials[index];
        const auto ordinal=seed.exact_combined_top32_ordinal;
        // Never rebase regularization or seed score to another topology:
        // the existing refiner retains each M10 immutable seed anchor.
        auto refined=CoordinateRefiner::run(
            full_exact_active_inventories[index],
            *seed.frozen_measured,result.total_attempted,
            [&](const std::vector<double>& u,bool allow_attempt) {
                return evaluate_typed_dsp_and_fresh_m05m06(
                    ordinal,u,allow_attempt);
            },cancellation);
        if(!refined) {
            result.reason=RefinementBatchReason::invalid_evaluation;
            result.winning_refinement_index.reset();
            return result;
        }
        result.total_attempted=refined->attempted_total;
        const auto state=refined->end;
        result.candidate_refinements.push_back(
            {index,ordinal,std::move(*refined)});
        if(state==End::cancelled) {
            result.reason=RefinementBatchReason::cancelled;
            result.winning_refinement_index.reset();
            return result;
        }
        if(state==End::budget_exhausted)
            result.reason=RefinementBatchReason::budget_exhausted;
        const auto new_index=result.candidate_refinements.size()-1U;
        if(!result.winning_refinement_index||
           compare_measured(
               result.candidate_refinements[new_index].numerical.winner,
               result.candidate_refinements[
                   *result.winning_refinement_index].numerical.winner)<0)
            result.winning_refinement_index=new_index;
    }
    if(result.reason!=RefinementBatchReason::budget_exhausted)
        result.reason=RefinementBatchReason::complete;
    // Winner remains a measured SOLVER-stage result, not M13 verified,
    // nor an M12 REF_LINKED Product candidate for publication.
    return result;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
