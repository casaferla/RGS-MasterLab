#pragma once
// L3 M05/M06 P/G once-per-generation native source pair.
// PREPARED is the Product-owned full PCM; REFERENCE is a DISTINCT
// independently decoded full PCM. The two ORIGINAL observation sets
// may be reused across every M10 candidate with the exact same
// source/reference dependency generation, never substituted by M21
// display metrics. Canonical cross-Fs grids/grades remain with M07.
#include "l3_m05m06_owned_full_source_native_facts.hpp"
#include "l3_m05m06_product_native_identity.hpp"
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
// Candidate, Fixed and fresh verifier all consume the actual P/G scope.
[[nodiscard]] inline bool bind_original_pair_profile_common(
    ProductNativeOnePassSpec& spec,const OriginalPreparedReferenceNativeFacts& pair) noexcept {
    const auto p=pair.prepared.exact_profile_common_bandwidth;
    const auto g=pair.reference.exact_profile_common_bandwidth;
    if(p.low_hz!=g.low_hz||p.high_hz!=g.high_hz)return false;
    return bind_product_native_profile_common(spec,{p.low_hz,p.high_hz});
}
// Generation alignment is necessary on EVERY new solver attempt AND
// exact completed-cache hit. Source and REFERENCE selection revisions
// are independent: a new REFERENCE with the same path is still new.
[[nodiscard]] inline bool original_pair_matches_product_owner(
    const OriginalPreparedReferenceNativeFacts& observed,
    const OfflineProductOwnerStamp& stamp)
{
    const auto& p=observed.prepared.original_m19_m20.owner;
    const auto& g=observed.reference.original_m19_m20.owner;
    return authentic_product_source_identity(
               p,stamp,OriginalProductRole::prepared)&&
           authentic_product_source_identity(
               g,stamp,OriginalProductRole::reference)&&
           p.resource_identity!=g.resource_identity;
}
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
    const auto common=m05m06_real::original_native_common_tonal_bandwidth(
        static_cast<std::uint32_t>(exact_current_prepared.view().format().sample_rate().value()),
        static_cast<std::uint32_t>(exact_independent_reference.view().format().sample_rate().value()));
    if(!common||!m05m06_real::native_common_contains(*common,exact_m07_common_bandwidth))
        return fail("L3_ORIGINAL_PAIR_COMMON_NATIVE_BANDWIDTH_INVALID");
    if(!bind_product_native_fft_metadata(prepared_native_spec,
           static_cast<std::uint32_t>(exact_current_prepared.view().format().sample_rate().value()))||
       !bind_product_native_fft_metadata(reference_native_spec,
           static_cast<std::uint32_t>(exact_independent_reference.view().format().sample_rate().value())))
        return fail("L3_ORIGINAL_PAIR_FFT_METADATA_INVALID");
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
