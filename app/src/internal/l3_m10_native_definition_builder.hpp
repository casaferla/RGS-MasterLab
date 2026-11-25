#pragma once
#include "l3_m10_original_definition_authority.hpp"
#include "l3_m10_native_source_grade.hpp"

namespace rgsml::app::l3_automatch::m10_frozen {
struct NativeDefinitionRecord final {
    std::string binding_id;
    std::string source_feature_id;
    std::string exact_dependency;
};
// Immutable accepted definition inventory from the Product owner.
// No new feature hashes or substitute registry identifiers are generated.
struct NativeDefinitionPolicy final {
    std::array<NativeDefinitionRecord,27> records;
    double exact_density_floor{};
    bool exact_current_registry_and_phase6_policy_attested{};
};
[[nodiscard]] inline bool valid_native_definition_policy(const NativeDefinitionPolicy& policy)
{
    if(!policy.exact_current_registry_and_phase6_policy_attested||
       !std::isfinite(policy.exact_density_floor)||policy.exact_density_floor<=0.0)return false;
    for(std::size_t i=0;i<policy.records.size();++i) {
        const auto& r=policy.records[i];const auto& b=m07_frozen::kBindings[i];
        // Primary profile only: a coarse fallback requires its own
        // native measurement adapter and cannot masquerade as primary.
        if(r.binding_id!=b.binding_id||r.source_feature_id!=b.source_feature_id||
           r.exact_dependency.empty())return false;
    }
    // Existing D03/D04 native family owns one shared definition identity.
    return policy.records[8].exact_dependency==policy.records[9].exact_dependency;
}
[[nodiscard]] inline std::optional<std::vector<m05m06_real::T01ConfiguredBand>>
native_common_primary_grid(const OriginalPreparedReferenceNativeFacts& pair)
{
    const auto common=pair.prepared.exact_profile_common_bandwidth;
    if(common.low_hz!=pair.reference.exact_profile_common_bandwidth.low_hz||
       common.high_hz!=pair.reference.exact_profile_common_bandwidth.high_hz||
       !std::isfinite(common.low_hz)||!std::isfinite(common.high_hz)||common.low_hz>=common.high_hz)return {};
    const auto& p=pair.prepared.original_m19_m20.profile_1_12;
    const auto& g=pair.reference.original_m19_m20.profile_1_12;
    std::vector<m05m06_real::T01ConfiguredBand> out;
    for(const auto& band:p) {
        if(band.low_hz<common.low_hz||band.high_hz>common.high_hz)continue;
        const auto it=std::find_if(g.begin(),g.end(),[&](const auto& b){
            return b.canonical_index==band.canonical_index;});
        if(it==g.end()||it->low_hz!=band.low_hz||it->center_hz!=band.center_hz||
           it->high_hz!=band.high_hz)return {};
        out.push_back({band.canonical_index,band.low_hz,band.center_hz,band.high_hz});
    }
    if(out.empty())return {};
    return out;
}
[[nodiscard]] inline std::vector<std::optional<double>>
native_primary_component_support(std::span<const m05m06_real::T01ConfiguredBand> grid,
    const m05m06_real::SourceReadings& source)
{
    std::vector<std::optional<double>> out;
    for(const auto& band:grid) {
        const auto it=std::find_if(source.profile_1_12.begin(),source.profile_1_12.end(),
            [&](const auto& b){return b.canonical_index==band.canonical_index;});
        // Full component availability is represented exactly once. Do
        // not multiply again by a profile-wide frequency coverage ratio.
        if(it!=source.profile_1_12.end()&&it->low_hz==band.low_hz&&
           it->center_hz==band.center_hz&&it->high_hz==band.high_hz&&it->power_fraction_db.ready())
            out.emplace_back(1.0);
        else out.emplace_back(std::nullopt);
    }
    return out;
}
[[nodiscard]] inline std::optional<P2OriginalRealNativeTriple>
build_current_native_definitions(const NativeDefinitionPolicy& policy,
    const OriginalPreparedReferenceNativeFacts& pair,const M10P2FullNativeEvidence& fixed,
    const OfflineProductOwnerStamp& owner,const std::string& ticket,
    const ProductNativeOnePassSpec& spec)
{
    using namespace m05m06_real;
    if(!valid_native_definition_policy(policy)||ticket.empty()||
       !original_pair_matches_product_owner(pair,owner)||
       !spec.current_full_phase6_source_and_grid||!spec.derive_own_m19_temporal_gate)return {};
    const auto candidate=native_product_candidate_identity(owner,ticket);
    const auto actual=import_actual_full_source(fixed.native.actual.levels,
        fixed.native.actual.tonal,candidate,fixed.native.selected_original_m20_ordinals);
    if(!actual.source)return {};
    const auto grid=native_common_primary_grid(pair);
    if(!grid||spec.spectral_bands.empty()||spec.stereo_bands.empty())return {};
    const std::array<const SourceReadings*,3> sources{
        &pair.prepared.original_m19_m20,&pair.reference.original_m19_m20,&*actual.source};
    const std::array<std::uint64_t,3> active{
        pair.prepared.full_native.exact_observed_active_frames,
        pair.reference.full_native.exact_observed_active_frames,
        fixed.native.exact_observed_active_frames};
    std::array<NativeDurationGrade,3> grades;
    for(std::size_t i=0;i<3U;++i) {
        auto grade=native_full_source_duration_grade(*sources[i],active[i]);
        if(!grade)return {};
        grades[i]=*grade;
    }
    const auto flags=[&](bool NativeDurationGrade::* member){
        return std::array<bool,3>{grades[0].*member,grades[1].*member,grades[2].*member};
    };
    const auto macro=flags(&NativeDurationGrade::integrated_and_macro);
    const auto tonal=flags(&NativeDurationGrade::spectrum);
    const auto stereo=flags(&NativeDurationGrade::stereo);
    const auto transient=flags(&NativeDurationGrade::transient);
    const auto dep=[&](std::size_t i)->const std::string&{return policy.records[i].exact_dependency;};
    P2OriginalRealNativeTriple out;
    out.t01.authoritative_primary_grid=*grid;
    for(std::size_t i=0;i<2U;++i)
        out.t01.exact_native_band_support_prepared_gold[i]=native_primary_component_support(*grid,*sources[i]);
    out.t01.phase6_source_reference_grade=tonal;
    out.t01.exact_primary_t01_feature_dependency=dep(0);
    out.t01.exact_phase6_grid_and_native_support_current=true;
    out.t02_t06.exact_common=pair.prepared.exact_profile_common_bandwidth;
    out.t02_t06.exact_accepted_density_floor=policy.exact_density_floor;
    for(std::size_t i=0;i<5U;++i)out.t02_t06.features[i]={dep(i+1U),true,tonal};
    out.d01.phase6_reference_grade=macro;
    out.d01.exact_feature_dependency=dep(6);
    out.d01.exact_phase6_feature_revision_and_context_current=true;
    out.d02.exact_feature_definition_and_all_source_epochs_current=true;
    out.d02.exact_feature_dependency=dep(7);
    out.d03_d04.exact_phase6_feature_definition_current=true;
    out.d03_d04.all_three_source_reference_grade_attested=macro[0]&&macro[1]&&macro[2];
    out.d03_d04.exact_feature_dependency=dep(8);
    for(std::size_t i=0;i<3U;++i) {
        out.d07_d09.exact_feature_definition_digests[i]=dep(12U+i);
        out.d07_d09.per_descriptor_source_grade[i]=transient;
        out.d10_d12.canonical_feature_dependency[i]=dep(15U+i);
        out.d10_d12.source_reference_grade[i]=tonal;
    }
    out.d07_d09.same_exact_p04_policy_context_and_generation_attested=true;
    out.d10_d12.exact_current_full_stft_grid_and_policy_attested=true;
    constexpr std::array<std::size_t,5> m19{10,11,24,25,26};
    for(std::size_t i=0;i<m19.size();++i)
        out.m19.authority[i]={dep(m19[i]),true,macro};
    for(std::size_t i=0;i<2U;++i)out.s01_s02.authority[i]={dep(18U+i),true,stereo};
    for(const auto& band:spec.stereo_bands)
        out.s03_s06.exact_canonical_band_indices.push_back(band.exact_canonical_index);
    for(std::size_t i=0;i<4U;++i) {
        out.s03_s06.immutable_feature_definition_digests[i]=dep(20U+i);
        out.s03_s06.source_phase6_reference_grade[i]=stereo;
    }
    out.s03_s06.exact_current_common_band_grid_and_retained_gate_policy=true;
    for(std::size_t i=0;i<3U;++i) {
        const auto& s=sources[i]->owner;
        out.t01.actual_sources[i].owner=s;out.t02_t06.sources[i].owner=s;
        out.d01.source[i].owner=s;out.d07_d09.current_sources[i].owner=s;
        out.d10_d12.source_owner[i]=s;out.s03_s06.independent_source_owners[i]=s;
    }
    out.m19.prepared.owner=sources[0]->owner;out.m19.reference.owner=sources[1]->owner;
    out.m19.actual_candidate.owner=candidate;
    out.s01_s02.prepared.owner=sources[0]->owner;out.s01_s02.reference.owner=sources[1]->owner;
    out.s01_s02.actual_candidate.owner=candidate;
    out.d02.prepared_owner=sources[0]->owner;out.d02.reference_owner=sources[1]->owner;
    out.d02.candidate_owner=candidate;
    out.d03_d04.prepared_owner=sources[0]->owner;out.d03_d04.reference_owner=sources[1]->owner;
    out.d03_d04.candidate_owner=candidate;
    return out;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
