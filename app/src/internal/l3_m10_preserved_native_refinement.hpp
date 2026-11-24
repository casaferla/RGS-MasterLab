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
// Execute the selected combined seeds using exactly the same evaluator
// that will refine them. No unmeasured seed may enter the top-two list.
template<class BuildTypedTrial,class ResolveOriginalTriple>
[[nodiscard]] inline RefinementBatchResult
run_preserved_native_seed_and_refinement(
    std::span<const std::size_t> ordered_ordinals,
    std::span<const std::vector<Coordinate>> inventories,
    std::uint32_t attempts_before_seeds,
    const PreservedNativeSearchEnvironment& env,
    BuildTypedTrial&& build_trial,
    ResolveOriginalTriple&& resolve_original,
    const std::atomic<bool>* cancellation=nullptr)
{
    RefinementBatchResult result;
    result.reason=RefinementBatchReason::invalid_inventory;
    result.total_attempted=attempts_before_seeds;
    if(ordered_ordinals.empty()||ordered_ordinals.size()>32U||
       ordered_ordinals.size()!=inventories.size()||
       attempts_before_seeds>kMaxRenderAnalyzeAttempts)
        return result;
    std::vector<std::vector<Coordinate>> sorted;
    sorted.reserve(inventories.size());
    for(std::size_t i=0;i<inventories.size();++i) {
        if(std::find(ordered_ordinals.begin(),ordered_ordinals.begin()+i,
                     ordered_ordinals[i])!=ordered_ordinals.begin()+i)return result;
        auto inventory=m10_sort_exact_inventory(inventories[i]);
        if(!inventory)return result;
        sorted.push_back(std::move(*inventory));
    }
    std::vector<SeedTrial> seeds;
    seeds.reserve(sorted.size());
    for(std::size_t i=0;i<sorted.size();++i) {
        if(cancellation&&cancellation->load(std::memory_order_acquire)) {
            result.reason=RefinementBatchReason::cancelled;
            return result;
        }
        const auto ordinal=ordered_ordinals[i];
        std::vector<double> u;
        u.reserve(sorted[i].size());
        for(const auto& c:sorted[i])u.push_back(c.normalized_u);
        auto ticket=m10_exact_native_trial_ticket(ordinal,sorted[i],u);
        auto trial=std::invoke(build_trial,ordinal,sorted[i],u);
        Evaluation event;
        if(ticket&&trial)
            event=evaluate_preserved_native_cached(
                env,trial->graph,std::move(*ticket),std::move(trial->key),
                std::move(trial->constraints),trial->exact_current_typed_graph_preflight_passed,
                result.total_attempted<kMaxRenderAnalyzeAttempts,
                [&](const M10P2FullNativeEvidence& measured){
                    return std::invoke(resolve_original,ordinal,measured);
                },cancellation);
        SeedTrial seed;
        seed.exact_combined_top32_ordinal=ordinal;
        switch(event.kind) {
        case EvaluationKind::attempted_complete:
            ++result.total_attempted;
            seed.kind=SeedTrialKind::attempted_complete;break;
        case EvaluationKind::attempted_failure:
            ++result.total_attempted;
            seed.kind=SeedTrialKind::attempted_failure;break;
        case EvaluationKind::completed_cache_hit:
            seed.kind=SeedTrialKind::completed_cache_hit;break;
        case EvaluationKind::preflight_rejected:
            seed.kind=SeedTrialKind::preflight_rejected;break;
        case EvaluationKind::needs_new_attempt:
            result.reason=RefinementBatchReason::seed_attempt_budget_exceeded;
            return result;
        }
        if(seed.kind==SeedTrialKind::attempted_complete||
           seed.kind==SeedTrialKind::completed_cache_hit) {
            if(!event.measured||!event.measured->native_prediction||
               !valid_key(*event.measured)) {
                result.reason=RefinementBatchReason::invalid_seed_trial;
                return result;
            }
            seed.frozen_measured=std::move(event.measured);
            seed.exact_current_candidate_and_dependency_attested=true;
            seed.all_hard_constraints_passed=true;
        }
        seeds.push_back(std::move(seed));
    }
    return run_preserved_native_top2(
        seeds,sorted,attempts_before_seeds,env,build_trial,resolve_original,cancellation);
}
} // namespace rgsml::app::l3_automatch::m10_frozen
