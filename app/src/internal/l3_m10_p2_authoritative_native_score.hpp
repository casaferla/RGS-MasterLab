#pragma once
// L3 M10 real P2 candidate -> exact M07 27-slot generation -> M08
// desired target -> frozen M11 protected/full safety -> measured M10 J.
// The native Product candidate is rendered and analyzed exactly once;
// all original source-specific M05/M06 bindings must be delivered by
// the owner-authenticated native triple resolver. Missing fragments
// never acquire an M10 score, even if M21 shows nine plausible values.
// This does NOT publish a Product graph, alter M17 or verify M13.
#include "l3_m10_p2_full_native_evidence.hpp"
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
        return {P2NativeCostReason::stale_or_unattested_product};
    m07_frozen::ExactTwentySevenFeatureAssembler generation{
        std::move(exact_phase6_m07_generation),true};
    const auto admitted=m07_frozen::collect_existing_m05m06_native_fragments(
        generation,actual_triple_native);
    if(admitted.status!=
       m07_frozen::NativeFragmentAdmission::accepted_complete)
        return {P2NativeCostReason::incomplete_or_conflicting_m07,
                admitted.inventory};
    auto exact=generation.complete_canonical_population();
    if(!exact)
        return {P2NativeCostReason::incomplete_or_conflicting_m07,
                admitted.inventory};
    // The Product callback must never be allowed to replace the
    // authenticated native M07 slots with unrelated numerical scores.
    independently_attested_m08_m11.exact_pair_slots=std::move(*exact);
    const auto cost=score_exact_candidate_population(
        independently_attested_m08_m11);
    if(cost.reason!=AuthoritativeCostReason::measured||
       !cost.measured||!valid_key(*cost.measured))
        return {P2NativeCostReason::m08_or_m11_safety_rejected,
                admitted.inventory};
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
    ResolveAllRealNativeTripleFragments&& resolve_original_m05m06,
    const std::atomic<bool>* cancellation=nullptr)
{
    if(!current_product_source_reference_and_candidate_attested||
       exact_phase6_m07_generation.empty()||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return {P2NativeCostReason::stale_or_unattested_product};
    auto measured=measure_m10_p2_full_native_once(
        candidate,std::move(native_spec),exact_pre_p2_prefix,cancellation);
    if(!measured)
        return {P2NativeCostReason::real_render_or_analysis_failed};
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return {P2NativeCostReason::stale_or_unattested_product};
    // Resolver cannot invent the actual P2 PCM: it receives the
    // original immutable render+M16+M19/M20/native events of this job.
    auto real_triple=std::invoke(
        std::forward<ResolveAllRealNativeTripleFragments>(
            resolve_original_m05m06),*measured.value());
    if(!real_triple)
        return {P2NativeCostReason::missing_triple_native_authority};
    return score_complete_p2_native_fragment_set(
        std::move(*real_triple),
        std::move(independently_attested_m08_m11),
        std::move(exact_phase6_m07_generation),true);
}
} // namespace rgsml::app::l3_automatch::m10_frozen
