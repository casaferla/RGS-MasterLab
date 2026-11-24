#pragma once
// L3-M07 -> M08 -> M10 explicit frozen authoritative-target numerical
// composition, *not* a new source-feature analyzer or a Product plan.
// Prepared/REFERENCE and candidate coordinates are already canonical
// and independently authority-bound by the caller's M05/M06/M07 data.
// Any ineligible component remains in the exact configured denominator.
#include "l3_m07_authoritative_m10_bridge.hpp"
#include "l3_m08_frozen_amount_target.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m08_frozen {
// Retain actual native coordinates for M13. Never invert Fisher-z
// or a clipped mono width to guess an endpoint. The M10 objective
// still uses the original transformed coordinate and pair support.
struct CandidateNativeObservation final {
    std::optional<double> value;
    Endpoint endpoint{Endpoint::ordinary};
    std::optional<std::uint64_t> valid_count;
    std::optional<std::uint64_t> configured_count;
    // Exact source-produced selector and selected population identity;
    // absent when the producer has not exposed that evidence.
    std::string selector;
    std::string population_identity;
    friend bool operator==(const CandidateNativeObservation&,
                           const CandidateNativeObservation&)=default;
};
struct AuthenticatedPairComponent final {
    bool current_valid{};
    double paired_native_support{};
    CanonicalCoordinate prepared;
    CanonicalCoordinate reference;
    std::optional<double> current_candidate_z;
    bool candidate_required{true};
    CandidateNativeObservation candidate_native{};
    friend bool operator==(const AuthenticatedPairComponent&,
                           const AuthenticatedPairComponent&)=default;
};
struct AuthenticatedPairSlot final {
    std::string binding_id;
    std::string source_feature_id;
    std::string exact_feature_dependency;
    bool feature_authority_attested{};
    std::vector<AuthenticatedPairComponent> configured;
    friend bool operator==(const AuthenticatedPairSlot&,
                           const AuthenticatedPairSlot&)=default;
};
enum class PopulationFailure:std::uint8_t {
    none,invalid_binding_or_dependency,
    missing_authoritative_coordinate,invalid_support,
    amount_or_target_invalid,m07_bridge_rejected
};
struct TargetedPopulation final {
    std::array<m10_frozen::DomainPopulation,4> objective_input;
    std::vector<std::vector<std::optional<DesiredCoordinate>>>
        desired_by_canonical_slot; // retains endpoint identity, nulls
    [[nodiscard]] constexpr bool authorizes_product_master()
        const noexcept{return false;}
};
struct TargetedPopulationResult final {
    std::optional<TargetedPopulation> value;
    PopulationFailure failure{PopulationFailure::invalid_binding_or_dependency};
};
[[nodiscard]] inline TargetedPopulationResult build_targeted_population(
    const AmountConfig& amounts,
    const std::vector<AuthenticatedPairSlot>& paired,
    const std::array<bool,4>& authenticated_m08_corrective_mask)
{
    const auto fail=[](PopulationFailure why)->TargetedPopulationResult {
        return {{},why};
    };
    if(paired.size()!=m07_frozen::kBindings.size())
        return fail(PopulationFailure::invalid_binding_or_dependency);
    std::vector<m07_frozen::ResolvedSlot> resolved;
    resolved.reserve(paired.size());
    TargetedPopulation result;
    result.desired_by_canonical_slot.reserve(paired.size());
    for(std::size_t i=0;i<paired.size();++i) {
        const auto& frozen=m07_frozen::kBindings[i];
        const auto& in=paired[i];
        if(in.binding_id!=frozen.binding_id
           ||(in.source_feature_id!=frozen.source_feature_id
              &&(frozen.exclusive_fallback_feature_id.empty()
                ||in.source_feature_id!=
                    frozen.exclusive_fallback_feature_id))
           ||in.exact_feature_dependency.empty()
           ||!in.feature_authority_attested)
            return fail(PopulationFailure::invalid_binding_or_dependency);
        m07_frozen::ResolvedSlot out;
        out.binding_id=in.binding_id;
        out.resolved_source_feature_id=in.source_feature_id;
        out.exact_feature_dependency=in.exact_feature_dependency;
        out.feature_authority_attested=true;
        std::vector<std::optional<DesiredCoordinate>> targets;
        targets.reserve(in.configured.size());
        out.configured_components.reserve(in.configured.size());
        for(const auto& item:in.configured) {
            if(!std::isfinite(item.paired_native_support)
               ||item.paired_native_support<0.0
               ||item.paired_native_support>1.0
               ||(item.current_candidate_z
                   &&!std::isfinite(*item.current_candidate_z)))
                return fail(PopulationFailure::invalid_support);
            m07_frozen::SourceComponent c;
            c.valid_current=item.current_valid;
            c.paired_support=item.paired_native_support;
            c.candidate_z=item.current_candidate_z;
            c.required_candidate_evidence=item.candidate_required;
            if(item.current_valid) {
                const auto desired=desired_for_component(
                    amounts,static_cast<std::size_t>(frozen.domain),
                    item.prepared,item.reference);
                if(!desired.value)
                    return fail(PopulationFailure::amount_or_target_invalid);
                c.optimization_target_z=desired.value->desired_z;
                targets.push_back(*desired.value);
            } else {
                // Null target remains genuinely null for unavailable
                // feature; NEVER manufacture Prepared at Amount=0.
                targets.push_back(std::nullopt);
            }
            out.configured_components.push_back(std::move(c));
        }
        resolved.push_back(std::move(out));
        result.desired_by_canonical_slot.push_back(std::move(targets));
    }
    const auto bridge=m07_frozen::to_m10_population(
        resolved,authenticated_m08_corrective_mask);
    if(!bridge.populations)
        return fail(PopulationFailure::m07_bridge_rejected);
    result.objective_input=std::move(bridge.populations->domain);
    return {std::move(result),PopulationFailure::none};
}
} // namespace rgsml::app::l3_automatch::m08_frozen
