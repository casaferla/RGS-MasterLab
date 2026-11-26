#pragma once
#include "l3_native_seed_measurement_carriers.hpp"
// One real candidate render -> one M19 and one M20 pass. The optional
// native M20 spectral and coherent-stereo taps observe the SAME FFT
// frames; distinct frozen P04 20ms/5ms FFT reads SAME rendered PCM.
// No surrogate M21, extra render, M10/M13 or publication authority.
#include "l3_product_offline_trial_job.hpp"
#include <array>
#include <atomic>
#include <cmath>
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
    // Fixed-only retention; ordinary candidates do not retain the matrix.
    bool retain_native_spectral_energy{};
    // Phase6 comparison support: derive the common geometric mask for
    // this actual M19-gated full resource. M20 itself applies the
    // measured relative-power floor; missing bands remain missing.
    bool derive_native_common_profile_support{};
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
[[nodiscard]] inline bool bind_product_native_profile_common(
    ProductNativeOnePassSpec& spec,analysis::NativeTonalBandwidth common) noexcept {
    if(!std::isfinite(common.low_hz)||!std::isfinite(common.high_hz)||
       !(common.high_hz>common.low_hz))return false;
    const auto& supplied=spec.source_support.profile_common_bandwidth;
    if(supplied&&(supplied->low_hz!=common.low_hz||supplied->high_hz!=common.high_hz))
        return false;
    spec.source_support.profile_common_bandwidth=common;
    return true;
}
// Exact L/R equality of the actual full resource proves the global
// zero-Side endpoint for every retained band. Do not infer it from a
// tiny measured width, a dB floor, or a different source's policy.
[[nodiscard]] inline bool actual_full_pcm_has_exact_zero_side(const audio::AudioBufferView& view) {
    if(view.format().channel_count()!=2U||view.frame_count().value()<=0)return false;
    const auto l=view.channel(0),r=view.channel(1);
    if(!l||!r||l.value()->size()!=r.value()->size())return false;
    for(std::size_t i=0;i<l.value()->size();++i)
        if(!std::isfinite((*l.value())[i])||(*l.value())[i]!=(*r.value())[i])return false;
    return true;
}
struct ProductNativeOnePassResult final {
    OfflineProductCandidateMeasurement actual;
    m05m06_real::SpectralD10D12Native d10_d12;
    m05m06_real::StereoCoherentNativeResult s03_s06;
    m05m06_real::P04ProductObservation d07_d09;
    std::vector<std::uint64_t> selected_original_m20_ordinals;
    std::uint64_t exact_observed_active_frames{};
    std::optional<NativeSpectralSeedCarrier> spectral_seed_carrier{};
    std::optional<analysis::TruePeakMeasurement> exact_full_resource_m16{};
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
    std::optional<NativeSpectralSeedCarrier> spectral_seed_carrier{};
    std::optional<analysis::TruePeakMeasurement> exact_full_resource_m16{};
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
    auto combined=analysis::measure_basic_level_and_true_peak_full_resource(
        full_resource);
    if(!combined)return R::failure(*combined.error());
    auto& levels=combined.value()->levels;
    std::optional<analysis::NativeM19TonalGate> owned_gate;
    if(spec.derive_own_m19_temporal_gate) {
        if(spec.source_support.has_spectral_gate||spec.source_support.has_width_gate||
           !spec.source_support.spectral_frame_mask.empty()||
           !spec.source_support.retained_width_blocks.empty())
            return fail("L3_ONCE_CONFLICTING_TEMPORAL_GATE_AUTHORITIES");
        auto gate=analysis::bind_native_m19_tonal_gate(levels);
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
    // Lifetime is confined to the synchronous analyzer call. Never
    // store a span into the moved/copyable Product spec or a Qt object.
    std::vector<std::uint8_t> profile_support;
    if(spec.derive_native_common_profile_support) {
        const auto& common=spec.source_support.profile_common_bandwidth;
        if(!spec.derive_own_m19_temporal_gate||!common||
           !spec.source_support.authoritative_band_eligibility.empty())
            return fail("L3_ONCE_CONFLICTING_PROFILE_SUPPORT_AUTHORITY");
        const auto grid=analysis::native_tonal_fractional_grid(rate,12);
        if(grid.empty())return fail("L3_ONCE_PROFILE_GRID_UNAVAILABLE");
        profile_support.assign(grid.size(),0U);
        // Confirm the *exact* admitted common cells, not an interpolated
        // or caller-selected subset. Eligibility here is geometric; the
        // canonical M20 normalizer evaluates actual common-band power
        // and excludes fractions below 1e-12 without renormalization.
        std::size_t admitted=0;
        const bool has_retained_frames=owned_gate&&owned_gate->available&&
            std::any_of(owned_gate->selected_complete_frames.begin(),
                owned_gate->selected_complete_frames.end(),[](auto v){return v!=0U;});
        for(std::size_t i=0;i<grid.size();++i) {
            const auto& cell=grid[i];
            if(!cell.fully_supported||cell.low_hz<common->low_hz||cell.high_hz>common->high_hz)
                continue;
            if(admitted>=spec.spectral_bands.size())
                return fail("L3_ONCE_PROFILE_COMMON_GRID_MISMATCH");
            const auto& expected=spec.spectral_bands[admitted++];
            if(cell.band_index!=expected.exact_canonical_index||
               cell.low_hz!=expected.low_hz||cell.high_hz!=expected.high_hz)
                return fail("L3_ONCE_PROFILE_COMMON_GRID_MISMATCH");
            if(has_retained_frames)profile_support[i]=1U;
        }
        if(admitted!=spec.spectral_bands.size()||admitted==0U)
            return fail("L3_ONCE_PROFILE_COMMON_GRID_MISMATCH");
        spec.source_support.authoritative_band_eligibility=profile_support;
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
        const bool actual_zero_side=actual_full_pcm_has_exact_zero_side(view);
        // In the owner-derived path, a shared input spec must not
        // transplant an endpoint assertion from PREPARED to a trial.
        const auto low_zero=spec.derive_own_m19_temporal_gate?
            std::array<bool,3>{actual_zero_side,actual_zero_side,actual_zero_side}:
            spec.exact_low_end_zero_side;
        s03=stereo->finalize(
            spec.phase6_active_pcm_frames,
            spec.exact_stereo_reference_grade,
            {},low_zero,actual_zero_side);
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
    std::optional<NativeSpectralSeedCarrier> carrier;
    if(spec.retain_native_spectral_energy)
        carrier=NativeSpectralSeedCarrier{spectral.canonical_grid(),
            std::move(spectral).take_native_energies(),spectral.canonical_original_ordinals(),
            spec.phase6_active_pcm_frames,rate};
    return R::success(ProductNativeFullResourceObservations{
        std::move(levels),std::move(*tonal.value()),
        std::move(d10),std::move(s03),std::move(p04),
        spectral.canonical_original_ordinals(),spec.phase6_active_pcm_frames,std::move(carrier),
        std::move(combined.value()->true_peak)});
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
        observed.value()->exact_observed_active_frames,std::move(observed.value()->spectral_seed_carrier),
        std::move(observed.value()->exact_full_resource_m16)});
}
} // namespace rgsml::app::l3_automatch
