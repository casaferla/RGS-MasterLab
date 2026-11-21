#pragma once
// L3-M10 P04/P05: measured seed-render shortlist of at most two.
// The candidate list is the already selected combined-top32 stream;
// this adapter never substitutes pre-render proxy, measured M21-only
// UI cells, noncurrent cache entries, or a failed hard constraint.
#include "l3_m10_frozen_coordinate_refiner.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace rgsml::app::l3_automatch::m10_frozen {
enum class SeedTrialKind:std::uint8_t {
    attempted_complete,completed_cache_hit,
    attempted_failure,preflight_rejected,not_evaluated
};
struct SeedTrial final {
    std::size_t exact_combined_top32_ordinal{};
    SeedTrialKind kind{SeedTrialKind::not_evaluated};
    std::optional<MeasuredOrderKey> frozen_measured;
    bool exact_current_candidate_and_dependency_attested{};
    bool all_hard_constraints_passed{};
};
enum class ShortlistReason:std::uint8_t {
    ready,no_valid_complete_candidate,unready_missing_trial,
    invalid_selected_identity_or_result,budget_exceeded
};
struct MeasuredShortlist final {
    ShortlistReason reason{ShortlistReason::unready_missing_trial};
    std::vector<std::size_t> best_trial_indices;
    std::uint32_t attempted_total{};
    std::uint32_t completed_cache_hits{};
    std::uint32_t attempted_failures{};
    std::uint32_t preflight_rejections{};
    [[nodiscard]] constexpr bool authorizes_publish_or_verify()
        const noexcept {return false;}
};
[[nodiscard]] inline MeasuredShortlist select_measured_seed_top2(
    std::span<const SeedTrial> trials,
    std::uint32_t attempted_before)
{
    MeasuredShortlist result;
    result.attempted_total=attempted_before;
    if(trials.empty())return result;
    if(trials.size()>32U||attempted_before>kMaxRenderAnalyzeAttempts) {
        result.reason=ShortlistReason::budget_exceeded;
        return result;
    }
    std::vector<std::size_t> valid;
    valid.reserve(trials.size());
    for(std::size_t i=0;i<trials.size();++i) {
        const auto& trial=trials[i];
        // One M09 selected combined key per candidate and stable ordinal.
        for(std::size_t j=0;j<i;++j)
            if(trials[j].exact_combined_top32_ordinal==
               trial.exact_combined_top32_ordinal) {
                result.reason=ShortlistReason::invalid_selected_identity_or_result;
                return result;
            }
        if(trial.kind==SeedTrialKind::not_evaluated) {
            result.reason=ShortlistReason::unready_missing_trial;
            return result;
        }
        const bool measured_kind=
            trial.kind==SeedTrialKind::attempted_complete||
            trial.kind==SeedTrialKind::completed_cache_hit;
        if(measured_kind) {
            if(!trial.frozen_measured||
               !valid_key(*trial.frozen_measured)||
               !trial.exact_current_candidate_and_dependency_attested||
               !trial.all_hard_constraints_passed) {
                result.reason=ShortlistReason::invalid_selected_identity_or_result;
                return result;
            }
            valid.push_back(i);
        } else if(trial.frozen_measured) {
            result.reason=ShortlistReason::invalid_selected_identity_or_result;
            return result;
        }
        if(trial.kind==SeedTrialKind::attempted_complete||
           trial.kind==SeedTrialKind::attempted_failure) {
            if(result.attempted_total==kMaxRenderAnalyzeAttempts) {
                result.reason=ShortlistReason::budget_exceeded;
                return result;
            }
            ++result.attempted_total;
            if(trial.kind==SeedTrialKind::attempted_failure)
                ++result.attempted_failures;
        } else if(trial.kind==SeedTrialKind::completed_cache_hit) {
            ++result.completed_cache_hits;
        } else if(trial.kind==SeedTrialKind::preflight_rejected) {
            ++result.preflight_rejections;
        }
    }
    if(valid.empty()) {
        result.reason=ShortlistReason::no_valid_complete_candidate;
        return result;
    }
    std::stable_sort(valid.begin(),valid.end(),
        [&](std::size_t a,std::size_t b) {
            return compare_measured(*trials[a].frozen_measured,
                                    *trials[b].frozen_measured)<0;
        });
    const auto n=std::min(std::size_t{2},valid.size());
    result.best_trial_indices.assign(valid.begin(),valid.begin()+n);
    result.reason=ShortlistReason::ready;
    return result;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
