#pragma once
// L3 M06 D07/D08/D09 exact frozen M07 FeatureBinding from a common
// independently authenticated Phase6 P04 detector event population.
// Source cannot be promoted merely by having plausible percentiles.
#include "l3_m06_native_transient_descriptors.hpp"
#include "l3_m08_frozen_targeted_population.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m05m06_real {
struct TransientTripleSource final {
    SourceIdentity owner;
    CanonicalTransientDistributions measured;
};
struct TransientTripleEvidence final {
    std::array<TransientTripleSource,3> current_sources; // P,G,C
    std::array<std::string,3> exact_feature_definition_digests;
    std::array<std::array<bool,3>,3> per_descriptor_source_grade{};
    bool same_exact_p04_policy_context_and_generation_attested{};
};
enum class TransientBindingFailure:std::uint8_t {
    none,unbound_or_stale_source,unresolved_native_descriptor_policy
};
struct TransientBindingProjection final {
    std::optional<std::array<m08_frozen::AuthenticatedPairSlot,3>> feature_slots;
    TransientBindingFailure failure{
        TransientBindingFailure::unbound_or_stale_source};
    [[nodiscard]] constexpr bool authorizes_match_or_publication()
        const noexcept{return false;}
};
[[nodiscard]] inline TransientBindingProjection
bind_real_d07_d09(const TransientTripleEvidence& e)
{
    if(!e.same_exact_p04_policy_context_and_generation_attested)
        return {};
    for(std::size_t j=0;j<3U;++j) {
        const auto& owner=e.current_sources[j].owner;
        if(!owner.full_track_owner_confirmed||!owner.generation||
           owner.resource_identity.empty())
            return {};
        for(std::size_t k=0;k<j;++k)
            if(owner.resource_identity==
               e.current_sources[k].owner.resource_identity)
                return {};
    }
    constexpr std::array<std::string_view,3> keys{{
        "rgsml.reference-match.binding.dynamics.d07",
        "rgsml.reference-match.binding.dynamics.d08",
        "rgsml.reference-match.binding.dynamics.d09"
    }};
    std::array<m08_frozen::AuthenticatedPairSlot,3> out;
    for(std::size_t d=0;d<3U;++d) {
        const auto* frozen=m07_frozen::find_binding(keys[d]);
        if(!frozen||e.exact_feature_definition_digests[d].empty())
            return {{},
                TransientBindingFailure::unresolved_native_descriptor_policy};
        out[d]={
            std::string{frozen->binding_id},
            std::string{frozen->source_feature_id},
            e.exact_feature_definition_digests[d],true,{}};
        std::array<const NativeTransientDescriptor*,3> src{};
        std::array<bool,3> valid{};
        for(std::size_t j=0;j<3U;++j) {
            src[j]=&e.current_sources[j].measured.d07_d08_d09[d];
            valid[j]=e.per_descriptor_source_grade[d][j]&&
                src[j]->reference_grade()&&src[j]->native_support&&
                *src[j]->native_support>0.0&&
                std::isfinite(*src[j]->native_support)&&
                *src[j]->native_support<=1.0;
        }
        const double support=valid[0]&&valid[1]
            ?std::min(*src[0]->native_support,*src[1]->native_support)
            :0.0;
        for(std::size_t k=0;k<3U;++k) {
            const auto value=[&](std::size_t j)->std::optional<double> {
                return valid[j]?
                    src[j]->canonical_type7_p10_p50_p90[k]:
                    std::nullopt;
            };
            const auto p=value(0),g=value(1),c=value(2);
            const bool ready=p&&g&&support>0.0;
            out[d].configured.push_back({
                static_cast<bool>(ready),ready?support:0.0,
                {p,m08_frozen::Endpoint::ordinary},
                {g,m08_frozen::Endpoint::ordinary},c,true});
        }
    }
    return {std::move(out),TransientBindingFailure::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
