#pragma once
// Frozen original S03/S04/S05/S06 source feature identities, no
// new artistic target/safety mass. All numbers originate from the
// same-FFT coherent signed L/R/cross-power native producer.
#include "l3_m06_m20_coherent_stereo_band_sink.hpp"
#include "l3_m05m06_real_analysis_port.hpp"
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
struct CoherentStereoTripleAuthority final {
    std::array<StereoCoherentNativeResult,3> observed;
    std::array<SourceIdentity,3> independent_source_owners;
    std::vector<int> exact_canonical_band_indices;
    std::array<std::array<bool,3>,4> source_phase6_reference_grade{};
    std::array<std::string,4> immutable_feature_definition_digests;
    bool exact_current_common_band_grid_and_retained_gate_policy{};
};
enum class CoherentStereoBindingFailure:std::uint8_t {
    none,unbound_or_stale_source,invalid_original_grid,
    missing_feature_definition
};
struct CoherentStereoBindingProjection final {
    std::optional<std::array<m08_frozen::AuthenticatedPairSlot,4>>
        feature_bindings;
    CoherentStereoBindingFailure failure{
        CoherentStereoBindingFailure::unbound_or_stale_source};
    [[nodiscard]] constexpr bool authorizes_m10_or_m13()
        const noexcept{return false;}
};
[[nodiscard]] inline CoherentStereoBindingProjection
bind_real_s03_s06(const CoherentStereoTripleAuthority& e) {
    if(!e.exact_current_common_band_grid_and_retained_gate_policy)
        return {};
    const auto n=e.exact_canonical_band_indices.size();
    if(!n)return {{},CoherentStereoBindingFailure::invalid_original_grid};
    for(std::size_t j=0;j<3U;++j) {
        const auto& owner=e.independent_source_owners[j];
        const auto& observed=e.observed[j];
        if(!owner.full_track_owner_confirmed||!owner.generation||
           owner.resource_identity.empty()||
           !observed.numerical_source_valid||
           observed.multiband.size()!=n)
            return {{},CoherentStereoBindingFailure::unbound_or_stale_source};
        for(std::size_t k=0;k<j;++k)
            if(owner.resource_identity==
                 e.independent_source_owners[k].resource_identity)
                return {{},CoherentStereoBindingFailure::unbound_or_stale_source};
        for(std::size_t i=0;i<n;++i) {
            if(observed.multiband[i].canonical_index!=
                 e.exact_canonical_band_indices[i]||
               (i>0U&&e.exact_canonical_band_indices[i]<=
                        e.exact_canonical_band_indices[i-1U]))
                return {{},CoherentStereoBindingFailure::invalid_original_grid};
        }
    }
    constexpr std::array<std::string_view,4> identity{{
        "rgsml.reference-match.binding.stereo.s03",
        "rgsml.reference-match.binding.stereo.s04",
        "rgsml.reference-match.binding.stereo.s05",
        "rgsml.reference-match.binding.stereo.s06"}};
    std::array<m08_frozen::AuthenticatedPairSlot,4> all;
    for(std::size_t i=0;i<4U;++i) {
        const auto* frozen=m07_frozen::find_binding(identity[i]);
        if(!frozen||e.immutable_feature_definition_digests[i].empty())
            return {{},CoherentStereoBindingFailure::
                missing_feature_definition};
        auto& slot=all[i];
        slot.binding_id=std::string{frozen->binding_id};
        slot.source_feature_id=std::string{frozen->source_feature_id};
        slot.exact_feature_dependency=e.immutable_feature_definition_digests[i];
        slot.feature_authority_attested=true;
        const bool lowend=i>=2U;
        const std::size_t components=lowend?3U:n;
        slot.configured.reserve(components);
        for(std::size_t k=0;k<components;++k) {
            std::array<m08_frozen::CanonicalCoordinate,3> z{};
            std::array<double,2> support{};
            for(std::size_t j=0;j<3U;++j) {
                const auto& observed=e.observed[j];
                const StereoCoherentScalar* scalar=nullptr;
                if(i==0U)scalar=&observed.multiband[k].width;
                else if(i==1U)scalar=&observed.multiband[k].correlation;
                else if(i==2U)scalar=&observed.low_end_width[k];
                else scalar=&observed.low_end_correlation[k];
                const bool eligible=e.source_phase6_reference_grade[i][j]&&
                    (!lowend||
                     observed.low_end_gate_current_and_identical);
                if(!eligible||!scalar->transformed_z)continue;
                const double coordinate=*scalar->transformed_z;
                if(!std::isfinite(coordinate))continue;
                const auto endpoint=i==0U||i==2U
                    ? (scalar->actual_zero_side_endpoint
                         ?m08_frozen::Endpoint::mono_side_muted
                         :m08_frozen::Endpoint::ordinary)
                    :m08_frozen::Endpoint::ordinary;
                z[j]={coordinate,endpoint};
                if(j<2U) {
                    const double ratio=lowend?1.0:scalar->native_support;
                    if(!std::isfinite(ratio)||ratio<0.0||ratio>1.0)
                        return {{},CoherentStereoBindingFailure::
                            invalid_original_grid};
                    support[j]=ratio;
                }
            }
            const double paired=std::min(support[0],support[1]);
            const bool valid=z[0].z&&z[1].z&&paired>0.0;
            slot.configured.push_back({
                static_cast<bool>(valid),valid?paired:0.0,
                z[0],z[1],z[2].z,true});
            auto& native=slot.configured.back().candidate_native;
            const auto& actual=e.observed[2];
            const StereoCoherentScalar* raw=nullptr;
            if(i==0U)raw=&actual.multiband[k].width;
            else if(i==1U)raw=&actual.multiband[k].correlation;
            else if(i==2U)raw=&actual.low_end_width[k];
            else raw=&actual.low_end_correlation[k];
            if(z[2].z)native.value=raw->raw_ratio_or_rho;
            native.endpoint=z[2].endpoint;
            native.valid_count=raw->valid_frames;
            native.configured_count=actual.native_selected_frames;
            native.population_identity=raw->exact_frame_population_identity;
            constexpr std::array<int,3> cutoffs{{80,120,200}};
            native.selector=lowend?
                std::to_string(cutoffs[k]):
                std::to_string(e.exact_canonical_band_indices[k]);
        }
    }
    return {std::move(all),CoherentStereoBindingFailure::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
