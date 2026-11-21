#pragma once
// L3 M05 T01 exact primary 1/12 fractional-octave M20 vector
// to original M07 T01 slot; FULL configured canonical band list stays
// in denominator. Missing/below-floor components never disappear
// and there is NO false 1/12 recreation from coarse 1/3 UI.
#include "l3_m05m06_real_analysis_port.hpp"
#include "l3_m08_frozen_targeted_population.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
struct T01ConfiguredBand final {
    int canonical_index{};
    double low_hz{},center_hz{},high_hz{};
};
struct T01TripleEvidence final {
    std::array<SourceReadings,3> actual_sources; // P,G,C
    std::vector<T01ConfiguredBand> authoritative_primary_grid;
    std::array<std::vector<std::optional<double>>,2>
        exact_native_band_support_prepared_gold;
    std::array<bool,3> phase6_source_reference_grade{};
    std::string exact_primary_t01_feature_dependency;
    bool exact_phase6_grid_and_native_support_current{};
};
enum class T01BindingFailure:std::uint8_t {
    none,stale_source,unresolved_primary_grid,
    mismatched_authoritative_geometry,invalid_native_support,
    unresolved_feature_revision
};
struct T01BindingProjection final {
    std::optional<m08_frozen::AuthenticatedPairSlot> slot;
    T01BindingFailure reason{T01BindingFailure::unresolved_primary_grid};
    [[nodiscard]] constexpr bool authorizes_plan() const noexcept {
        return false;
    }
};
[[nodiscard]] inline T01BindingProjection bind_real_t01_primary(
    const T01TripleEvidence& e)
{
    if(!e.exact_phase6_grid_and_native_support_current||
       e.exact_primary_t01_feature_dependency.empty())
        return {{},T01BindingFailure::unresolved_feature_revision};
    for(std::size_t k=0;k<3U;++k) {
        const auto& s=e.actual_sources[k];
        if(!s.owner.full_track_owner_confirmed||
           !s.owner.generation||s.owner.resource_identity.empty()||
           s.sample_rate_hz<=0||s.scope_frame_count<=0)
            return {{},T01BindingFailure::stale_source};
        for(std::size_t j=0;j<k;++j)
            if(s.owner.resource_identity==
               e.actual_sources[j].owner.resource_identity)
                return {{},T01BindingFailure::stale_source};
    }
    const auto& grid=e.authoritative_primary_grid;
    if(grid.empty()||e.exact_native_band_support_prepared_gold[0].size()
                        !=grid.size()||
       e.exact_native_band_support_prepared_gold[1].size()!=grid.size())
        return {{},T01BindingFailure::unresolved_primary_grid};
    for(std::size_t i=0;i<grid.size();++i) {
        const auto& b=grid[i];
        if(!std::isfinite(b.low_hz)||!std::isfinite(b.center_hz)||
           !std::isfinite(b.high_hz)||b.low_hz<=0.0||
           b.center_hz<=b.low_hz||b.high_hz<=b.center_hz||
           (i>0U&&grid[i-1U].canonical_index>=b.canonical_index))
            return {{},T01BindingFailure::unresolved_primary_grid};
        for(const auto& support:e.exact_native_band_support_prepared_gold) {
            if(!support[i])continue; // explicit unmeasured native support
            if(!std::isfinite(*support[i])||*support[i]<0.0||
               *support[i]>1.0)
                return {{},T01BindingFailure::invalid_native_support};
        }
    }
    const auto* frozen=m07_frozen::find_binding(
        "rgsml.reference-match.binding.tonal.t01");
    if(!frozen)return {{},T01BindingFailure::unresolved_feature_revision};
    m08_frozen::AuthenticatedPairSlot slot{
        std::string{frozen->binding_id},
        std::string{frozen->source_feature_id},
        e.exact_primary_t01_feature_dependency,true,{}};
    slot.configured.reserve(grid.size());
    for(std::size_t i=0;i<grid.size();++i) {
        std::array<std::optional<double>,3> z{};
        const auto& expected=grid[i];
        for(std::size_t k=0;k<3U;++k) {
            const auto& observed=e.actual_sources[k].profile_1_12;
            const auto it=std::lower_bound(
                observed.begin(),observed.end(),expected.canonical_index,
                [](const SourceReadings::Band& a,int idx) {
                    return a.canonical_index<idx;
                });
            if(it==observed.end()||
               it->canonical_index!=expected.canonical_index)
                continue; // absent canonical component remains NULL
            if(it->low_hz!=expected.low_hz||
               it->center_hz!=expected.center_hz||
               it->high_hz!=expected.high_hz)
                return {{},T01BindingFailure::
                    mismatched_authoritative_geometry};
            if(e.phase6_source_reference_grade[k]&&
               it->power_fraction_db.ready())
                z[k]=it->power_fraction_db.unrounded;
        }
        double paired=0.0;
        const auto& p=e.exact_native_band_support_prepared_gold[0][i];
        const auto& g=e.exact_native_band_support_prepared_gold[1][i];
        if(p&&g)paired=std::min(*p,*g);
        const bool valid=z[0]&&z[1]&&paired>0.0;
        slot.configured.push_back({
            static_cast<bool>(valid),valid?paired:0.0,
            {z[0],m08_frozen::Endpoint::ordinary},
            {z[1],m08_frozen::Endpoint::ordinary},
            z[2],true});
    }
    return {std::move(slot),T01BindingFailure::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
