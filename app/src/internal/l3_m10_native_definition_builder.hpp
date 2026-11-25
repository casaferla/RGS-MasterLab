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
} // namespace rgsml::app::l3_automatch::m10_frozen
