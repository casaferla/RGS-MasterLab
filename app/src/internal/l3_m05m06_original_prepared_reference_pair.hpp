#pragma once
// L3 M05/M06 P/G once-per-generation native source pair.
// PREPARED is the Product-owned full PCM; REFERENCE is a DISTINCT
// independently decoded full PCM. The two ORIGINAL observation sets
// may be reused across every M10 candidate with the exact same
// source/reference dependency generation, never substituted by M21
// display metrics. Canonical cross-Fs grids/grades remain with M07.
#include "l3_m05m06_owned_full_source_native_facts.hpp"
#include <atomic>
#include <cstdint>
#include <utility>

namespace rgsml::app::l3_automatch {
struct OriginalPreparedReferenceNativeFacts final {
    OriginalOwnedNativeSourceFacts prepared;
    OriginalOwnedNativeSourceFacts reference;
    [[nodiscard]] constexpr bool authorizes_m10_or_m12()
        const noexcept{return false;}
};
[[nodiscard]] inline core::Result<OriginalPreparedReferenceNativeFacts>
measure_original_prepared_reference_native_once(
    const audio::AudioBuffer& exact_current_prepared,
    m05m06_real::SourceIdentity prepared_current_product_owner,
    ProductNativeOnePassSpec prepared_native_spec,
    const audio::AudioBuffer& exact_independent_reference,
    m05m06_real::SourceIdentity reference_current_product_owner,
    ProductNativeOnePassSpec reference_native_spec,
    m05m06_real::CommonTonalBandwidth exact_m07_common_bandwidth,
    double exact_phase6_tonal_density_floor,
    bool prepared_file_based_lra_policy_attested,
    bool reference_file_based_lra_policy_attested,
    bool current_product_source_reference_epoch_attested,
    const std::atomic<bool>* cancellation=nullptr)
{
    using R=core::Result<OriginalPreparedReferenceNativeFacts>;
    const auto fail=[](const char* why){
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    if(!current_product_source_reference_epoch_attested||
       !prepared_current_product_owner.full_track_owner_confirmed||
       !reference_current_product_owner.full_track_owner_confirmed||
       !prepared_current_product_owner.generation||
       !reference_current_product_owner.generation||
       prepared_current_product_owner.resource_identity.empty()||
       reference_current_product_owner.resource_identity.empty()||
       prepared_current_product_owner.resource_identity==
           reference_current_product_owner.resource_identity||
       &exact_current_prepared==&exact_independent_reference||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return fail("L3_ORIGINAL_PAIR_SAME_OR_UNATTESTED_SOURCE");
    auto prepared=observe_original_owned_full_source_once(
        exact_current_prepared,std::move(prepared_current_product_owner),
        std::move(prepared_native_spec),exact_m07_common_bandwidth,
        exact_phase6_tonal_density_floor,
        prepared_file_based_lra_policy_attested,cancellation);
    if(!prepared)return R::failure(*prepared.error());
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_ORIGINAL_PAIR_CANCELLED_BETWEEN_SOURCES");
    auto reference=observe_original_owned_full_source_once(
        exact_independent_reference,std::move(reference_current_product_owner),
        std::move(reference_native_spec),exact_m07_common_bandwidth,
        exact_phase6_tonal_density_floor,
        reference_file_based_lra_policy_attested,cancellation);
    if(!reference)return R::failure(*reference.error());
    // Readouts are intentionally two independent real producers.
    // Neither M05/M06 nor this pair can stamp missing Phase6 source
    // grade, immutable M07 digests, or an M10 J score.
    return R::success(OriginalPreparedReferenceNativeFacts{
        std::move(*prepared.value()),std::move(*reference.value())});
}
} // namespace rgsml::app::l3_automatch
