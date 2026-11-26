#pragma once
// Cache addendum for actual native analysis inputs. No changes to the
// canonical M07-M11 serializer or artistic comparison weights.
#include "l3_product_native_one_pass.hpp"
#include "l3_m10_p2_exact_cache_evaluator.hpp"
#include <bit>

namespace rgsml::app::l3_automatch::m10_frozen {
[[nodiscard]] inline std::optional<ExactCacheBytes>
exact_native_analysis_scope_content(const ProductNativeOnePassSpec& spec)
{
    const auto& support=spec.source_support;
    if(!spec.current_full_phase6_source_and_grid||
       spec.exact_m20_fft_size<2U||spec.spectral_bands.empty()||
       support.selected_native_frame_sink||support.selected_stereo_frame_sink)
        return std::nullopt;
    ExactCacheBytes out;
    using namespace p2_cache_detail;
    const auto integer=[&](auto x){append_exact_field(out,std::to_string(x));};
    const auto real=[&](double x){integer(std::bit_cast<std::uint64_t>(x));};
    append_exact_field(out,"rgsml.l3.actual-native-analysis-scope/3");
    integer(spec.exact_m20_fft_size);
    integer(spec.derive_own_m19_temporal_gate);
    integer(spec.derive_native_common_profile_support);
    integer(spec.phase6_active_pcm_frames);
    integer(spec.exact_stereo_reference_grade);
    integer(spec.exact_m19_low_end_gate_same_as_m20_stft);
    for(bool zero:spec.exact_low_end_zero_side)integer(zero);
    for(const auto* grid:{&spec.spectral_bands,&spec.stereo_bands}) {
        integer(grid->size());
        for(const auto& band:*grid) {
            integer(band.exact_canonical_index);
            real(band.low_hz);real(band.high_hz);
        }
    }
    integer(support.has_spectral_gate);
    integer(support.spectral_frame_mask.size());
    for(auto v:support.spectral_frame_mask)out.push_back(v);
    integer(support.has_width_gate);
    integer(support.retained_width_blocks.size());
    for(const auto& block:support.retained_width_blocks) {
        integer(block.begin);integer(block.end);
    }
    integer(support.profile_common_bandwidth.has_value());
    if(support.profile_common_bandwidth) {
        real(support.profile_common_bandwidth->low_hz);
        real(support.profile_common_bandwidth->high_hz);
    }
    integer(support.authoritative_band_eligibility.size());
    for(auto v:support.authoritative_band_eligibility)out.push_back(v);
    return out;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
