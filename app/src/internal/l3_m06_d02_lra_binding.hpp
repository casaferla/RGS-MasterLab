#pragma once
// L3-M06 D02 exact EBU native LRA -> existing M07/M08 Macro slot.
// No Type7, new source ID, reference-grade inference or absent
// Phase-6 feature-definition checksum.
#include "l3_m06_real_lra_native.hpp"
#include "l3_m08_frozen_targeted_population.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m05m06_real {
struct LraTripleEvidence final {
    LraNativeObservation prepared,reference,actual_candidate;
    SourceIdentity prepared_owner,reference_owner,candidate_owner;
    bool exact_feature_definition_and_all_source_epochs_current{};
    std::string exact_feature_dependency;
};
enum class LraBindingReason:std::uint8_t{
    ready,unqualified_source_or_revision,invalid_feature_identity
};
struct LraBindingProjection final {
    std::optional<m08_frozen::AuthenticatedPairSlot> slot;
    LraBindingReason reason{LraBindingReason::unqualified_source_or_revision};
    [[nodiscard]] constexpr bool authorizes_plan() const noexcept {
        return false;
    }
};
[[nodiscard]] inline LraBindingProjection
bind_real_d02_lra(const LraTripleEvidence& e)
{
    if(!e.exact_feature_definition_and_all_source_epochs_current||
       !e.prepared_owner.full_track_owner_confirmed||
       !e.reference_owner.full_track_owner_confirmed||
       !e.candidate_owner.full_track_owner_confirmed||
       !e.prepared_owner.generation||!e.reference_owner.generation||
       !e.candidate_owner.generation||
       e.prepared_owner.resource_identity.empty()||
       e.reference_owner.resource_identity.empty()||
       e.candidate_owner.resource_identity.empty()||
       e.prepared_owner.resource_identity==e.reference_owner.resource_identity||
       e.prepared_owner.resource_identity==e.candidate_owner.resource_identity||
       e.reference_owner.resource_identity==e.candidate_owner.resource_identity)
        return {{},LraBindingReason::unqualified_source_or_revision};
    const auto* binding=m07_frozen::find_binding(
        "rgsml.reference-match.binding.dynamics.d02");
    if(!binding||e.exact_feature_dependency.empty())
        return {{},LraBindingReason::invalid_feature_identity};
    const bool source_grade=
        e.prepared.is_reference_grade()&&
        e.reference.is_reference_grade();
    m08_frozen::AuthenticatedPairSlot result{
        std::string{binding->binding_id},
        std::string{binding->source_feature_id},
        e.exact_feature_dependency,true,{}};
    result.configured.push_back({
        source_grade,source_grade?1.0:0.0,
        {source_grade?e.prepared.lra_lu:std::nullopt,
         m08_frozen::Endpoint::ordinary},
        {source_grade?e.reference.lra_lu:std::nullopt,
         m08_frozen::Endpoint::ordinary},
        e.actual_candidate.is_reference_grade()
            ?e.actual_candidate.lra_lu:std::nullopt,
        true});
    auto& native=result.configured.back().candidate_native;
    native.value=result.configured.back().current_candidate_z;
    native.selector="EBU-P95-P10";
    native.valid_count=e.actual_candidate.relative_retained_windows;
    native.configured_count=e.actual_candidate.absolute_gated_windows;
    return {std::move(result),LraBindingReason::ready};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
