#pragma once
// L3-M07/M08/M10 exact authoritative measured-cost handoff.
// Consumes a FULL authenticated 27-slot paired feature population
// from an external real M05/M06 producer; never consumes the nine
// UI M21 metric probe or guesses missing canonical feature evidence.
// No render, persistence or M13 verification lives in this helper.
#include "l3_m08_frozen_targeted_population.hpp"
#include "l3_m10_frozen_total_cost.hpp"
#include "l3_m11_p03_protected_no_regression.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
namespace rgsml::app::l3_automatch::m10_frozen {
enum class AuthoritativeCostReason:std::uint8_t {
    measured,proven_no_correction_required,stale_or_missing_owner,
    missing_actual_candidate_or_protection,invalid_27_slot_population,
    target_cost_rejected,invalid_reference_or_complexity
};
struct AuthoritativeCostInput final {
    m08_frozen::AmountConfig amount;
    std::vector<m08_frozen::AuthenticatedPairSlot> exact_pair_slots;
    std::array<bool,4> m08_corrective_mask{};
    // M08 current implementation supplies DesiredTarget. When later
    // M11 constraint adjustment changes any coordinate, caller MUST
    // first supply the exact frozen optimizationTarget. Never relabel
    // raw Desired as optimizationTarget without an explicit attestation.
    bool optimization_target_equals_desired_attested{};
    bool exact_m05_m06_source_and_m07_dependency_current{};
    bool m11_fixed_and_protected_snapshot_current{};
    bool fresh_candidate_render_and_analysis_current{};
    bool hard_m11_and_independent_safety_preflight_passed{};
    bool m11_no_correction_full_evidence_attested{};
    // Ordered full M11 K, structurally matching M07 configured
    // component count. Actual transformed values must come from M11.
    std::vector<m11_frozen::ProtectedComponent> protected_full_k;
    bool m11_full_k_dependency_and_order_attested{};
    std::vector<ActiveReferenceCoordinate> active_coordinates;
    std::uint32_t active_owned_modules{};
    std::uint32_t enabled_artistic_bands{};
    std::vector<std::uint8_t> canonical_current_tuple;
    std::vector<std::uint8_t> exact_topology_evidence_identity;
};
struct AuthoritativeCostResult final {
    std::optional<MeasuredOrderKey> measured;
    std::optional<TargetCost> target_evidence;
    AuthoritativeCostReason reason{
        AuthoritativeCostReason::stale_or_missing_owner};
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
};
[[nodiscard]] inline AuthoritativeCostResult
score_exact_candidate_population(const AuthoritativeCostInput& input)
{
    if(!input.exact_m05_m06_source_and_m07_dependency_current||
       !input.m11_fixed_and_protected_snapshot_current||
       !input.optimization_target_equals_desired_attested)
        return {{},{},AuthoritativeCostReason::stale_or_missing_owner};
    // True no-correction is an independent M11 proof; it must not
    // arise by silently losing the NONE candidate during domain top8.
    bool any_corrective=false;
    for(bool bit:input.m08_corrective_mask)any_corrective|=bit;
    if(!any_corrective) {
        if(input.m11_no_correction_full_evidence_attested)
            return {{},{},
                AuthoritativeCostReason::proven_no_correction_required};
        return {{},{},AuthoritativeCostReason::stale_or_missing_owner};
    }
    if(!input.fresh_candidate_render_and_analysis_current||
       !input.hard_m11_and_independent_safety_preflight_passed)
        return {{},{},
            AuthoritativeCostReason::missing_actual_candidate_or_protection};
    auto population=m08_frozen::build_targeted_population(
        input.amount,input.exact_pair_slots,input.m08_corrective_mask);
    if(!population.value)
        return {{},{},
            AuthoritativeCostReason::invalid_27_slot_population};
    std::size_t expected_k_size=0U;
    for(const auto& slot:input.exact_pair_slots)
        expected_k_size+=slot.configured.size();
    if(expected_k_size!=input.protected_full_k.size())
        return {{},{},
            AuthoritativeCostReason::missing_actual_candidate_or_protection};
    const auto protected_check=
        m11_frozen::verify_frozen_protected_no_regression(
            input.protected_full_k,
            input.m11_full_k_dependency_and_order_attested,
            input.hard_m11_and_independent_safety_preflight_passed);
    if(!protected_check.is_admissible())
        return {{},{},
            AuthoritativeCostReason::missing_actual_candidate_or_protection};
    const auto measured=compute_frozen_target_cost(
        population.value->objective_input);
    if(!measured.valid_measured())
        return {{},measured,AuthoritativeCostReason::target_cost_rejected};
    const auto key=total_measured_cost(
        measured,input.active_coordinates,input.active_owned_modules,
        input.enabled_artistic_bands,input.canonical_current_tuple,
        input.exact_topology_evidence_identity);
    if(!key)
        return {{},measured,
            AuthoritativeCostReason::invalid_reference_or_complexity};
    return {key,measured,AuthoritativeCostReason::measured};
}
} // namespace rgsml::app::l3_automatch::m10_frozen
