#pragma once
// L3 Product REAL M19/M20 plus same RenderPreview PCM one-pass bundle.
// Reuses already measured owning levels/tonal; D03/D04 reads native
// PCM blocks once. No new FFT or independent M21 target surrogate.
#include "l3_m05m06_real_analysis_port.hpp"
#include "l3_m05_real_tonal_statistics.hpp"
#include "l3_m06_real_windowed_rms_crest.hpp"
#include "l3_m06_real_lra_native.hpp"
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <cstdint>
#include <optional>
#include <utility>

namespace rgsml::app::l3_automatch::m05m06_real {
struct ProductNativeFeatureBundle final {
    SourceReadings m19_m20_current_readings;
    TonalObservationBundle tonal_t02_t06;
    D03D04NativeSeries native_d03_d04;
    LraNativeObservation native_d02_lra;
    [[nodiscard]] constexpr bool authorizes_m10_or_export()
        const noexcept {return false;}
};
[[nodiscard]] inline std::optional<ProductNativeFeatureBundle>
bundle_actual_product_m05m06(
    const audio::AudioBufferView& physically_rendered_pcm,
    const analysis::BasicLevelAnalysisResult& real_m19,
    const analysis::TonalStereoAnalysisResult& real_m20,
    SourceIdentity product_owner,
    CommonTonalBandwidth exact_common_bandwidth,
    double exact_accepted_tonal_density_floor,
    std::int64_t phase6_exact_active_frames,
    bool phase6_ebu_file_based_trailing_attested)
{
    if(physically_rendered_pcm.absolute_start_frame().value()!=0||
       physically_rendered_pcm.frame_count().value()<=0||
       physically_rendered_pcm.frame_count().value()!=
           real_m19.scope_frame_count||
       physically_rendered_pcm.format().channel_count()!=
           real_m19.channel_count||
       physically_rendered_pcm.format().sample_rate().value()!=
           real_m19.sample_rate_hz||
       physically_rendered_pcm.timebase().frame_domain_id()!=
           real_m19.frame_domain)
        return std::nullopt;
    auto observed=import_actual_full_source(
        real_m19,real_m20,std::move(product_owner));
    if(!observed.source)return std::nullopt;
    auto native=observe_d03_d04_real_pcm(physically_rendered_pcm);
    if(native.problem!=NativeWindowProblem::none)
        return std::nullopt;
    const auto tonal=observe_m05_tonal_bundle(
        real_m20,exact_common_bandwidth,
        exact_accepted_tonal_density_floor);
    const auto lra=observe_lra_from_real_m19(
        *observed.source,phase6_exact_active_frames,
        phase6_ebu_file_based_trailing_attested);
    return ProductNativeFeatureBundle{
        std::move(*observed.source),tonal,std::move(native),lra};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
