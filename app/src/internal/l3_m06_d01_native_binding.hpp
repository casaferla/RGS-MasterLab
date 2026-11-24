#pragma once
// L3 M06 D01 center-relative Short-term shape reuses exact real M19
// ungated native 3s/Grid100, independently of M05 L02 absolute P50.
// Original D01 3-component mass, MIN(native support) once, no new
// percentile source feature or invented processing chain.
#include "l3_m06_real_short_term_shape.hpp"
#include "l3_m08_frozen_targeted_population.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m05m06_real {
struct D01TripleEvidence final {
    std::array<SourceReadings,3> source; // Prepared, Gold, Candidate
    std::array<bool,3> phase6_reference_grade{};
    std::string exact_feature_dependency;
    bool exact_phase6_feature_revision_and_context_current{};
};
enum class D01BindingFailure:std::uint8_t {
    none,unverified_owner_or_phase6,invalid_binding_definition
};
struct D01BindingProjection final {
    std::optional<m08_frozen::AuthenticatedPairSlot> slot;
    D01BindingFailure failure{D01BindingFailure::unverified_owner_or_phase6};
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
};
[[nodiscard]] inline D01BindingProjection bind_real_d01_native(
    const D01TripleEvidence& e)
{
    if(!e.exact_phase6_feature_revision_and_context_current)
        return {};
    for(std::size_t i=0;i<3U;++i) {
        const auto& owner=e.source[i].owner;
        if(!owner.full_track_owner_confirmed||!owner.generation||
           owner.resource_identity.empty())
            return {};
        for(std::size_t j=0;j<i;++j)
            if(owner.resource_identity==e.source[j].owner.resource_identity)
                return {};
    }
    if(e.exact_feature_dependency.empty())
        return {{},D01BindingFailure::invalid_binding_definition};
    const auto* frozen=m07_frozen::find_binding(
        "rgsml.reference-match.binding.dynamics.d01");
    if(!frozen)
        return {{},D01BindingFailure::invalid_binding_definition};
    const auto pair=pair_d01(e.source[0],e.source[1]);
    const auto candidate=observe_d01(e.source[2]);
    const bool valid_pair=pair&&e.phase6_reference_grade[0]&&
        e.phase6_reference_grade[1]&&pair->pair_support>0.0;
    m08_frozen::AuthenticatedPairSlot result{
        std::string{frozen->binding_id},
        std::string{frozen->source_feature_id},
        e.exact_feature_dependency,true,{}};
    for(std::size_t i=0;i<3U;++i) {
        const std::optional<double> p=valid_pair?
            std::optional<double>{
                pair->candidate.relative_to_p50_lu[i]}:std::nullopt;
        const std::optional<double> g=valid_pair?
            std::optional<double>{
                pair->reference.relative_to_p50_lu[i]}:std::nullopt;
        const std::optional<double> c=
            candidate&&e.phase6_reference_grade[2]?
            std::optional<double>{
                candidate->relative_to_p50_lu[i]}:std::nullopt;
        result.configured.push_back({
            valid_pair,valid_pair?pair->pair_support:0.0,
            {p,m08_frozen::Endpoint::ordinary},
            {g,m08_frozen::Endpoint::ordinary},c,true});
        auto& native=result.configured.back().candidate_native;
        native.value=c;
        native.selector=std::array<const char*,3>{{"P10-P50","P90-P50","P95-P50"}}[i];
        if(candidate) {
            native.valid_count=candidate->valid_windows;
            native.configured_count=candidate->complete_windows;
        }
    }
    return {std::move(result),D01BindingFailure::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
