#pragma once
// L3-M06 D10-D12 original M07 canonical band-vector binding.
// The precomputed actual native per-frame series is ONLY accepted
// after independent Phase6 STFT, source and policy authentication.
// No resampling of M20 time-average PSD and no shortened denominator.
#include "l3_m06_native_spectral_d10_d12.hpp"
#include "l3_m08_frozen_targeted_population.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
struct SpectralTripleAuthority final {
    std::array<SpectralD10D12Native,3> observed; // P/G/C, all same grid
    std::array<SourceIdentity,3> source_owner;
    std::array<std::array<bool,3>,3> source_reference_grade{};
    std::array<std::string,3> canonical_feature_dependency;
    bool exact_current_full_stft_grid_and_policy_attested{};
};
enum class SpectralSlotFailure:std::uint8_t {
    none,unbound_or_stale_stft,invalid_component_geometry,
    missing_feature_definition_or_revision
};
struct SpectralSlotProjection final {
    std::optional<std::array<m08_frozen::AuthenticatedPairSlot,3>>
        feature_bindings;
    SpectralSlotFailure failure{SpectralSlotFailure::unbound_or_stale_stft};
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
};
[[nodiscard]] inline SpectralSlotProjection bind_real_d10_d12(
    const SpectralTripleAuthority& a)
{
    if(!a.exact_current_full_stft_grid_and_policy_attested)
        return {};
    const std::size_t n=a.observed[0].canonical_band_results.size();
    if(!n)return {{},SpectralSlotFailure::invalid_component_geometry};
    for(std::size_t j=0;j<3U;++j) {
        const auto& owner=a.source_owner[j];
        if(!owner.full_track_owner_confirmed||owner.generation==0||
           owner.resource_identity.empty()||
           a.observed[j].canonical_band_results.size()!=n||
           a.observed[j].source_reason!=SpectralNativeReason::ready)
            return {{},SpectralSlotFailure::unbound_or_stale_stft};
        for(std::size_t k=0;k<j;++k)
            if(owner.resource_identity==a.source_owner[k].resource_identity)
                return {{},SpectralSlotFailure::unbound_or_stale_stft};
    }
    constexpr std::array<std::string_view,3> identities{{
        "rgsml.reference-match.binding.dynamics.d10",
        "rgsml.reference-match.binding.dynamics.d11",
        "rgsml.reference-match.binding.dynamics.d12"}};
    std::array<m08_frozen::AuthenticatedPairSlot,3> out;
    for(std::size_t d=0;d<3U;++d) {
        const auto* frozen=m07_frozen::find_binding(identities[d]);
        if(!frozen||a.canonical_feature_dependency[d].empty())
            return {{},
                SpectralSlotFailure::missing_feature_definition_or_revision};
        auto& slot=out[d];
        slot.binding_id=std::string{frozen->binding_id};
        slot.source_feature_id=std::string{frozen->source_feature_id};
        slot.exact_feature_dependency=a.canonical_feature_dependency[d];
        slot.feature_authority_attested=true;
        slot.configured.reserve(n);
        for(std::size_t i=0;i<n;++i) {
            std::array<std::optional<double>,3> measured{};
            std::array<double,2> native_support{};
            for(std::size_t j=0;j<3U;++j) {
                const auto& v=a.observed[j].canonical_band_results[i];
                if(v.reason[d]!=SpectralNativeReason::ready||
                   !a.source_reference_grade[d][j])
                    continue;
                const auto value=v.d10_d11_d12_z[d];
                if(!value||!std::isfinite(*value))
                    continue;
                measured[j]=value;
                if(j<2U)native_support[j]=v.native_support[d];
            }
            const double support=std::min(
                native_support[0],native_support[1]);
            if(!std::isfinite(support)||support<0.0||support>1.0)
                return {{},SpectralSlotFailure::invalid_component_geometry};
            const bool valid=measured[0]&&measured[1]&&support>0.0;
            slot.configured.push_back({
                static_cast<bool>(valid),valid?support:0.0,
                {measured[0],m08_frozen::Endpoint::ordinary},
                {measured[1],m08_frozen::Endpoint::ordinary},
                measured[2],true});
            auto& native=slot.configured.back().candidate_native;
            const auto& actual=a.observed[2].canonical_band_results[i];
            if(measured[2])
                native.value=d==2U?actual.d12_raw_rho:measured[2];
            native.valid_count=actual.valid_native_counts[d];
            native.configured_count=a.observed[2].candidate_active_frames;
            native.selector=std::to_string(i);
            native.population_identity=actual.exact_frame_population_identity;
        }
    }
    return {std::move(out),SpectralSlotFailure::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
