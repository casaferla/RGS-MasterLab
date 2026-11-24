#pragma once
// L3 M05 already-computed REAL M20 PSD/OLS Tonal T02..T06 ->
// original five M07 feature_slots. No new FFT, band shrinkage, invented floor,
// spectral-profile resampling or source revision. T01 handled separately.
#include "l3_m05_real_tonal_statistics.hpp"
#include "l3_m05m06_real_analysis_port.hpp"
#include "l3_m08_frozen_targeted_population.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace rgsml::app::l3_automatch::m05m06_real {
inline constexpr std::array<std::string_view,5> kM20ScalarTonalBindings{{
    "rgsml.reference-match.binding.tonal.t02",
    "rgsml.reference-match.binding.tonal.t03",
    "rgsml.reference-match.binding.tonal.t04",
    "rgsml.reference-match.binding.tonal.t05",
    "rgsml.reference-match.binding.tonal.t06"
}};
struct M20TonalSource final {
    SourceIdentity owner;
    TonalObservationBundle observed_from_real_m20;
    std::string selected_spectral_population_identity{};
    std::uint64_t selected_spectral_frames{};
    std::uint64_t complete_spectral_frames{};
};
struct M20TonalAuthority final {
    std::string exact_phase6_feature_dependency;
    bool phase6_definition_and_common_bandwidth_current{};
    std::array<bool,3> reference_grade{}; // P,G,C
};
struct M20TonalTriple final {
    std::array<M20TonalSource,3> sources;
    CommonTonalBandwidth exact_common;
    double exact_accepted_density_floor{};
    std::array<M20TonalAuthority,5> features;
};
enum class TonalBindingProblem:std::uint8_t {
    none,invalid_or_stale_owner,common_bandwidth_or_floor_unresolved,
    feature_content_unresolved
};
struct TonalBindingProjection final {
    std::optional<std::array<m08_frozen::AuthenticatedPairSlot,5>> feature_slots;
    TonalBindingProblem problem{
        TonalBindingProblem::common_bandwidth_or_floor_unresolved};
    [[nodiscard]] constexpr bool authorizes_m10() const noexcept {
        return false;
    }
};
[[nodiscard]] inline TonalBindingProjection bind_real_m20_t02_t06(
    const M20TonalTriple& evidence)
{
    const auto& common=evidence.exact_common;
    if(!std::isfinite(common.low_hz)||
       !std::isfinite(common.high_hz)||
       common.low_hz<20.0||common.high_hz<=common.low_hz||
       !std::isfinite(evidence.exact_accepted_density_floor)||
       evidence.exact_accepted_density_floor<=0.0)
        return {{},
            TonalBindingProblem::common_bandwidth_or_floor_unresolved};
    for(std::size_t i=0;i<3U;++i) {
        const auto& owner=evidence.sources[i].owner;
        if(owner.resource_identity.empty()||!owner.generation||
           !owner.full_track_owner_confirmed)
            return {{},TonalBindingProblem::invalid_or_stale_owner};
        for(std::size_t j=0;j<i;++j)
            if(owner.resource_identity==
               evidence.sources[j].owner.resource_identity)
                return {{},TonalBindingProblem::invalid_or_stale_owner};
        const auto& b=evidence.sources[i].observed_from_real_m20;
        if(b.t3_centroid_t4t5_rolloffs&&
           (b.t3_centroid_t4t5_rolloffs->common.low_hz!=common.low_hz||
            b.t3_centroid_t4t5_rolloffs->common.high_hz!=common.high_hz))
            return {{},
                TonalBindingProblem::common_bandwidth_or_floor_unresolved};
        for(const auto* slope:{
                &b.t2_spectral_tilt,&b.t6_high_frequency_slope})
            if(*slope&&
               (*slope)->supplied_density_floor!=
                   evidence.exact_accepted_density_floor)
                return {{},
                    TonalBindingProblem::common_bandwidth_or_floor_unresolved};
    }
    std::array<m08_frozen::AuthenticatedPairSlot,5> feature_slots;
    for(std::size_t i=0;i<5U;++i) {
        const auto* m07=m07_frozen::find_binding(
            kM20ScalarTonalBindings[i]);
        if(!m07||
           !evidence.features[i].
               phase6_definition_and_common_bandwidth_current||
           evidence.features[i].exact_phase6_feature_dependency.empty())
            return {{},TonalBindingProblem::feature_content_unresolved};
        feature_slots[i]={
            std::string{m07->binding_id},
            std::string{m07->source_feature_id},
            evidence.features[i].exact_phase6_feature_dependency,
            true,{}};
        std::array<std::optional<double>,3> scalar{};
        for(std::size_t j=0;j<3U;++j) {
            const auto& b=evidence.sources[j].observed_from_real_m20;
            if(!evidence.features[i].reference_grade[j])continue;
            switch(i) {
            case 0:
                if(b.t2_spectral_tilt)
                    scalar[j]=b.t2_spectral_tilt->slope_db_per_octave;
                break;
            case 1:
                if(b.t3_centroid_t4t5_rolloffs)
                    scalar[j]=
                        b.t3_centroid_t4t5_rolloffs->centroid_log2_hz;
                break;
            case 2:
                if(b.t3_centroid_t4t5_rolloffs)
                    scalar[j]=
                        b.t3_centroid_t4t5_rolloffs->rolloff85_log2_hz;
                break;
            case 3:
                if(b.t3_centroid_t4t5_rolloffs)
                    scalar[j]=
                        b.t3_centroid_t4t5_rolloffs->rolloff95_log2_hz;
                break;
            case 4:
                if(b.t6_high_frequency_slope)
                    scalar[j]=
                        b.t6_high_frequency_slope->slope_db_per_octave;
                break;
            default:break;
            }
            if(scalar[j]&&!std::isfinite(*scalar[j]))
                return {{},TonalBindingProblem::feature_content_unresolved};
        }
        const bool ready=scalar[0]&&scalar[1];
        feature_slots[i].configured.push_back({
            static_cast<bool>(ready),ready?1.0:0.0,
            {scalar[0],m08_frozen::Endpoint::ordinary},
            {scalar[1],m08_frozen::Endpoint::ordinary},
            scalar[2],true});
        auto& native=feature_slots[i].configured.back().candidate_native;
        if(scalar[2]) {
            const auto& b=evidence.sources[2].observed_from_real_m20;
            switch(i) {
            case 0:native.value=b.t2_spectral_tilt->slope_db_per_octave;break;
            case 1:native.value=b.t3_centroid_t4t5_rolloffs->centroid_hz;break;
            case 2:native.value=b.t3_centroid_t4t5_rolloffs->rolloff85_hz;break;
            case 3:native.value=b.t3_centroid_t4t5_rolloffs->rolloff95_hz;break;
            case 4:native.value=b.t6_high_frequency_slope->slope_db_per_octave;break;
            default:break;
            }
            if(i==0U||i==4U) {
                const auto& slope=i==0U?*b.t2_spectral_tilt:*b.t6_high_frequency_slope;
                native.valid_count=slope.above_floor_bands;
                native.configured_count=slope.configured_bands;
                if(!slope.exact_ols_population_identity.empty()&&
                   !evidence.sources[2].selected_spectral_population_identity.empty())
                    native.population_identity=slope.exact_ols_population_identity+
                        ":"+evidence.sources[2].selected_spectral_population_identity;
            } else {
                const auto& source=evidence.sources[2];
                if(!source.selected_spectral_population_identity.empty()) {
                    native.population_identity=source.selected_spectral_population_identity;
                    native.valid_count=source.selected_spectral_frames;
                    native.configured_count=source.complete_spectral_frames;
                }
            }
        }
        native.selector=feature_slots[i].binding_id;
    }
    return {std::move(feature_slots),TonalBindingProblem::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
