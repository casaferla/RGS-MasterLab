#pragma once
// Existing frozen top2 solver with an actual preserved-terminal native
// evaluator. M09 remains responsible for decoding every artistic graph;
// no callback may manufacture a measured J.
#include "l3_m10_preserved_native_search.hpp"
#include "l3_m10_frozen_refinement_batch.hpp"

namespace rgsml::app::l3_automatch::m10_frozen {
struct PreservedNativeTrial final {
    PreservedNativeGraph graph;
    ExactExecutionCacheKey key;
    AuthoritativeCostInput constraints;
    bool exact_current_typed_graph_preflight_passed{};
};
template<class BuildTypedTrial,class ResolveOriginalTriple>
[[nodiscard]] inline RefinementBatchResult
run_preserved_native_top2(
    std::span<const SeedTrial> seeds,
    std::span<const std::vector<Coordinate>> inventories,
    std::uint32_t attempts_before_seeds,
    const PreservedNativeSearchEnvironment& env,
    BuildTypedTrial&& build_trial,
    ResolveOriginalTriple&& resolve_original,
    const std::atomic<bool>* cancellation=nullptr)
{
    RefinementBatchResult invalid;
    invalid.reason=RefinementBatchReason::invalid_inventory;
    if(seeds.size()!=inventories.size())return invalid;
    std::vector<std::vector<Coordinate>> sorted;
    sorted.reserve(inventories.size());
    for(const auto& inventory:inventories) {
        auto canonical=m10_sort_exact_inventory(inventory);
        if(!canonical)return invalid;
        sorted.push_back(std::move(*canonical));
    }
    return run_frozen_top2_refinements(
        seeds,sorted,attempts_before_seeds,
        env.owners.current_qt_product_epoch_and_m09_m11_attested,
        [&](std::size_t ordinal,const std::vector<double>& u,bool allow_new) {
            const auto refused=[](){
                return Evaluation{EvaluationKind::preflight_rejected,{}};
            };
            const auto seed=std::find_if(seeds.begin(),seeds.end(),
                [ordinal](const auto& item){
                    return item.exact_combined_top32_ordinal==ordinal;
                });
            if(seed==seeds.end())return refused();
            const auto index=static_cast<std::size_t>(seed-seeds.begin());
            auto ticket=m10_exact_native_trial_ticket(ordinal,sorted[index],u);
            if(!ticket)return refused();
            auto trial=std::invoke(build_trial,ordinal,sorted[index],u);
            if(!trial)return refused();
            return evaluate_preserved_native_cached(env,trial->graph,std::move(*ticket),
                std::move(trial->key),std::move(trial->constraints),
                trial->exact_current_typed_graph_preflight_passed,allow_new,
                [&](const M10P2FullNativeEvidence& measured){
                    return std::invoke(resolve_original,ordinal,measured);
                },cancellation);
        },cancellation);
}
} // namespace rgsml::app::l3_automatch::m10_frozen
