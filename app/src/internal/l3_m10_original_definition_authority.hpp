#pragma once
// Keep only the original definition and source ownership authority used
// by the measured candidate. All numerical P/G/C readings are rebuilt
// from independently owned PCM by the existing native binders.
#include "l3_m10_p2_real_native_triple_binding.hpp"
#include "l3_m10_p2_exact_cache_evaluator.hpp"
#include <bit>
#include <memory>

namespace rgsml::app::l3_automatch::m10_frozen {
[[nodiscard]] inline P2OriginalRealNativeTriple retain_original_native_definitions(
    const P2OriginalRealNativeTriple& input)
{
    P2OriginalRealNativeTriple out;
    out.t01.authoritative_primary_grid=input.t01.authoritative_primary_grid;
    out.t01.exact_native_band_support_prepared_gold=
        input.t01.exact_native_band_support_prepared_gold;
    out.t01.phase6_source_reference_grade=input.t01.phase6_source_reference_grade;
    out.t01.exact_primary_t01_feature_dependency=input.t01.exact_primary_t01_feature_dependency;
    out.t01.exact_phase6_grid_and_native_support_current=
        input.t01.exact_phase6_grid_and_native_support_current;
    out.t02_t06.exact_common=input.t02_t06.exact_common;
    out.t02_t06.exact_accepted_density_floor=input.t02_t06.exact_accepted_density_floor;
    out.t02_t06.features=input.t02_t06.features;
    out.d01.phase6_reference_grade=input.d01.phase6_reference_grade;
    out.d01.exact_feature_dependency=input.d01.exact_feature_dependency;
    out.d01.exact_phase6_feature_revision_and_context_current=
        input.d01.exact_phase6_feature_revision_and_context_current;
    out.d02.prepared_owner=input.d02.prepared_owner;
    out.d02.reference_owner=input.d02.reference_owner;
    out.d02.candidate_owner=input.d02.candidate_owner;
    out.d02.exact_feature_definition_and_all_source_epochs_current=
        input.d02.exact_feature_definition_and_all_source_epochs_current;
    out.d02.exact_feature_dependency=input.d02.exact_feature_dependency;
    out.d03_d04.prepared_owner=input.d03_d04.prepared_owner;
    out.d03_d04.reference_owner=input.d03_d04.reference_owner;
    out.d03_d04.candidate_owner=input.d03_d04.candidate_owner;
    out.d03_d04.exact_phase6_feature_definition_current=
        input.d03_d04.exact_phase6_feature_definition_current;
    out.d03_d04.all_three_source_reference_grade_attested=
        input.d03_d04.all_three_source_reference_grade_attested;
    out.d03_d04.exact_feature_dependency=input.d03_d04.exact_feature_dependency;
    out.d07_d09.exact_feature_definition_digests=input.d07_d09.exact_feature_definition_digests;
    out.d07_d09.per_descriptor_source_grade=input.d07_d09.per_descriptor_source_grade;
    out.d07_d09.same_exact_p04_policy_context_and_generation_attested=
        input.d07_d09.same_exact_p04_policy_context_and_generation_attested;
    out.d10_d12.source_owner=input.d10_d12.source_owner;
    out.d10_d12.source_reference_grade=input.d10_d12.source_reference_grade;
    out.d10_d12.canonical_feature_dependency=input.d10_d12.canonical_feature_dependency;
    out.d10_d12.exact_current_full_stft_grid_and_policy_attested=
        input.d10_d12.exact_current_full_stft_grid_and_policy_attested;
    out.m19.prepared.owner=input.m19.prepared.owner;
    out.m19.reference.owner=input.m19.reference.owner;
    out.m19.actual_candidate.owner=input.m19.actual_candidate.owner;
    out.m19.authority=input.m19.authority;
    out.s01_s02.prepared.owner=input.s01_s02.prepared.owner;
    out.s01_s02.reference.owner=input.s01_s02.reference.owner;
    out.s01_s02.actual_candidate.owner=input.s01_s02.actual_candidate.owner;
    out.s01_s02.authority=input.s01_s02.authority;
    out.s03_s06.independent_source_owners=input.s03_s06.independent_source_owners;
    out.s03_s06.exact_canonical_band_indices=input.s03_s06.exact_canonical_band_indices;
    out.s03_s06.source_phase6_reference_grade=input.s03_s06.source_phase6_reference_grade;
    out.s03_s06.immutable_feature_definition_digests=input.s03_s06.immutable_feature_definition_digests;
    out.s03_s06.exact_current_common_band_grid_and_retained_gate_policy=
        input.s03_s06.exact_current_common_band_grid_and_retained_gate_policy;
    for(std::size_t j=0;j<3U;++j) {
        out.t01.actual_sources[j].owner=input.t01.actual_sources[j].owner;
        out.t02_t06.sources[j].owner=input.t02_t06.sources[j].owner;
        out.d01.source[j].owner=input.d01.source[j].owner;
        out.d07_d09.current_sources[j].owner=input.d07_d09.current_sources[j].owner;
    }
    return out;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
