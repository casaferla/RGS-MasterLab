#pragma once
// L3-M11 P03 ACCEPTED_FROZEN: exact transformed-coordinate protected
// component no-regression candidate hard gate. No per-domain averaging,
// soft penalty, 1e-9 tolerance or stale/partial-K shortcut.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rgsml::app::l3_automatch::m11_frozen {
struct ProtectedComponent final {
    // Canonical ordered K from M05/M06, with exact source/target/Fixed
    // analysis and endpoint validity attested by the Product owner.
    bool required_in_frozen_k{};
    bool protected_by_m11_mask{};
    bool exact_current{};
    double prepared_z{};
    double gold_z{};
    double fixed_z{};
    double candidate_z{};
    double frozen_deadband{};
};
enum class NoRegressionReason:std::uint8_t {
    pass,missing_canonical_population_or_dependency,
    incomplete_required_component,invalid_numeric,
    protected_component_regressed
};
struct NoRegressionResult final {
    NoRegressionReason reason{
        NoRegressionReason::missing_canonical_population_or_dependency};
    std::optional<std::size_t> first_bad_canonical_index;
    std::size_t protected_count{};
    [[nodiscard]] constexpr bool is_admissible() const noexcept {
        return reason==NoRegressionReason::pass;
    }
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept {return false;}
};
[[nodiscard]] inline NoRegressionResult
verify_frozen_protected_no_regression(
    std::span<const ProtectedComponent> canonical_k,
    bool exact_complete_k_and_m11_fixed_mask_current,
    bool endpoint_and_independent_safety_eligibility_attested)
{
    if(!exact_complete_k_and_m11_fixed_mask_current||
       !endpoint_and_independent_safety_eligibility_attested||
       canonical_k.empty())
        return {};
    NoRegressionResult result;
    for(std::size_t i=0;i<canonical_k.size();++i) {
        const auto& c=canonical_k[i];
        if(!c.required_in_frozen_k||!c.exact_current)
            return {NoRegressionReason::incomplete_required_component,i,
                    result.protected_count};
        if(!std::isfinite(c.prepared_z)||
           !std::isfinite(c.gold_z)||
           !std::isfinite(c.fixed_z)||
           !std::isfinite(c.candidate_z)||
           !std::isfinite(c.frozen_deadband)||
           c.frozen_deadband<0.0)
            return {NoRegressionReason::invalid_numeric,i,
                    result.protected_count};
        if(!c.protected_by_m11_mask)continue;
        ++result.protected_count;
        const double progress=c.gold_z-c.prepared_z;
        const double distance_gold=c.candidate_z-c.gold_z;
        if(!std::isfinite(progress)||!std::isfinite(distance_gold))
            return {NoRegressionReason::invalid_numeric,i,
                    result.protected_count};
        bool pass=false;
        if(std::abs(progress)<=c.frozen_deadband) {
            // Initially already close: never allow leaving deadband.
            pass=std::abs(distance_gold)<=c.frozen_deadband;
        } else {
            const double sign=progress>0.0?1.0:-1.0;
            const double fixed_distance=c.fixed_z-c.gold_z;
            const double directional_floor=
                std::min(sign*c.fixed_z,sign*c.gold_z)-
                c.frozen_deadband;
            const double remaining=std::abs(fixed_distance)+
                                   c.frozen_deadband;
            if(!std::isfinite(fixed_distance)||
               !std::isfinite(directional_floor)||
               !std::isfinite(remaining))
                return {NoRegressionReason::invalid_numeric,i,
                        result.protected_count};
            pass=sign*c.candidate_z>=directional_floor &&
                 std::abs(distance_gold)<=remaining;
        }
        if(!pass)
            return {NoRegressionReason::protected_component_regressed,i,
                    result.protected_count};
    }
    result.reason=NoRegressionReason::pass;
    return result;
}
} // namespace rgsml::app::l3_automatch::m11_frozen
