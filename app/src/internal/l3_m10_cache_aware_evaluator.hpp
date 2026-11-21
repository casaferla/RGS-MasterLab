#pragma once
// L3-M10 callback adapter: authentic exact cache hits vs new real
// render+full-analysis attempts. This does NOT call Product or fabricate
// a canonical execution key. The supplied renderer must be a real
// owner-authorized typed job and all hard gates must be complete.
#include "l3_m10_exact_completed_result_cache.hpp"
#include <optional>
#include <utility>

namespace rgsml::app::l3_automatch::m10_frozen {
struct FreshCompleteEvidence final {
    std::optional<MeasuredOrderKey> measured;
    bool full_current_render_analysis_hard_gates_attested{};
};
template<typename FreshExecute>
[[nodiscard]] inline Evaluation evaluate_with_exact_completed_cache(
    ExactCompletedEvaluationCache& cache,
    const ExactExecutionCacheKey& exact_candidate,
    bool exact_product_preflight_passed,
    bool allow_new_attempt,
    FreshExecute&& physically_render_analyze_and_score)
{
    if(!exact_product_preflight_passed||
       !cache.accepts_exact_current_key(exact_candidate))
        return {EvaluationKind::preflight_rejected,{}};
    if(const auto complete=cache.find(exact_candidate))
        return {EvaluationKind::completed_cache_hit,*complete};
    if(!allow_new_attempt)
        return {EvaluationKind::needs_new_attempt,{}};
    // Calling this is ONE attempted full Product render/analyzer. A
    // failed/invalid result consumes a slot, never enters the cache.
    const auto obtained=physically_render_analyze_and_score();
    if(!obtained.full_current_render_analysis_hard_gates_attested||
       !obtained.measured||!valid_key(*obtained.measured))
        return {EvaluationKind::attempted_failure,{}};
    const auto inserted=cache.insert_completed(
        exact_candidate,*obtained.measured,true);
    if(inserted==CacheInsert::deterministic_result_collision||
       inserted==CacheInsert::generation_poisoned||
       inserted==CacheInsert::invalid_or_unattested)
        return {EvaluationKind::attempted_failure,{}};
    // A valid completed render remains completed even when the optional
    // bounded cache is full. Cache capacity is NOT an M10 hard safety
    // veto, nor a reason to erase actually measured score evidence.
    return {EvaluationKind::attempted_complete,*obtained.measured};
}
} // namespace rgsml::app::l3_automatch::m10_frozen
