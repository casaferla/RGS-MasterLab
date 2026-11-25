#pragma once
// One Product candidate M05/M06 source provenance bridge.
// PREPARED/REFERENCE and Phase6 definition flags remain under the
// authenticated Product owner. CANDIDATE feature values are REBUILT
// from this exact P2 render, M19, M20, P04 and PCM only: never accept
// numerical candidate readings fabricated by the original-triple
// resolver. No M21 proxy, second rendering, M17 WAV Export, M12 receipt.
#include "l3_m10_p2_real_native_triple_binding.hpp"
#include "l3_m10_native_source_grade.hpp"
#include "l3_m10_p2_full_native_evidence.hpp"
#include "l3_m05m06_original_prepared_reference_pair.hpp"
#include <array>
#include <cstdint>
#include <utility>

namespace rgsml::app::l3_automatch::m10_frozen {
// M13 changes only the candidate owner of the already accepted definitions.
// All candidate observations are discarded/rebuilt by the real binder below.
[[nodiscard]] inline bool rebind_original_candidate_owner(
    P2OriginalRealNativeTriple& original,
    const m05m06_real::SourceIdentity& accepted,
    const m05m06_real::SourceIdentity& fresh)
{
    if(!fresh.full_track_owner_confirmed||!fresh.generation||
       fresh.resource_identity.empty()||
       fresh.resource_identity==accepted.resource_identity)return false;
    std::array<m05m06_real::SourceIdentity*,10> owners{{
        &original.m19.actual_candidate.owner,
        &original.s01_s02.actual_candidate.owner,
        &original.t01.actual_sources[2].owner,
        &original.t02_t06.sources[2].owner,
        &original.d01.source[2].owner,
        &original.d02.candidate_owner,
        &original.d03_d04.candidate_owner,
        &original.d07_d09.current_sources[2].owner,
        &original.d10_d12.source_owner[2],
        &original.s03_s06.independent_source_owners[2]}};
    for(const auto* owner:owners)
        if(!exact_source_owner_equal(*owner,accepted))return false;
    for(auto* owner:owners)*owner=fresh;
    return true;
}
[[nodiscard]] inline P2RealTripleBindingResult
bind_original_native_triple_from_measured_source(
    P2OriginalRealNativeTriple original,
    const m05m06_real::SourceReadings& actual,
    const analysis::TonalStereoAnalysisResult& tonal,
    const audio::AudioBufferView& pcm,
    const m05m06_real::P04ProductObservation& transient,
    const m05m06_real::SpectralD10D12Native& spectral,
    const m05m06_real::StereoCoherentNativeResult& stereo,
    const m05m06_real::SourceIdentity& current_candidate_product_owner,
    std::int64_t exact_phase6_active_frames,
    bool phase6_file_based_trailing_policy_attested,
    m05m06_real::D03D04NativeSeries* retain_actual_windows=nullptr)
{
    // The owner proves the exact candidate generation. A callback must
    // not introduce another candidate identity within a feature family
    // or copy PREPARED readings while claiming this P2 run.
    if(!current_candidate_product_owner.full_track_owner_confirmed||
       !current_candidate_product_owner.generation||
       current_candidate_product_owner.resource_identity.empty())
        return {};
    const auto same=[&](const m05m06_real::SourceIdentity& got) {
        return exact_source_owner_equal(
            got,current_candidate_product_owner);
    };
    if(!same(original.m19.actual_candidate.owner)||
       !same(original.s01_s02.actual_candidate.owner)||
       !same(original.t01.actual_sources[2].owner)||
       !same(original.t02_t06.sources[2].owner)||
       !same(original.d01.source[2].owner)||
       !same(original.d02.candidate_owner)||
       !same(original.d03_d04.candidate_owner)||
       !same(original.d07_d09.current_sources[2].owner)||
       !same(original.d10_d12.source_owner[2])||
       !same(original.s03_s06.independent_source_owners[2]))
        return {};
    if(!same(actual.owner))return {};
    if(original.recheck_candidate_duration_grade) {
        if(exact_phase6_active_frames<0)return {};
        const auto grade=m05m06_real::native_full_source_duration_grade(actual,
            static_cast<std::uint64_t>(exact_phase6_active_frames));
        if(!grade||!original.d03_d04.exact_per_source_grade)return {};
        original.t01.phase6_source_reference_grade[2]=grade->spectrum;
        for(auto& f:original.t02_t06.features)f.reference_grade[2]=grade->spectrum;
        original.d01.phase6_reference_grade[2]=grade->integrated_and_macro;
        (*original.d03_d04.exact_per_source_grade)[2]=grade->integrated_and_macro;
        for(auto& f:original.m19.authority)f.source_reference_grade[2]=grade->integrated_and_macro;
        for(auto& f:original.d07_d09.per_descriptor_source_grade)f[2]=grade->transient;
        for(auto& f:original.d10_d12.source_reference_grade)f[2]=grade->spectrum;
        for(auto& f:original.s01_s02.authority)f.phase6_reference_grade[2]=grade->stereo;
        for(auto& f:original.s03_s06.source_phase6_reference_grade)f[2]=grade->stereo;
    }
    original.m19.actual_candidate=actual;
    original.s01_s02.actual_candidate=actual;
    original.t01.actual_sources[2]=actual;
    original.t02_t06.sources[2].observed_from_real_m20=
        m05m06_real::observe_m05_tonal_bundle(
            tonal,original.t02_t06.exact_common,
            original.t02_t06.exact_accepted_density_floor);
    original.t02_t06.sources[2].selected_spectral_population_identity=
        actual.selected_spectral_population_identity;
    original.t02_t06.sources[2].selected_spectral_frames=
        actual.selected_spectral_frames;
    original.t02_t06.sources[2].complete_spectral_frames=
        actual.complete_spectral_frames;
    original.d01.source[2]=actual;
    original.d02.actual_candidate=m05m06_real::observe_lra_from_real_m19(
        actual,exact_phase6_active_frames,
        phase6_file_based_trailing_policy_attested);
    original.d03_d04.actual_candidate=
        m05m06_real::observe_d03_d04_real_pcm(pcm);
    original.d07_d09.current_sources[2].measured=
        transient.original_d07_d08_d09_type7;
    original.d10_d12.observed[2]=spectral;
    original.s03_s06.observed[2]=stereo;
    auto result=bind_all_original_real_native_p2_triple(original);
    if(result.fragments&&retain_actual_windows)
        *retain_actual_windows=std::move(original.d03_d04.actual_candidate);
    return result;
}
[[nodiscard]] inline P2RealTripleBindingResult
bind_p2_original_native_triple_from_real_candidate(
    P2OriginalRealNativeTriple original,
    const M10P2FullNativeEvidence& measured,
    const m05m06_real::SourceIdentity& current_candidate_product_owner,
    std::int64_t exact_phase6_active_frames,
    bool phase6_file_based_trailing_policy_attested,
    m05m06_real::D03D04NativeSeries* retain_actual_windows=nullptr)
{
    const auto from_real=m05m06_real::import_actual_full_source(
        measured.native.actual.levels,measured.native.actual.tonal,
        current_candidate_product_owner,
        measured.native.selected_original_m20_ordinals);
    if(!from_real.source)return {};
    return bind_original_native_triple_from_measured_source(
        std::move(original),*from_real.source,measured.native.actual.tonal,
        measured.native.actual.rendered.view(),measured.native.d07_d09,
        measured.native.d10_d12,measured.native.s03_s06,
        current_candidate_product_owner,exact_phase6_active_frames,
        phase6_file_based_trailing_policy_attested,retain_actual_windows);
}
// Strong Product path: the original callback carries canonical M07
// definitions/support/grades, but it CANNOT introduce PREPARED or GOLD
// numerical observations. Every one of their ten native families is
// overwritten from the same actual, independent, once-per-generation
// PCM analysis. Candidate C remains rebuilt from the actual P2 render
// by the existing strict candidate binder below.
[[nodiscard]] inline bool
rebind_independent_original_native_pair(
    P2OriginalRealNativeTriple& original,
    const OriginalPreparedReferenceNativeFacts& independent,
    const m05m06_real::SourceIdentity& exact_current_p2_candidate_owner)
{
    const auto& prepared=independent.prepared;
    const auto& reference=independent.reference;
    const auto& p=prepared.original_m19_m20.owner;
    const auto& g=reference.original_m19_m20.owner;
    const auto common=original.t02_t06.exact_common;
    const auto matches_common=[&](const auto& source){
        return source.exact_profile_common_bandwidth.low_hz==common.low_hz&&
               source.exact_profile_common_bandwidth.high_hz==common.high_hz;
    };
    if(!matches_common(prepared)||!matches_common(reference))return false;
    if(!p.full_track_owner_confirmed||!g.full_track_owner_confirmed||
       !p.generation||!g.generation||
       p.resource_identity.empty()||g.resource_identity.empty()||
       p.resource_identity==g.resource_identity||
       exact_current_p2_candidate_owner.resource_identity==
           p.resource_identity||
       exact_current_p2_candidate_owner.resource_identity==
           g.resource_identity)
        return false;
    const std::array<const OriginalOwnedNativeSourceFacts*,2> sources{{
        &prepared,&reference}};
    for(std::size_t i=0;i<2U;++i) {
        const auto& true_owner=sources[i]->original_m19_m20.owner;
        const auto same=[&](const m05m06_real::SourceIdentity& got){
            return exact_source_owner_equal(got,true_owner);
        };
        if(!same(original.t01.actual_sources[i].owner)||
           !same(original.t02_t06.sources[i].owner)||
           !same(original.d01.source[i].owner)||
           !same(original.d07_d09.current_sources[i].owner)||
           !same(original.d10_d12.source_owner[i])||
           !same(original.s03_s06.independent_source_owners[i])||
           !same(i==0U?original.m19.prepared.owner:
                         original.m19.reference.owner)||
           !same(i==0U?original.s01_s02.prepared.owner:
                         original.s01_s02.reference.owner)||
           !same(i==0U?original.d02.prepared_owner:
                         original.d02.reference_owner)||
           !same(i==0U?original.d03_d04.prepared_owner:
                         original.d03_d04.reference_owner))
            return false;
    }
    for(std::size_t i=0;i<2U;++i) {
        const auto& measured=*sources[i];
        const auto& real=measured.original_m19_m20;
        original.t01.actual_sources[i]=real;
        original.t02_t06.sources[i].observed_from_real_m20=
            measured.original_t02_t06;
        original.d01.source[i]=real;
        if(i==0U) {
            original.m19.prepared=real;
            original.s01_s02.prepared=real;
            original.d02.prepared=measured.original_d02_lra;
            original.d03_d04.prepared=
                measured.original_d03_d04_window_series;
        } else {
            original.m19.reference=real;
            original.s01_s02.reference=real;
            original.d02.reference=measured.original_d02_lra;
            original.d03_d04.reference=
                measured.original_d03_d04_window_series;
        }
        original.d07_d09.current_sources[i].measured=
            measured.full_native.d07_d09.original_d07_d08_d09_type7;
        original.d10_d12.observed[i]=measured.full_native.d10_d12;
        original.s03_s06.observed[i]=measured.full_native.s03_s06;
    }
    return true;
}
[[nodiscard]] inline P2RealTripleBindingResult
bind_p2_original_native_triple_from_independent_full_pair(
    P2OriginalRealNativeTriple original,
    const OriginalPreparedReferenceNativeFacts& independent,
    const M10P2FullNativeEvidence& measured_candidate,
    const m05m06_real::SourceIdentity& exact_current_p2_candidate_owner,
    std::int64_t candidate_exact_phase6_active_frames,
    bool candidate_exact_file_based_lra_policy_attested,
    m05m06_real::D03D04NativeSeries* retain_actual_windows=nullptr)
{
    if(!rebind_independent_original_native_pair(
        original,independent,exact_current_p2_candidate_owner))return {};
    // No grade/digest/amount/safety attestation is created here.
    // If any frozen native feature lacks support or its original
    // policy/definition authority, the original M07 assembler refuses J.
    return bind_p2_original_native_triple_from_real_candidate(
        std::move(original),measured_candidate,
        exact_current_p2_candidate_owner,
        candidate_exact_phase6_active_frames,
        candidate_exact_file_based_lra_policy_attested,retain_actual_windows);
}
} // namespace rgsml::app::l3_automatch::m10_frozen
