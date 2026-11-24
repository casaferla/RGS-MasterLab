#pragma once
// M06 D03/D04 actual PCM observations -> original M07/M08 binding IDs.
// No invented Phase-6 checksum, M08 optimization, M10 score or M13 proof.
#include "l3_m06_real_windowed_rms_crest.hpp"
#include "l3_m08_frozen_targeted_population.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m05m06_real {
struct D03D04FullEvidence final {
    D03D04NativeSeries prepared,reference,actual_candidate;
    SourceIdentity prepared_owner,reference_owner,candidate_owner;
    bool exact_phase6_feature_definition_current{};
    bool all_three_source_reference_grade_attested{};
    std::string exact_feature_dependency;
};
enum class D03D04SlotFailure:std::uint8_t {
    none,stale_or_unauthenticated_input,invalid_real_series,
    invalid_feature_dependency
};
struct D03D04SlotProjection final {
    std::optional<std::array<m08_frozen::AuthenticatedPairSlot,2>> feature_slots;
    D03D04SlotFailure reason{D03D04SlotFailure::stale_or_unauthenticated_input};
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
};
[[nodiscard]] inline D03D04SlotProjection
make_real_d03_d04_m07_m08_slots(const D03D04FullEvidence& e)
{
    if(!e.exact_phase6_feature_definition_current||
       !e.all_three_source_reference_grade_attested||
       !e.candidate_owner.full_track_owner_confirmed||
       !e.candidate_owner.generation||
       e.candidate_owner.resource_identity.empty()||
       e.candidate_owner.resource_identity==e.prepared_owner.resource_identity||
       e.candidate_owner.resource_identity==e.reference_owner.resource_identity)
        return {{},D03D04SlotFailure::stale_or_unauthenticated_input};
    if(e.exact_feature_dependency.empty())
        return {{},D03D04SlotFailure::invalid_feature_dependency};
    const auto pair=pair_d03_d04_native(
        e.prepared,e.reference,e.prepared_owner,e.reference_owner);
    if(!pair||e.actual_candidate.problem!=NativeWindowProblem::none)
        return {{},D03D04SlotFailure::invalid_real_series};
    const auto candidate=extract_d03_d04_type7(e.actual_candidate);
    const auto* d03=m07_frozen::find_binding(
        "rgsml.reference-match.binding.dynamics.d03");
    const auto* d04=m07_frozen::find_binding(
        "rgsml.reference-match.binding.dynamics.d04");
    if(!d03||!d04)return {{},D03D04SlotFailure::invalid_feature_dependency};
    const auto as_slot=[&](const m07_frozen::Binding& binding) {
        return m08_frozen::AuthenticatedPairSlot{
            std::string{binding.binding_id},
            std::string{binding.source_feature_id},
            e.exact_feature_dependency,true,{}};
    };
    std::array<m08_frozen::AuthenticatedPairSlot,2> feature_slots{
        as_slot(*d03),as_slot(*d04)};
    const auto add=[](m08_frozen::AuthenticatedPairSlot& to,
        std::optional<double> prepared,std::optional<double> gold,
        std::optional<double> observed,std::optional<double> support) {
        to.configured.push_back({
            static_cast<bool>(prepared&&gold&&support&&*support>0),
            support?*support:0.0,
            {prepared,m08_frozen::Endpoint::ordinary},
            {gold,m08_frozen::Endpoint::ordinary},observed,true});
    };
    add(feature_slots[0],pair->prepared.d03_type7_rms_p95_minus_p10_db,
        pair->reference.d03_type7_rms_p95_minus_p10_db,
        candidate.d03_type7_rms_p95_minus_p10_db,
        pair->paired_d03_support);
    for(std::size_t i=0;i<3U;++i)
        add(feature_slots[1],
            pair->prepared.d04_type7_crest_p10_p50_p90_db[i],
            pair->reference.d04_type7_crest_p10_p50_p90_db[i],
            candidate.d04_type7_crest_p10_p50_p90_db[i],
            pair->paired_d04_support);
    auto& rms=feature_slots[0].configured.front().candidate_native;
    rms.value=candidate.d03_type7_rms_p95_minus_p10_db;
    rms.valid_count=candidate.valid_rms_windows;
    rms.configured_count=candidate.complete_native_windows;
    rms.selector="P95-P10";
    for(std::size_t i=0;i<3U;++i) {
        auto& crest=feature_slots[1].configured[i].candidate_native;
        crest.value=candidate.d04_type7_crest_p10_p50_p90_db[i];
        crest.valid_count=candidate.valid_crest_windows;
        crest.configured_count=candidate.complete_native_windows;
        crest.selector=std::array<const char*,3>{{"P10","P50","P90"}}[i];
    }
    return {std::move(feature_slots),D03D04SlotFailure::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
