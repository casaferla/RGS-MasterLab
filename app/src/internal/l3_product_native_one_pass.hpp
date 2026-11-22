#pragma once
// One real candidate render -> one M19 and one M20 pass. The optional
// native M20 spectral and coherent-stereo taps observe the SAME FFT
// frames; distinct frozen P04 20ms/5ms FFT reads SAME rendered PCM.
// No surrogate M21, extra render, M10/M13 or publication authority.
#include "l3_product_offline_trial_job.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
struct ProductNativeOnePassSpec final {
    std::vector<m05m06_real::NativeFrameBand> spectral_bands;
    std::vector<m05m06_real::NativeFrameBand> stereo_bands;
    std::uint64_t exact_m20_fft_size{};
    std::uint64_t phase6_active_pcm_frames{};
    bool current_full_phase6_source_and_grid{};
    bool exact_stereo_reference_grade{};
    bool exact_m19_low_end_gate_same_as_m20_stft{};
    std::array<bool,3> exact_low_end_zero_side{};
    analysis::TonalStereoTrustedSupport source_support{};
};
struct ProductNativeOnePassResult final {
    OfflineProductCandidateMeasurement actual;
    m05m06_real::SpectralD10D12Native d10_d12;
    m05m06_real::StereoCoherentNativeResult s03_s06;
    m05m06_real::P04ProductObservation d07_d09;
    std::vector<std::uint64_t> selected_original_m20_ordinals;
    [[nodiscard]] constexpr bool authorizes_m10_or_export()
        const noexcept{return false;}
};
[[nodiscard]] inline core::Result<ProductNativeOnePassResult>
measure_real_product_candidate_once(
    const OfflineProductTrialJob& job,ProductNativeOnePassSpec spec,
    const std::atomic<bool>* cancellation=nullptr) {
    using R=core::Result<ProductNativeOnePassResult>;
    const auto fail=[](const char* text) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,text});
    };
    if(!spec.current_full_phase6_source_and_grid||
       spec.exact_m20_fft_size<2U||spec.spectral_bands.empty()||
       spec.source_support.selected_native_frame_sink||
       spec.source_support.selected_stereo_frame_sink)
        return fail("L3_ONCE_MISSING_SOURCE_OR_GRID");
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_ONCE_CANCELLED_BEFORE_RENDER");
    auto rendered=job.execute();
    if(!rendered)return R::failure(*rendered.error());
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_ONCE_CANCELLED_AFTER_RENDER");
    if(!job.result_has_expected_scope(*rendered.value()))
        return fail("L3_ONCE_RENDER_SCOPE_STALE");
    const auto view=rendered.value()->view();
    const auto rate=static_cast<std::uint32_t>(
        view.format().sample_rate().value());
    const auto channels=view.format().channel_count();
    if(spec.phase6_active_pcm_frames>
       static_cast<std::uint64_t>(view.frame_count().value()))
        return fail("L3_ONCE_ACTIVE_SCOPE_NOT_CURRENT");
    auto levels=analysis::measure_basic_level_full_resource(
        rendered.value()->buffer());
    if(!levels)return R::failure(*levels.error());
    m05m06_real::AuthenticM20FrameEnergySink spectral{
        rate,spec.exact_m20_fft_size,
        std::move(spec.spectral_bands),true};
    if(!spectral.valid())return fail("L3_ONCE_SPECTRAL_GRID_INVALID");
    std::optional<m05m06_real::AuthenticM20CoherentStereoSink> stereo;
    if(channels==2U) {
        if(spec.stereo_bands.empty())
            return fail("L3_ONCE_STEREO_GRID_MISSING");
        stereo.emplace(rate,spec.exact_m20_fft_size,
            std::move(spec.stereo_bands),true,
            spec.exact_m19_low_end_gate_same_as_m20_stft);
        if(!stereo->valid())return fail("L3_ONCE_STEREO_GRID_INVALID");
        spec.source_support.selected_stereo_frame_sink=&*stereo;
    }else if(channels!=1U)
        return fail("L3_ONCE_INVALID_AUDIO_CHANNELS");
    spec.source_support.selected_native_frame_sink=&spectral;
    auto tonal=analysis::measure_tonal_stereo_scope(
        view,view.absolute_start_frame(),view.frame_count(),
        view.timebase().frame_domain_id(),spec.source_support);
    if(!tonal)return R::failure(*tonal.error());
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_ONCE_CANCELLED_AFTER_M20");
    if(!spectral.valid()||spectral.exact_e_tb().size()!=
       tonal.value()->selected_spectral_windows)
        return fail("L3_ONCE_NATIVE_STFT_COUNT_MISMATCH");
    auto d10=m05m06_real::reduce_native_spectral_d10_d12(
        spectral.exact_e_tb(),spectral.configured_bands(),true,
        spec.phase6_active_pcm_frames,rate);
    if(d10.source_reason==
           m05m06_real::SpectralNativeReason::invalid_arithmetic||
       d10.source_reason==
           m05m06_real::SpectralNativeReason::invalid_energy_or_geometry)
        return fail("L3_ONCE_NATIVE_SPECTRAL_INVALID");
    m05m06_real::StereoCoherentNativeResult s03;
    if(stereo) {
        s03=stereo->finalize(
            spec.phase6_active_pcm_frames,
            spec.exact_stereo_reference_grade,
            {},spec.exact_low_end_zero_side);
        if(!s03.numerical_source_valid||
           s03.native_selected_frames!=
             tonal.value()->selected_spectral_windows)
            return fail("L3_ONCE_NATIVE_STEREO_COUNT_MISMATCH");
    }
    auto p04=m05m06_real::measure_product_p04_d07_d09(
        view,static_cast<std::int64_t>(spec.phase6_active_pcm_frames),
        true,cancellation);
    if(p04.reason!=m05m06_real::P04ProductObservationReason::ready)
        return fail("L3_ONCE_NATIVE_P04_INVALID");
    return R::success(ProductNativeOnePassResult{
        {std::move(*rendered.value()),std::move(*levels.value()),
         std::move(*tonal.value())},
        std::move(d10),std::move(s03),std::move(p04),
        spectral.canonical_original_ordinals()});
}
} // namespace rgsml::app::l3_automatch
