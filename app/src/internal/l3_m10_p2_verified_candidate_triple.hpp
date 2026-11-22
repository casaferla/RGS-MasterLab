#pragma once
// One Product candidate M05/M06 source provenance bridge.
// PREPARED/REFERENCE and Phase6 definition flags remain under the
// authenticated Product owner. CANDIDATE feature values are REBUILT
// from this exact P2 render, M19, M20, P04 and PCM only: never accept
// numerical candidate readings fabricated by the original-triple
// resolver. No M21 proxy, second rendering, M17 WAV Export, M12 receipt.
#include "l3_m10_p2_real_native_triple_binding.hpp"
#include "l3_m10_p2_full_native_evidence.hpp"
#include <cstdint>
#include <utility>

namespace rgsml::app::l3_automatch::m10_frozen {
[[nodiscard]] inline P2RealTripleBindingResult
bind_p2_original_native_triple_from_real_candidate(
    P2OriginalRealNativeTriple original,
    const M10P2FullNativeEvidence& measured,
    const m05m06_real::SourceIdentity& current_candidate_product_owner,
    std::int64_t exact_phase6_active_frames,
    bool phase6_file_based_trailing_policy_attested)
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
    const auto from_real=measured.native.actual.observed_m05m06(
        current_candidate_product_owner);
    if(!from_real.source)
        return {};
    // Owner-supplied PREPARED/REFERENCE observations are untouched,
    // as are all authoritative M07 digests, support masks and grids.
    const auto& actual=*from_real.source;
    original.m19.actual_candidate=actual;
    original.s01_s02.actual_candidate=actual;
    original.t01.actual_sources[2]=actual;
    original.t02_t06.sources[2].observed_from_real_m20=
        measured.native.actual.observed_m05_tonal(
            original.t02_t06.exact_common,
            original.t02_t06.exact_accepted_density_floor);
    original.d01.source[2]=actual;
    original.d02.actual_candidate=m05m06_real::observe_lra_from_real_m19(
        actual,exact_phase6_active_frames,
        phase6_file_based_trailing_policy_attested);
    original.d03_d04.actual_candidate=
        measured.native.actual.observed_m06_native_rms_crest();
    original.d07_d09.current_sources[2].measured=
        measured.native.d07_d09.original_d07_d08_d09_type7;
    original.d10_d12.observed[2]=measured.native.d10_d12;
    original.s03_s06.observed[2]=measured.native.s03_s06;
    return bind_all_original_real_native_p2_triple(original);
}
} // namespace rgsml::app::l3_automatch::m10_frozen
