#pragma once
// L3 M05/M06 ORIGINAL PREPARED or independent REFERENCE:
/// full, physically owned PCM -> original native M19/M20/P04 + M05/M06
// facts. Run ONCE per authenticated immutable Product source/generation
// and reuse for all M10 candidates. No candidate/REFERENCE alias, no
// virtual frames, no M21 display metrics, no fabricated Phase6 grades.
// Not itself an M07 admission or Product publication authorization.
#include "l3_product_native_one_pass.hpp"
#include "l3_m05_real_tonal_statistics.hpp"
#include "l3_m06_real_lra_native.hpp"
#include "l3_m06_real_windowed_rms_crest.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <utility>

namespace rgsml::app::l3_automatch {
struct OriginalOwnedNativeSourceFacts final {
    m05m06_real::SourceReadings original_m19_m20;
    ProductNativeFullResourceObservations full_native;
    m05m06_real::TonalObservationBundle original_t02_t06;
    m05m06_real::LraNativeObservation original_d02_lra;
    // Retain the COMPLETE 400ms/100ms native window population for
    // the original D03/D04 paired support. Scalar selectors alone are
    // insufficient and must never replace a full M07 source series.
    m05m06_real::D03D04NativeSeries original_d03_d04_window_series;
    m05m06_real::D03D04Selectors original_d03_d04;
    std::int64_t exactly_attested_phase6_active_frames{};
    m05m06_real::CommonTonalBandwidth exact_profile_common_bandwidth{};
    bool native_temporal_gate_derived_from_owned_pcm{};
    std::vector<m05m06_real::NativeFrameBand> exact_spectral_grid,exact_stereo_grid;
    double exact_density_floor{};
    [[nodiscard]] constexpr bool authorizes_m10_or_m13()
        const noexcept{return false;}
};
// The Product owner must attest both the immutable full PCM and this
// SourceIdentity. The worker never manufactures a source generation.
// M16 for protected True Peak is separately measured: M19's
// SUPPORTED_UNQUALIFIED reading cannot silently become QUALIFIED.
[[nodiscard]] inline core::Result<OriginalOwnedNativeSourceFacts>
observe_original_owned_full_source_once(
    const audio::AudioBuffer& exact_full_pcm,
    m05m06_real::SourceIdentity original_product_owner,
    ProductNativeOnePassSpec canonical_native_spec,
    m05m06_real::CommonTonalBandwidth exact_common_tonal_bandwidth,
    double exact_m05_phase6_density_floor,
    bool original_file_based_lra_trailing_policy_attested,
    const std::atomic<bool>* cancellation=nullptr)
{
    using R=core::Result<OriginalOwnedNativeSourceFacts>;
    const auto fail=[](const char* why) {
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    if(!original_product_owner.full_track_owner_confirmed||
       !original_product_owner.generation||
       original_product_owner.resource_identity.empty()||
       !canonical_native_spec.current_full_phase6_source_and_grid||
       canonical_native_spec.phase6_active_pcm_frames>
           static_cast<std::uint64_t>(
               exact_full_pcm.view().frame_count().value())||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return fail("L3_ORIGINAL_NATIVE_SOURCE_OWNER_OR_GRID_STALE");
    const auto native_bandwidth=analysis::native_tonal_measurement_bandwidth(
        static_cast<std::uint32_t>(exact_full_pcm.view().format().sample_rate().value()));
    if(!native_bandwidth||!std::isfinite(exact_common_tonal_bandwidth.low_hz)||
       !std::isfinite(exact_common_tonal_bandwidth.high_hz)||
       !(exact_common_tonal_bandwidth.high_hz>exact_common_tonal_bandwidth.low_hz)||
       exact_common_tonal_bandwidth.low_hz<native_bandwidth->low_hz||
       exact_common_tonal_bandwidth.high_hz>native_bandwidth->high_hz||
       !std::isfinite(exact_m05_phase6_density_floor)||
       !(exact_m05_phase6_density_floor>0.0))
        return fail("L3_ORIGINAL_NATIVE_SOURCE_BANDWIDTH_OR_FLOOR_INVALID");
    if(!bind_product_native_profile_common(canonical_native_spec,
           {exact_common_tonal_bandwidth.low_hz,exact_common_tonal_bandwidth.high_hz}))
        return fail("L3_ORIGINAL_NATIVE_PROFILE_COMMON_BANDWIDTH_CONFLICT");
    const bool own_gate=canonical_native_spec.derive_own_m19_temporal_gate;
    auto spectral_grid=canonical_native_spec.spectral_bands;
    auto stereo_grid=canonical_native_spec.stereo_bands;
    auto native=measure_real_full_owned_pcm_native_once(
        exact_full_pcm,std::move(canonical_native_spec),cancellation);
    if(!native)return R::failure(*native.error());
    const auto active=static_cast<std::int64_t>(native.value()->exact_observed_active_frames);
    auto imported=m05m06_real::import_actual_full_source(
        native.value()->levels,native.value()->tonal,
        std::move(original_product_owner),
        native.value()->selected_original_m20_ordinals);
    if(!imported.source)
        return fail("L3_ORIGINAL_NATIVE_M19_M20_PROVENANCE_INVALID");
    auto tonal=m05m06_real::observe_m05_tonal_bundle(
        native.value()->tonal,exact_common_tonal_bandwidth,
        exact_m05_phase6_density_floor);
    auto lra=m05m06_real::observe_lra_from_real_m19(
        *imported.source,active,
        original_file_based_lra_trailing_policy_attested);
    auto native_windows=m05m06_real::observe_d03_d04_real_pcm(
        exact_full_pcm.view());
    if(native_windows.problem!=m05m06_real::NativeWindowProblem::none)
        return fail("L3_ORIGINAL_NATIVE_D03_D04_SOURCE_INVALID");
    auto rms=m05m06_real::extract_d03_d04_type7(native_windows);
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_ORIGINAL_NATIVE_SOURCE_CANCELLED_AFTER_ANALYSIS");
    return R::success(OriginalOwnedNativeSourceFacts{
        std::move(*imported.source),std::move(*native.value()),
        std::move(tonal),std::move(lra),std::move(native_windows),
        std::move(rms),active,exact_common_tonal_bandwidth,own_gate,std::move(spectral_grid),
        std::move(stereo_grid),exact_m05_phase6_density_floor});
}
} // namespace rgsml::app::l3_automatch
