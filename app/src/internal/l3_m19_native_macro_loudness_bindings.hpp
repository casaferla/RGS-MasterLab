#pragma once
// L3 M05/M06 M19 source -> exact M07 D05,D06,L01,L02,L03 feature_slots.
// Existing M19 scope/quality, M05/06 reference-grade and immutable
// Phase6 feature content are required. No new LUFS/TP/PLR estimator,
// no Type7-to-scalar support substitution, no missing-M16 promotion.
#include "l3_m05m06_real_analysis_port.hpp"
#include "l3_m08_frozen_targeted_population.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace rgsml::app::l3_automatch::m05m06_real {
inline constexpr std::array<std::string_view,5> kM19NativeBindingIds{{
    "rgsml.reference-match.binding.dynamics.d05",
    "rgsml.reference-match.binding.dynamics.d06",
    "rgsml.reference-match.binding.loudness.l01",
    "rgsml.reference-match.binding.loudness.l02",
    "rgsml.reference-match.binding.loudness.l03"
}};
struct M19NativeFeatureAuthority final {
    std::string exact_feature_dependency;
    bool current_phase6_definition_and_revision{};
    std::array<bool,3> source_reference_grade{}; // Prepared, Gold, Candidate
};
struct M19NativeTriple final {
    SourceReadings prepared,reference,actual_candidate;
    std::array<M19NativeFeatureAuthority,5> authority;
};
enum class M19SlotFailure:std::uint8_t {
    none,stale_or_unauthenticated_owners,invalid_feature_authority
};
struct M19NativeSlotProjection final {
    std::optional<std::array<m08_frozen::AuthenticatedPairSlot,5>> feature_slots;
    M19SlotFailure reason{M19SlotFailure::stale_or_unauthenticated_owners};
    [[nodiscard]] constexpr bool authorizes_solver_or_publish()
        const noexcept {return false;}
};
[[nodiscard]] inline bool source_owner_ready(
    const SourceReadings& s) noexcept {
    return s.owner.full_track_owner_confirmed &&
        s.owner.generation>0 &&
        !s.owner.resource_identity.empty() &&
        s.sample_rate_hz>0 &&
        s.scope_frame_count>0;
}
[[nodiscard]] inline M19NativeSlotProjection bind_real_m19_loudness_macro(
    const M19NativeTriple& e)
{
    const std::array<const SourceReadings*,3> owners{{
        &e.prepared,&e.reference,&e.actual_candidate}};
    for(const auto* owner:owners)
        if(!source_owner_ready(*owner))
            return {{},M19SlotFailure::stale_or_unauthenticated_owners};
    if(e.prepared.owner.resource_identity==
           e.reference.owner.resource_identity||
       e.prepared.owner.resource_identity==
           e.actual_candidate.owner.resource_identity||
       e.reference.owner.resource_identity==
           e.actual_candidate.owner.resource_identity)
        return {{},M19SlotFailure::stale_or_unauthenticated_owners};
    std::array<m08_frozen::AuthenticatedPairSlot,5> feature_slots;
    for(std::size_t i=0;i<feature_slots.size();++i) {
        const auto* binding=m07_frozen::find_binding(
            kM19NativeBindingIds[i]);
        if(!binding||
           !e.authority[i].current_phase6_definition_and_revision||
           e.authority[i].exact_feature_dependency.empty())
            return {{},M19SlotFailure::invalid_feature_authority};
        feature_slots[i].binding_id=std::string{binding->binding_id};
        feature_slots[i].source_feature_id=std::string{binding->source_feature_id};
        feature_slots[i].exact_feature_dependency=
            e.authority[i].exact_feature_dependency;
        feature_slots[i].feature_authority_attested=true;
        std::array<std::optional<double>,3> coords{};
        std::array<bool,3> ready{};
        for(std::size_t j=0;j<owners.size();++j) {
            const auto& s=*owners[j];
            const Value* observed=nullptr;
            switch(i) {
            case 0:observed=&s.plr;break;
            case 1:observed=&s.global_crest_db;break;
            case 2:observed=&s.integrated_lufs;break;
            case 3:observed=&s.short_term_p50_lufs;break;
            case 4:observed=&s.true_peak_db_tp;break;
            default:break;
            }
            ready[j]=observed&&observed->ready()&&
                     e.authority[i].source_reference_grade[j];
            if(ready[j])coords[j]=observed->unrounded;
        }
        double support=1.0;
        if(i==3U) {
            // Loudness L02 owns native ST valid/complete support; its
            // pair support MIN is NOT qualityScore or arbitrary 1.0.
            if(!e.prepared.short_term_native_support||
               !e.reference.short_term_native_support)
                support=0.0;
            else {
                support=std::min(
                    *e.prepared.short_term_native_support,
                    *e.reference.short_term_native_support);
                if(!std::isfinite(support)||support<0.0||support>1.0)
                    return {{},M19SlotFailure::invalid_feature_authority};
            }
        }
        const bool pair_valid=ready[0]&&ready[1]&&support>0.0;
        feature_slots[i].configured.push_back({
            pair_valid,pair_valid?support:0.0,
            {coords[0],m08_frozen::Endpoint::ordinary},
            {coords[1],m08_frozen::Endpoint::ordinary},
            coords[2],true});
    }
    return {std::move(feature_slots),M19SlotFailure::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
