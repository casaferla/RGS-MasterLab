#pragma once
// L3 AutoMatch P2 original M05/M06 source bindings, with no alternate
// source feature registry or generated numerical proxies. The three
// independent owners (PREPARED, REFERENCE, REAL P2 CANDIDATE) must match
// exactly across ALL ten original native producer families before we
// admit ANY of the 27 M07 feature slots. Each original binding remains
// responsible for its frozen support and feature-definition evidence.
#include "l3_m07_existing_native_fragment_collector.hpp"
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m10_frozen {
struct P2OriginalRealNativeTriple final {
    m05m06_real::T01TripleEvidence t01;
    m05m06_real::M20TonalTriple t02_t06;
    m05m06_real::D01TripleEvidence d01;
    m05m06_real::LraTripleEvidence d02;
    m05m06_real::D03D04FullEvidence d03_d04;
    m05m06_real::TransientTripleEvidence d07_d09;
    m05m06_real::SpectralTripleAuthority d10_d12;
    m05m06_real::M19NativeTriple m19;
    m05m06_real::M20StereoTriple s01_s02;
    m05m06_real::CoherentStereoTripleAuthority s03_s06;
    bool recheck_candidate_duration_grade{};
};
enum class P2RealTripleBindingReason:std::uint8_t {
    ready,missing_owner_or_inconsistent_three_way_scope,
    missing_original_source_binding
};
struct P2RealTripleBindingResult final {
    std::optional<m07_frozen::ExistingNativeSourceFragments> fragments;
    P2RealTripleBindingReason reason{
        P2RealTripleBindingReason::
            missing_owner_or_inconsistent_three_way_scope};
    [[nodiscard]] constexpr bool authorizes_solver()const noexcept{
        return false;
    }
};
[[nodiscard]] inline bool exact_source_owner_equal(
    const m05m06_real::SourceIdentity& a,
    const m05m06_real::SourceIdentity& b) noexcept
{
    return a.full_track_owner_confirmed&&b.full_track_owner_confirmed&&
           a.generation!=0U&&b.generation!=0U&&
           !a.resource_identity.empty()&&!b.resource_identity.empty()&&
           a.generation==b.generation&&
           a.resource_identity==b.resource_identity;
}
[[nodiscard]] inline P2RealTripleBindingResult
bind_all_original_real_native_p2_triple(
    const P2OriginalRealNativeTriple& input)
{
    const std::array<m05m06_real::SourceIdentity,3> owners{{
        input.m19.prepared.owner,
        input.m19.reference.owner,
        input.m19.actual_candidate.owner}};
    for(std::size_t i=0;i<owners.size();++i) {
        if(!owners[i].full_track_owner_confirmed||
           owners[i].generation==0U||
           owners[i].resource_identity.empty())
            return {};
        for(std::size_t j=0;j<i;++j)
            if(owners[i].resource_identity==owners[j].resource_identity)
                return {};
        const auto& same=[&](const m05m06_real::SourceIdentity& actual){
            return exact_source_owner_equal(owners[i],actual);
        };
        if(!same(input.t01.actual_sources[i].owner)||
           !same(input.t02_t06.sources[i].owner)||
           !same(input.d01.source[i].owner)||
           !same(input.d07_d09.current_sources[i].owner)||
           !same(input.d10_d12.source_owner[i])||
           !same(input.s03_s06.independent_source_owners[i]))
            return {};
    }
    if(!exact_source_owner_equal(
           owners[0],input.d02.prepared_owner)||
       !exact_source_owner_equal(
           owners[1],input.d02.reference_owner)||
       !exact_source_owner_equal(
           owners[2],input.d02.candidate_owner)||
       !exact_source_owner_equal(
           owners[0],input.d03_d04.prepared_owner)||
       !exact_source_owner_equal(
           owners[1],input.d03_d04.reference_owner)||
       !exact_source_owner_equal(
           owners[2],input.d03_d04.candidate_owner)||
       !exact_source_owner_equal(
           owners[0],input.s01_s02.prepared.owner)||
       !exact_source_owner_equal(
           owners[1],input.s01_s02.reference.owner)||
       !exact_source_owner_equal(
           owners[2],input.s01_s02.actual_candidate.owner))
        return {};
    m07_frozen::ExistingNativeSourceFragments fragments;
    fragments.tonal_primary=m05m06_real::bind_real_t01_primary(input.t01);
    fragments.tonal_scalar=m05m06_real::bind_real_m20_t02_t06(
        input.t02_t06);
    fragments.macro_d01=m05m06_real::bind_real_d01_native(input.d01);
    fragments.macro_lra=m05m06_real::bind_real_d02_lra(input.d02);
    fragments.macro_windowed=m05m06_real::make_real_d03_d04_m07_m08_slots(
        input.d03_d04);
    fragments.micro_events=m05m06_real::bind_real_d07_d09(input.d07_d09);
    fragments.spectral_frame_energy=m05m06_real::bind_real_d10_d12(
        input.d10_d12);
    fragments.macro_loudness=m05m06_real::bind_real_m19_loudness_macro(
        input.m19);
    fragments.stereo_broadband=m05m06_real::bind_real_m20_s01_s02(
        input.s01_s02);
    fragments.stereo_coherent_bands_and_lowend=
        m05m06_real::bind_real_s03_s06(input.s03_s06);
    if(!fragments.tonal_primary->slot||
       !fragments.tonal_scalar->feature_slots||
       !fragments.macro_d01->slot||
       !fragments.macro_lra->slot||
       !fragments.macro_windowed->feature_slots||
       !fragments.micro_events->feature_slots||
       !fragments.spectral_frame_energy->feature_bindings||
       !fragments.macro_loudness->feature_slots||
       !fragments.stereo_broadband->feature_slots||
       !fragments.stereo_coherent_bands_and_lowend->feature_bindings)
        return {{},P2RealTripleBindingReason::
            missing_original_source_binding};
    return {std::move(fragments),P2RealTripleBindingReason::ready};
}
} // namespace rgsml::app::l3_automatch::m10_frozen
