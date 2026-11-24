#pragma once
// L3-M07/M08/M10 exact authoritative measured-cost handoff.
// Consumes a FULL authenticated 27-slot paired feature population
// from an external real M05/M06 producer; never consumes the nine
// UI M21 metric probe or guesses missing canonical feature evidence.
// No render, persistence or M13 verification lives in this helper.
#include "l3_m08_frozen_targeted_population.hpp"
#include "l3_m10_frozen_total_cost.hpp"
#include "l3_m10_native_prediction_snapshot.hpp"
#include <memory>
#include "l3_m11_p03_protected_no_regression.hpp"
#include "l3_m11_native_fixed_population.hpp"
#include "l3_m11_native_stereo_hard_safety.hpp"
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
    // When current Product scope is stereo, independent raw M20 and
    // coherent low-end M11 evidence is mandatory, NOT a fourth artistic
    // domain or a replacement for protected full K. Legacy mono inputs
    // remain unchanged unless an explicit native record is attached.
    bool native_stereo_raw_safety_required_by_current_owner{};
    std::optional<m11_frozen::NativeStereoRawSafety>
        native_stereo_raw_safety;
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
    std::optional<std::array<bool,4>> m08_reporting_requested_domains{};
    std::string native_candidate_resource_identity{};
    std::uint64_t native_candidate_generation{};
    std::string native_candidate_ticket{};
    std::string native_m07_generation{};
    std::vector<std::uint8_t> native_linked_graph_content{};
    std::vector<std::uint8_t> native_execution_graph_content{};
    std::shared_ptr<const P2OriginalRealNativeTriple> native_original_definition_authority{};
    std::vector<std::uint8_t> native_definition_authority_content{};
    std::optional<m11_frozen::NativeFixedPopulation> independently_measured_fixed;
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
    if(input.native_stereo_raw_safety_required_by_current_owner&&
       !input.native_stereo_raw_safety)
        return {{},{},
            AuthoritativeCostReason::missing_actual_candidate_or_protection};
    if(input.native_stereo_raw_safety&&
       !m11_frozen::verify_m11_native_stereo_hard_safety(
           *input.native_stereo_raw_safety).passes_hard_gate())
        return {{},{},
            AuthoritativeCostReason::missing_actual_candidate_or_protection};
    auto population=m08_frozen::build_targeted_population(
        input.amount,input.exact_pair_slots,input.m08_corrective_mask);
    if(!population.value)
        return {{},{},
            AuthoritativeCostReason::invalid_27_slot_population};
    std::vector<m11_frozen::ProtectedComponent> bound_k=input.protected_full_k;
    if(input.independently_measured_fixed) {
        auto bound=m11_frozen::bind_native_fixed_to_candidate(
            *input.independently_measured_fixed,input.exact_pair_slots,input.amount);
        if(!bound)return {{},{},AuthoritativeCostReason::missing_actual_candidate_or_protection};
        bound_k=std::move(*bound);
    }
    std::vector<m11_frozen::ProtectedComponent> required_k;
    std::size_t expected_k_size=0U;
    for(const auto& slot:input.exact_pair_slots)
        expected_k_size+=slot.configured.size();
    if(expected_k_size!=bound_k.size())
        return {{},{},
            AuthoritativeCostReason::missing_actual_candidate_or_protection};
    // Cardinality alone does not bind the protected K to the scored
    // actual candidate. Compare every canonical ordered transformed
    // P/G/C value against its authenticated M07/M08 source slot.
    // In particular, a second fabricated benign K must never be
    // allowed to vouch for an unrelated regressing candidate.
    std::size_t canonical_index=0U;
    std::array<std::size_t,4> domain_ordinals{};
    for(std::size_t i=0;i<input.exact_pair_slots.size();++i) {
        const auto& slot=input.exact_pair_slots[i];
        const auto d=static_cast<std::size_t>(m07_frozen::kBindings[i].domain);
        for(const auto& feature:slot.configured) {
            const auto& guard=bound_k[canonical_index++];
            auto& objective=population.value->objective_input[d].configured[domain_ordinals[d]++];
            if(input.independently_measured_fixed) {
                if(!guard.required_in_frozen_k)continue;
                objective.protected_from_correction=guard.protected_by_m11_mask;
            }
            if(!feature.prepared.z||!feature.reference.z||
               !feature.current_candidate_z||
               !guard.exact_current||!guard.required_in_frozen_k||
               guard.prepared_z!=*feature.prepared.z||
               guard.gold_z!=*feature.reference.z||
               guard.candidate_z!=*feature.current_candidate_z)
                return {{},{},
                    AuthoritativeCostReason::
                        missing_actual_candidate_or_protection};
            required_k.push_back(guard);
        }
    }
    const auto protected_check=
        m11_frozen::verify_frozen_protected_no_regression(
            required_k,
            input.m11_full_k_dependency_and_order_attested,
            input.hard_m11_and_independent_safety_preflight_passed);
    if(!protected_check.is_admissible())
        return {{},{},
            AuthoritativeCostReason::missing_actual_candidate_or_protection};
    const auto measured=compute_frozen_target_cost(
        population.value->objective_input);
    if(!measured.valid_measured())
        return {{},measured,AuthoritativeCostReason::target_cost_rejected};
    auto key=total_measured_cost(
        measured,input.active_coordinates,input.active_owned_modules,
        input.enabled_artistic_bands,input.canonical_current_tuple,
        input.exact_topology_evidence_identity);
    if(!key)
        return {{},measured,
            AuthoritativeCostReason::invalid_reference_or_complexity};
    key->native_prediction=std::make_shared<const NativePredictionSnapshot>(
        NativePredictionSnapshot{input.exact_pair_slots,std::move(bound_k),
                                 input.m08_reporting_requested_domains,
                                 input.native_candidate_resource_identity,
                                 input.native_candidate_generation,
                                 input.native_candidate_ticket,
                                 input.native_m07_generation,
                                 input.native_linked_graph_content,
                                 input.native_execution_graph_content,
                                 input.native_original_definition_authority,
                                 input.native_definition_authority_content});
    return {std::move(key),measured,AuthoritativeCostReason::measured};
}
} // namespace rgsml::app::l3_automatch::m10_frozen
