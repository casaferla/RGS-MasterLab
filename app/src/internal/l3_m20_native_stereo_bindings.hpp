#pragma once
// L3 M06 real M20 S01/S02 -> exactly two original M07 Stereo feature_slots.
// M20 measured broadband width and native 400ms correlation quantiles
// already exist; do not regenerate side energy or create a new safety
// objective. Mono endpoint uses RAW actual zero-side evidence, not z.
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

namespace rgsml::app::l3_automatch::m05m06_real {
struct M20StereoFeatureAuthority final {
    std::string exact_feature_dependency;
    bool current_phase6_feature_definition{};
    std::array<bool,3> phase6_reference_grade{};
};
struct M20StereoTriple final {
    SourceReadings prepared,reference,actual_candidate;
    std::array<M20StereoFeatureAuthority,2> authority; // S01,S02
};
enum class M20StereoFailure:std::uint8_t {
    none,invalid_owner,unresolved_phase6_feature,
    invalid_native_support
};
struct M20StereoProjection final {
    std::optional<std::array<m08_frozen::AuthenticatedPairSlot,2>> feature_slots;
    M20StereoFailure reason{M20StereoFailure::invalid_owner};
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
};
[[nodiscard]] inline M20StereoProjection bind_real_m20_s01_s02(
    const M20StereoTriple& in)
{
    const std::array<const SourceReadings*,3> owners{{
        &in.prepared,&in.reference,&in.actual_candidate}};
    for(const auto* s:owners)
        if(!s->owner.full_track_owner_confirmed||
           !s->owner.generation||s->owner.resource_identity.empty()||
           s->sample_rate_hz<=0||s->scope_frame_count<=0)
            return {{},M20StereoFailure::invalid_owner};
    if(owners[0]->owner.resource_identity==owners[1]->owner.resource_identity||
       owners[0]->owner.resource_identity==owners[2]->owner.resource_identity||
       owners[1]->owner.resource_identity==owners[2]->owner.resource_identity)
        return {{},M20StereoFailure::invalid_owner};
    const auto* s01=m07_frozen::find_binding(
        "rgsml.reference-match.binding.stereo.s01");
    const auto* s02=m07_frozen::find_binding(
        "rgsml.reference-match.binding.stereo.s02");
    if(!s01||!s02)return {{},M20StereoFailure::unresolved_phase6_feature};
    std::array<m08_frozen::AuthenticatedPairSlot,2> out;
    for(std::size_t i=0;i<2U;++i) {
        const auto* binding=i==0?s01:s02;
        if(!in.authority[i].current_phase6_feature_definition||
           in.authority[i].exact_feature_dependency.empty())
            return {{},M20StereoFailure::unresolved_phase6_feature};
        out[i]={
            std::string{binding->binding_id},
            std::string{binding->source_feature_id},
            in.authority[i].exact_feature_dependency,true,{}};
    }
    // S01: exact actual raw zero-Side endpoint is distinct from a
    // positive width ratio that happens to transform to z=-24.
    const auto width=[&](std::size_t j)
        ->m08_frozen::CanonicalCoordinate {
        const auto& s=*owners[j];
        if(!in.authority[0].phase6_reference_grade[j]||
           !s.broadband_width.ready()||
           !s.broadband_width_z.ready())
            return {};
        return {s.broadband_width_z.unrounded,
            *s.broadband_width.unrounded==0.0
                ?m08_frozen::Endpoint::mono_side_muted
                :m08_frozen::Endpoint::ordinary};
    };
    const auto pw=width(0),rw=width(1),cw=width(2);
    const bool valid_width=pw.z&&rw.z;
    out[0].configured.push_back({
        static_cast<bool>(valid_width),
        valid_width?1.0:0.0,
        pw,rw,cw.z,true});
    auto& width_native=out[0].configured.back().candidate_native;
    if(cw.z)width_native.value=in.actual_candidate.broadband_width.unrounded;
    width_native.endpoint=cw.endpoint;
    width_native.selector="broadband-width";
    // S02: transformed Fisher-Z Type7 native P10/P50/P90 and
    // independent one-time valid/complete native correlation support.
    double pair_support=0.0;
    if(in.prepared.correlation_native_support&&
       in.reference.correlation_native_support) {
        const auto p=*in.prepared.correlation_native_support;
        const auto g=*in.reference.correlation_native_support;
        if(!std::isfinite(p)||!std::isfinite(g)||
           p<0.0||p>1.0||g<0.0||g>1.0)
            return {{},M20StereoFailure::invalid_native_support};
        pair_support=std::min(p,g);
    }
    for(std::size_t i=0;i<3U;++i) {
        std::array<std::optional<double>,3> z{};
        for(std::size_t j=0;j<3U;++j)
            if(in.authority[1].phase6_reference_grade[j]&&
               owners[j]->correlation_fisher_z[i].ready())
                z[j]=owners[j]->correlation_fisher_z[i].unrounded;
        const bool valid_corr=z[0]&&z[1]&&pair_support>0.0;
        out[1].configured.push_back({
            static_cast<bool>(valid_corr),valid_corr?pair_support:0.0,
            {z[0],m08_frozen::Endpoint::ordinary},
            {z[1],m08_frozen::Endpoint::ordinary},
            z[2],true});
        auto& native=out[1].configured.back().candidate_native;
        const std::array<const Value*,3> raw{{
            &in.actual_candidate.correlation_p10,
            &in.actual_candidate.correlation_p50,
            &in.actual_candidate.correlation_p90}};
        if(z[2])native.value=raw[i]->unrounded;
        native.valid_count=in.actual_candidate.valid_correlation_windows;
        native.configured_count=in.actual_candidate.complete_correlation_windows;
        native.selector=std::array<const char*,3>{{"P10","P50","P90"}}[i];
    }
    return {std::move(out),M20StereoFailure::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
