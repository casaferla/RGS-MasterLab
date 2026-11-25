#pragma once
// L3 M10 real P2 candidate -> exact M07 27-slot generation -> M08
// desired target -> frozen M11 protected/full safety -> measured M10 J.
// The native Product candidate is rendered and analyzed exactly once;
// all original source-specific M05/M06 bindings must be delivered by
// the owner-authenticated native triple resolver. Missing fragments
// never acquire an M10 score, even if M21 shows nine plausible values.
// This does NOT publish a Product graph, alter M17 or verify M13.
#include "l3_m10_p2_full_native_evidence.hpp"
#include "l3_m10_p2_actual_m16_guard.hpp"
#include "l3_m11_actual_native_stereo_safety.hpp"
#include "l3_m10_p2_verified_candidate_triple.hpp"
#include "l3_m10_original_definition_authority.hpp"
#include "l3_m07_existing_native_fragment_collector.hpp"
#include "l3_m10_authoritative_candidate_cost.hpp"
#include "l3_m10_cache_aware_evaluator.hpp"
#include <atomic>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m10_frozen {
enum class P2NativeCostReason:std::uint8_t {
    measured,stale_or_unattested_product,
    real_render_or_analysis_failed,
    missing_triple_native_authority,
    incomplete_or_conflicting_m07,
    m08_or_m11_safety_rejected
};
struct P2NativeCostResult final {
    P2NativeCostReason reason{P2NativeCostReason::stale_or_unattested_product};
    m07_frozen::PartialFeatureInventory m07;
    std::optional<MeasuredOrderKey> measured;
    std::optional<TargetCost> target;
    [[nodiscard]] bool complete_measured() const noexcept {
        return reason==P2NativeCostReason::measured&&measured&&
               valid_key(*measured);
    }
    [[nodiscard]] FreshCompleteEvidence as_exact_cache_evidence() const {
        return FreshCompleteEvidence{measured,complete_measured()};
    }
    [[nodiscard]] constexpr bool authorizes_m12_or_m13()
        const noexcept{return false;}
};
// Small, testable and strict authority boundary. M11 supplies the
// real independently attested protected K, native raw stereo safety,
// optimization target policy, and topology/complexity. This function
// supplies the ORIGINAL actual M07 27 slots, never caller-supplied slots.
// It cannot fake a successful M11 check: score_exact_candidate_population
// performs all of its own mandatory checks against this slot population.
[[nodiscard]] inline P2NativeCostResult
score_complete_p2_native_fragment_set(
    m07_frozen::ExistingNativeSourceFragments actual_triple_native,
    AuthoritativeCostInput independently_attested_m08_m11,
    std::string exact_phase6_m07_generation,
    bool current_product_source_reference_and_candidate_attested)
{
    if(!current_product_source_reference_and_candidate_attested||
       exact_phase6_m07_generation.empty())
        return {P2NativeCostReason::stale_or_unattested_product,
                {},{},{} };
    m07_frozen::ExactTwentySevenFeatureAssembler generation{
        std::move(exact_phase6_m07_generation),true};
    const auto admitted=m07_frozen::collect_existing_m05m06_native_fragments(
        generation,actual_triple_native);
    if(admitted.status!=
       m07_frozen::NativeFragmentAdmission::accepted_complete)
        return {P2NativeCostReason::incomplete_or_conflicting_m07,
                admitted.inventory,{},{} };
    auto exact=generation.complete_canonical_population();
    if(!exact)
        return {P2NativeCostReason::incomplete_or_conflicting_m07,
                admitted.inventory,{},{} };
    // The Product callback must never be allowed to replace the
    // authenticated native M07 slots with unrelated numerical scores.
    independently_attested_m08_m11.exact_pair_slots=std::move(*exact);
    const auto cost=score_exact_candidate_population(
        independently_attested_m08_m11);
    if(cost.reason!=AuthoritativeCostReason::measured||
       !cost.measured||!valid_key(*cost.measured))
        return {P2NativeCostReason::m08_or_m11_safety_rejected,
                admitted.inventory,{},{} };
    return {P2NativeCostReason::measured,admitted.inventory,
            cost.measured,cost.target_evidence};
}

