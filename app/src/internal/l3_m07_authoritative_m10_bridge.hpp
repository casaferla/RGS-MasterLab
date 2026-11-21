#pragma once
// L3-M07 -> M08/M10 exact 27-slot adapter. It never measures audio,
// normalizes missing feature mass or guesses an optimizer target.
// A source resolver MUST attest every M05/M06 feature definition,
// content identity and M07 canonical binding before ANY M10 evaluation.
#include "l3_m07_frozen_binding_projection.hpp"
#include "l3_m10_frozen_target_objective.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rgsml::app::l3_automatch::m07_frozen {
struct SourceComponent final {
    bool valid_current{false}; // exact Phase6 feature evidence, NOT UI
    double paired_support{};   // M05/M06 paired native support
    std::optional<double> candidate_z; // actual canonical observed z
    std::optional<double> optimization_target_z; // M08, NOT raw Gold
    bool required_candidate_evidence{true};
};
struct ResolvedSlot final {
    std::string binding_id;
    std::string resolved_source_feature_id;
    // Non-empty exact feature-definition artifact content identity,
    // with authenticated per-feature algorithm/configuration dependency.
    std::string exact_feature_dependency;
    bool feature_authority_attested{false};
    std::vector<SourceComponent> configured_components;
};
enum class BridgeFailure :std::uint8_t {
    none, wrong_slot_count, binding_order_or_identity_mismatch,
    exact_feature_authority_unresolved, invalid_component_population,
    illegal_support_or_numeric, incompatible_feature_variant
};
struct Populations final {
    std::array<m10_frozen::DomainPopulation,4> domain;
    // This is only an input to the independent M10 evaluator. The
    // subsequent hard stereo/TP constraints, M11/12 materialization,
    // full fresh M13 verification remain separate.
    [[nodiscard]] constexpr bool authorizes_master_publication()
        const noexcept {return false;}
};
struct BridgeResult final {
    std::optional<Populations> populations;
    BridgeFailure failure{BridgeFailure::wrong_slot_count};
};
[[nodiscard]] inline BridgeResult to_m10_population(
    const std::vector<ResolvedSlot>& slots,
    const std::array<bool,4>& m08_requested_corrective)
{
    const auto reject=[](BridgeFailure reason)->BridgeResult{
        return {{},reason};
    };
    if(slots.size()!=kBindings.size())
        return reject(BridgeFailure::wrong_slot_count);
    Populations out;
    for(std::size_t d=0;d<4;++d) {
        out.domain[d].domain=
            static_cast<m10_frozen::Domain>(d);
        out.domain[d].requested_corrective=m08_requested_corrective[d];
    }
    for(std::size_t i=0;i<kBindings.size();++i) {
        const auto& frozen=kBindings[i];
        const auto& source=slots[i];
        if(source.binding_id!=frozen.binding_id)
            return reject(BridgeFailure::binding_order_or_identity_mismatch);
        if(source.resolved_source_feature_id!=frozen.source_feature_id &&
           (frozen.exclusive_fallback_feature_id.empty() ||
            source.resolved_source_feature_id!=
                frozen.exclusive_fallback_feature_id))
            return reject(BridgeFailure::incompatible_feature_variant);
        if(!source.feature_authority_attested
           ||source.exact_feature_dependency.empty())
            return reject(BridgeFailure::exact_feature_authority_unresolved);
        const auto n=source.configured_components.size();
        if(n==0||(
           frozen.component_share==ComponentShare::one&&n!=1U) ||
           (frozen.component_share==ComponentShare::thirds&&n!=3U))
            return reject(BridgeFailure::invalid_component_population);
        const auto mass=
            static_cast<double>(frozen.domain_mass.numerator)/
            static_cast<double>(frozen.domain_mass.denominator);
        const double component_mass=mass/static_cast<double>(n);
        if(!std::isfinite(component_mass)||component_mass<=0)
            return reject(BridgeFailure::invalid_component_population);
        auto& domain=out.domain[static_cast<std::size_t>(frozen.domain)];
        for(const auto& part:source.configured_components) {
            if(!std::isfinite(part.paired_support)
               ||part.paired_support<0.0||part.paired_support>1.0
               ||(part.candidate_z&&!std::isfinite(*part.candidate_z))
               ||(part.optimization_target_z &&
                   !std::isfinite(*part.optimization_target_z)))
                return reject(BridgeFailure::illegal_support_or_numeric);
            if(part.valid_current&&part.paired_support>0.0 &&
               (!part.candidate_z||!part.optimization_target_z))
                return reject(BridgeFailure::exact_feature_authority_unresolved);
            // Ineligible parts still occupy their EXACT original mass in
            // configured population; the solver never renormalizes them.
            domain.configured.push_back(
                m10_frozen::AuthoritativeComponent{
                    component_mass,part.valid_current,
                    part.paired_support,frozen.scale,
                    part.candidate_z,part.optimization_target_z,
                    part.required_candidate_evidence});
        }
    }
    // Mass integrity was proven EXACTLY in reduced rational units by
    // static_structure_valid(): no invented epsilon-based "mass repair".
    // Each configured slot contributes all its original component mass.
    return {std::move(out),BridgeFailure::none};
}
} // namespace rgsml::app::l3_automatch::m07_frozen
