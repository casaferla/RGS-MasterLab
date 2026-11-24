#pragma once
// L3 M05/M06 Product Qt -> once-only full native PREPARED/REFERENCE.
// The sole Qt-thread operation was ProductQtP2NativeAdmission::capture;
// this worker reads only immutable PREPARED RenderResult, retained
// GoldCapture and independently decoded native REFERENCE WAV. Exact
// M19/M20/P04 original producers, not M21's display metric cells.
// Caller MUST use ProductQtP2NativeAdmission::still_current_on_owner
// AFTER worker return. This is not a persistent/verified M10 result.
#include "l3_m10_p2_product_qt_owner_capture.hpp"
#include "l3_m05m06_product_reference_full_native_worker.hpp"
#include "l3_m05m06_original_prepared_reference_pair.hpp"
#include <atomic>
#include <memory>
#include <utility>

namespace rgsml::app::l3_automatch {
[[nodiscard]] inline core::Result<OriginalPreparedReferenceNativeFacts>
measure_product_qt_original_pair_native_worker(
    const ProductQtP2NativeAdmission::Accepted& captured,
    ProductNativeOnePassSpec exact_prepared_native_spec,
    ProductNativeOnePassSpec exact_reference_native_spec,
    m05m06_real::CommonTonalBandwidth exact_m07_common_tonal,
    double exact_m05_phase6_density_floor,
    bool prepared_ebu_trailing_attested,
    bool reference_ebu_trailing_attested,
    const std::atomic<bool>* cancellation=nullptr)
{
    using R=core::Result<OriginalPreparedReferenceNativeFacts>;
    const auto fail=[](const char* reason){
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,reason});
    };
    const auto& stamp=captured.job.owner_stamp();
    if(!stamp.populated()||
       !captured.source.prepared||
       !captured.source.source_full_frames||
       captured.source.prepared->view().frame_count().value()!=
           captured.source.source_full_frames||
       captured.source.prepared->view().absolute_start_frame().value()!=0||
       captured.source.source.provider_id()!=stamp.source_provider_id||
       captured.source.source.locator()!=stamp.source_locator||
       captured.reference.gold.provider_id()!=stamp.reference_provider_id||
       captured.reference.gold.locator()!=stamp.reference_locator||
       captured.reference.owner.epoch!=stamp.m18_owner_epoch||
       captured.reference.owner.identity.source_generation!=
           stamp.source_generation||
       captured.reference.selection_revision!=
           stamp.reference_selection_revision||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return fail("L3_PRODUCT_NATIVE_PAIR_CAPTURE_OR_SCOPE_STALE");
    const auto common=m05m06_real::original_native_common_tonal_bandwidth(
        static_cast<std::uint32_t>(captured.source.prepared->view().format().sample_rate().value()),
        captured.reference.sample_rate_hz);
    if(!common||!m05m06_real::native_common_contains(*common,exact_m07_common_tonal))
        return fail("L3_PRODUCT_NATIVE_PAIR_COMMON_BANDWIDTH_INVALID");
    auto prepared=observe_original_owned_full_source_once(
        captured.source.prepared->buffer(),
        native_product_original_identity(
            stamp,OriginalProductRole::prepared),
        std::move(exact_prepared_native_spec),
        exact_m07_common_tonal,exact_m05_phase6_density_floor,
        prepared_ebu_trailing_attested,cancellation);
    if(!prepared)return R::failure(*prepared.error());
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_PRODUCT_NATIVE_PAIR_CANCELLED_AFTER_PREPARED");
    auto gold=measure_current_gold_full_native_worker(
        captured.reference,stamp,std::move(exact_reference_native_spec),
        exact_m07_common_tonal,exact_m05_phase6_density_floor,
        reference_ebu_trailing_attested,cancellation);
    if(!gold)return R::failure(*gold.error());
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_PRODUCT_NATIVE_PAIR_CANCELLED_AFTER_REFERENCE");
    // Gold's large decoded PCM is allowed to release here: all
    // original native feature families retain their exact observations.
    // No second full PCM copy or duplicate full M20 Fourier pass.
    OriginalPreparedReferenceNativeFacts pair{
        std::move(*prepared.value()),
        std::move(gold.value()->original_full_m05_m06)};
    if(!original_pair_matches_product_owner(pair,stamp))
        return fail("L3_PRODUCT_NATIVE_PAIR_ORIGINAL_GENERATION_MISMATCH");
    return R::success(std::move(pair));
}
} // namespace rgsml::app::l3_automatch