// The REAL worker half: render actual fixed->REF_LINKED->P2, observe
// independent M16 and shared M19/M20/P04, then hand the exact PCM-derived
// measurements to the owner-provided ten real source binding projections.
// Calling this never grants owner authority to a raw callback by itself.
// The Product owner MUST resolve and attest all three source generations,
// immutable per-feature digests and target/M11 evidence separately.
template<typename ResolveAllRealNativeTripleFragments>
[[nodiscard]] inline P2NativeCostResult
measure_and_score_m10_p2_full_native_candidate(
    const M10P2RealNativeProductJob& candidate,
    ProductNativeOnePassSpec native_spec,
    const M10P2ExactPrefix* exact_pre_p2_prefix,
    std::string exact_phase6_m07_generation,
    bool current_product_source_reference_and_candidate_attested,
    AuthoritativeCostInput independently_attested_m08_m11,
    double exact_current_m11_final_tp_safety_ceiling_db_tp,
    m05m06_real::SourceIdentity verified_candidate_product_owner,
    const OriginalPreparedReferenceNativeFacts*
        authentic_prepared_and_independent_reference,
    bool phase6_file_based_lra_trailing_attested,
    ResolveAllRealNativeTripleFragments&& resolve_original_m05m06,
    const std::atomic<bool>* cancellation=nullptr,
    const M10ExactFixedPrefix* exact_fixed_prefix=nullptr)
{
    if(!current_product_source_reference_and_candidate_attested||
       exact_phase6_m07_generation.empty()||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return {P2NativeCostReason::stale_or_unattested_product,
                {},{},{} };
    // Bind original P/G to current real resource+selection identity
    // and C to the exact typed M10 candidate ticket BEFORE rendering.
    // A reused old generation or fake current identity cannot enter J.
    if(!authentic_prepared_and_independent_reference||
       !original_pair_matches_product_owner(
           *authentic_prepared_and_independent_reference,
           candidate.owner_stamp())||
       !authentic_product_candidate_identity(
           verified_candidate_product_owner,candidate.owner_stamp(),
           candidate.ticket_identity()))
        return {P2NativeCostReason::missing_triple_native_authority,
                {},{},{} };
    if(!bind_original_pair_profile_common(native_spec,*authentic_prepared_and_independent_reference))
        return {P2NativeCostReason::missing_triple_native_authority,{},{},{}};
    const auto linked_graph=candidate.exact_linked_graph_content();
    const auto execution_graph=candidate.exact_execution_graph_content();
    if(!linked_graph||!execution_graph)
        return {P2NativeCostReason::stale_or_unattested_product,{},{},{}};
    independently_attested_m08_m11.native_linked_graph_content=*linked_graph;
    independently_attested_m08_m11.native_execution_graph_content=*execution_graph;
    auto measured=measure_m10_p2_full_native_once(
        candidate,std::move(native_spec),exact_pre_p2_prefix,cancellation,exact_fixed_prefix);
    if(!measured)
        return {P2NativeCostReason::real_render_or_analysis_failed,
                {},{},{} };
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return {P2NativeCostReason::stale_or_unattested_product,
                {},{},{} };
    const auto exact_active_frames=static_cast<std::int64_t>(
        measured.value()->native.exact_observed_active_frames);
    // Independent post-P2 M16 is a mandatory M11 hard constraint,
    // checked before the expensive 27-slot projection. A claimed
    // prior/comparison metric or the M17 export ceiling cannot pass.
    if(!check_m10_p2_native_actual_m16(
           *measured.value(),
           exact_current_m11_final_tp_safety_ceiling_db_tp).passes())
        return {P2NativeCostReason::m08_or_m11_safety_rejected,
                {},{},{} };
    independently_attested_m08_m11.native_independent_full_m16_linear_peak=
        measured.value()->m16_actual_p2_output.global_linear_peak();
    independently_attested_m08_m11.native_independent_m11_ceiling_db_tp=
        exact_current_m11_final_tp_safety_ceiling_db_tp;
    // Resolver delivers the original owner-checked native Prepared/G
    // family and all source-specific feature definitions. Rebind EVERY
    // candidate family from this exact real P2 render, not from any
    // callback-supplied numerical C. M11 retains independent hard gates.
    // The resolver MUST return optional<P2OriginalRealNativeTriple>.
    auto original_triple=std::invoke(
        std::forward<ResolveAllRealNativeTripleFragments>(
            resolve_original_m05m06),*measured.value());
    if(!original_triple)
        return {P2NativeCostReason::missing_triple_native_authority,
                {},{},{} };
    auto definitions=std::make_shared<const P2OriginalRealNativeTriple>(
        retain_original_native_definitions(*original_triple));
    independently_attested_m08_m11.native_definition_authority_content=
        exact_original_native_definition_content(*definitions);
    independently_attested_m08_m11.native_original_definition_authority=std::move(definitions);
    auto bound=bind_p2_original_native_triple_from_independent_full_pair(
        std::move(*original_triple),
        *authentic_prepared_and_independent_reference,
        *measured.value(),verified_candidate_product_owner,
        exact_active_frames,phase6_file_based_lra_trailing_attested);
    if(!bound.fragments)
        return {P2NativeCostReason::missing_triple_native_authority,
                {},{},{} };
    const auto& original_prepared=authentic_prepared_and_independent_reference->prepared;
    if(original_prepared.original_m19_m20.channels==2U) {
        const auto actual=m05m06_real::import_actual_full_source(
            measured.value()->native.actual.levels,measured.value()->native.actual.tonal,
            verified_candidate_product_owner,
            measured.value()->native.selected_original_m20_ordinals);
        if(!actual.source||!independently_attested_m08_m11.native_stereo_raw_safety)
            return {P2NativeCostReason::m08_or_m11_safety_rejected,{},{},{}};
        const auto& policy=*independently_attested_m08_m11.native_stereo_raw_safety;
        independently_attested_m08_m11.native_stereo_raw_safety_required_by_current_owner=true;
        independently_attested_m08_m11.native_stereo_raw_safety=
            m11_frozen::bind_actual_native_stereo_safety(
                original_prepared.original_m19_m20,original_prepared.full_native.s03_s06,
                *actual.source,measured.value()->native.s03_s06,
                policy.exact_current_source_candidate_and_canonical_safety_revision,
                policy.no_forbidden_decorrelation_or_haas);
    }
    independently_attested_m08_m11.native_candidate_resource_identity=
        verified_candidate_product_owner.resource_identity;
    independently_attested_m08_m11.native_candidate_generation=
        verified_candidate_product_owner.generation;
    independently_attested_m08_m11.native_candidate_ticket=candidate.ticket_identity();
    independently_attested_m08_m11.native_m07_generation=exact_phase6_m07_generation;
    return score_complete_p2_native_fragment_set(
        std::move(*bound.fragments),
        std::move(independently_attested_m08_m11),
        std::move(exact_phase6_m07_generation),true);
}
} // namespace rgsml::app::l3_automatch::m10_frozen
