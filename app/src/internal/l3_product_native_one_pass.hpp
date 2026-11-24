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
    // Opt-in Product path: derive temporal gates from this rendered PCM,
    // never reuse the PREPARED or another candidate's retained intervals.
    bool derive_own_m19_temporal_gate{};
};
// Resolve an omitted FFT size from the actual source rate, without
// granting any missing Phase6 grid/grade or changing caller definitions.
// A supplied conflicting size is an error, never silently overwritten.
[[nodiscard]] inline bool bind_product_native_fft_metadata(
    ProductNativeOnePassSpec& spec,std::uint32_t source_rate) noexcept {
    const auto geometry=analysis::native_tonal_frame_geometry(source_rate);
    if(!geometry||(spec.exact_m20_fft_size!=0U&&
                   spec.exact_m20_fft_size!=geometry->fft))return false;
    spec.exact_m20_fft_size=geometry->fft;
    return true;
}
struct ProductNativeOnePassResult final {
    OfflineProductCandidateMeasurement actual;
    m05m06_real::SpectralD10D12Native d10_d12;
    m05m06_real::StereoCoherentNativeResult s03_s06;
    m05m06_real::P04ProductObservation d07_d09;
    std::vector<std::uint64_t> selected_original_m20_ordinals;
    std::uint64_t exact_observed_active_frames{};
    [[nodiscard]] constexpr bool authorizes_m10_or_export()
        const noexcept{return false;}
};
// This is a FULL, real, owning PCM source analyzer. Unlike the candidate
// entry point below, it does not render a DSP graph and can be reused for
// the once-per-generation PREPARED and independent decoded REFERENCE.
// Reuse exact M19/M20 original producers and the exact M20 frame sinks;
// no M21 display proxy and no second/surrogate Phase6 FFT definitions.
struct ProductNativeFullResourceObservations final {
    analysis::BasicLevelAnalysisResult levels;
    analysis::TonalStereoAnalysisResult tonal;
    m05m06_real::SpectralD10D12Native d10_d12;
    m05m06_real::StereoCoherentNativeResult s03_s06;
    m05m06_real::P04ProductObservation d07_d09;
    std::vector<std::uint64_t> selected_original_m20_ordinals;
    std::uint64_t exact_observed_active_frames{};
    [[nodiscard]] constexpr bool authorizes_m10_or_m12()const noexcept {
        return false;
    }
};
[[nodiscard]] inline core::Result<ProductNativeFullResourceObservations>
measure_real_full_owned_pcm_native_once(
    const audio::AudioBuffer& full_resource,
    ProductNativeOnePassSpec spec,
    const std::atomic<bool>* cancellation=nullptr)
{
    using R=core::Result<ProductNativeFullResourceObservations>;
    const auto fail=[](const char* msg){
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,msg});
    };
    const auto geometry=full_resource.view();
    if(!spec.current_full_phase6_source_and_grid||
       spec.exact_m20_fft_size<2U||spec.spectral_bands.empty()||
       spec.source_support.selected_native_frame_sink||
       spec.source_support.selected_stereo_frame_sink||
       geometry.absolute_start_frame().value()!=0||
       geometry.frame_count().value()<=0||
       geometry.timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return fail("L3_ONCE_MISSING_SOURCE_OR_GRID");
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_ONCE_CANCELLED_BEFORE_NATIVE_ANALYSIS");
    const auto view=full_resource.view();
    const auto rate=static_cast<std::uint32_t>(
        view.format().sample_rate().value());
    const auto channels=view.format().channel_count();
    const auto native_geometry=analysis::native_tonal_frame_geometry(rate);
    if(!native_geometry||native_geometry->fft!=spec.exact_m20_fft_size)
        return fail("L3_ONCE_NATIVE_FFT_METADATA_MISMATCH");
    if(spec.phase6_active_pcm_frames>
       static_cast<std::uint64_t>(view.frame_count().value()))
        return fail("L3_ONCE_ACTIVE_SCOPE_NOT_CURRENT");
    auto levels=analysis::measure_basic_level_full_resource(
        full_resource);
    if(!levels)return R::failure(*levels.error());
    std::optional<analysis::NativeM19TonalGate> owned_gate;
    if(spec.derive_own_m19_temporal_gate) {
        if(spec.source_support.has_spectral_gate||spec.source_support.has_width_gate||
           !spec.source_support.spectral_frame_mask.empty()||
           !spec.source_support.retained_width_blocks.empty())
            return fail("L3_ONCE_CONFLICTING_TEMPORAL_GATE_AUTHORITIES");
        auto gate=analysis::bind_native_m19_tonal_gate(*levels.value());
        if(!gate)return R::failure(*gate.error());
        owned_gate=std::move(*gate.value());
        spec.phase6_active_pcm_frames=owned_gate->active_pcm_frames;
        spec.source_support.has_spectral_gate=owned_gate->available;
        spec.source_support.has_width_gate=owned_gate->available;
        spec.source_support.spectral_frame_mask=owned_gate->selected_complete_frames;
        spec.source_support.retained_width_blocks=owned_gate->retained_union;
        // Low-end consumes the exact same retained temporal selection.
        spec.exact_m19_low_end_gate_same_as_m20_stft=owned_gate->available;
    }
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
        spec.phase6_active_pcm_frames,rate,
        spectral.canonical_original_ordinals());
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
        if(!s03.source_observation_complete||
           s03.native_selected_frames!=
             tonal.value()->selected_spectral_windows)
            return fail("L3_ONCE_NATIVE_STEREO_COUNT_MISMATCH");
    }else {
        // Mono has no coherent stereo observations. Preserve configured
        // component identities without inventing a stereo/Side reading.
        s03.source_observation_complete=true;
        s03.source_channel_count=1U;
        for(const auto& b:spec.stereo_bands)
            s03.multiband.push_back({b.exact_canonical_index,{},{}});
    }
    auto p04=m05m06_real::measure_product_p04_d07_d09(
        view,static_cast<std::int64_t>(spec.phase6_active_pcm_frames),
        true,cancellation);
    if(p04.reason!=m05m06_real::P04ProductObservationReason::ready)
        return fail("L3_ONCE_NATIVE_P04_INVALID");
    return R::success(ProductNativeFullResourceObservations{
        std::move(*levels.value()),std::move(*tonal.value()),
        std::move(d10),std::move(s03),std::move(p04),
        spectral.canonical_original_ordinals(),spec.phase6_active_pcm_frames});
}
// Reusable native analyzer for any genuine immutable Product job that
// exposes execute()/result_has_expected_scope(). The specialized M10/P2
// three-segment job renders ONCE, then shares the same full PCM native
// analysis entry point as PREPARED and decoded REFERENCE.
template<typename ProductJob>
[[nodiscard]] inline core::Result<ProductNativeOnePassResult>
measure_real_product_candidate_once(
    const ProductJob& job,ProductNativeOnePassSpec spec,
    const std::atomic<bool>* cancellation=nullptr)
{
    using R=core::Result<ProductNativeOnePassResult>;
    const auto fail=[](const char* text){
        return R::failure(core::Error{core::ErrorCode::InvalidState,text});
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
    auto observed=measure_real_full_owned_pcm_native_once(
        rendered.value()->buffer(),std::move(spec),cancellation);
    if(!observed)return R::failure(*observed.error());
    return R::success(ProductNativeOnePassResult{
        {std::move(*rendered.value()),
         std::move(observed.value()->levels),
         std::move(observed.value()->tonal)},
        std::move(observed.value()->d10_d12),
        std::move(observed.value()->s03_s06),
        std::move(observed.value()->d07_d09),
        std::move(observed.value()->selected_original_m20_ordinals),
        observed.value()->exact_observed_active_frames});
}
} // namespace rgsml::app::l3_automatch
