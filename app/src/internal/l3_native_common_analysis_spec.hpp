#pragma once
#include "l3_product_native_one_pass.hpp"
#include "l3_m05_real_tonal_statistics.hpp"
#include <algorithm>
#include <optional>

namespace rgsml::app::l3_automatch {
struct NativeCommonAnalysisSpec final {
    ProductNativeOnePassSpec prepared;
    ProductNativeOnePassSpec reference;
    m05m06_real::CommonTonalBandwidth common;
};
// Phase6 grid: exact intersection of fully supported canonical cells.
// Different input rates retain their respective FFTs; no SRC or
// frequency-center interpolation is introduced by this builder.
[[nodiscard]] inline std::optional<NativeCommonAnalysisSpec>
build_current_native_common_analysis_spec(std::uint32_t prepared_rate,
    std::uint32_t reference_rate,bool exact_current_phase6_policy)
{
    if(!exact_current_phase6_policy)return {};
    const auto p=analysis::native_tonal_frame_geometry(prepared_rate);
    const auto g=analysis::native_tonal_frame_geometry(reference_rate);
    if(!p||!g)return {};
    const auto bandwidth=m05m06_real::original_native_common_tonal_bandwidth(prepared_rate,reference_rate);
    if(!bandwidth)return {};
    const auto [low,high]=*bandwidth;
    const auto prepared_grid=analysis::native_tonal_fractional_grid(prepared_rate,12);
    const auto reference_grid=analysis::native_tonal_fractional_grid(reference_rate,12);
    std::vector<m05m06_real::NativeFrameBand> common;
    for(const auto& cell:prepared_grid) {
        if(!cell.fully_supported||cell.low_hz<low||cell.high_hz>high)continue;
        const auto other=std::find_if(reference_grid.begin(),reference_grid.end(),
            [&](const auto& b){return b.band_index==cell.band_index;});
        if(other==reference_grid.end()||!other->fully_supported||
           other->low_hz!=cell.low_hz||other->center_hz!=cell.center_hz||
           other->high_hz!=cell.high_hz)return {};
        common.push_back({cell.band_index,cell.low_hz,cell.high_hz});
    }
    if(common.empty())return {};
    NativeCommonAnalysisSpec out;
    out.common=*bandwidth;
    for(auto* spec:{&out.prepared,&out.reference}) {
        spec->current_full_phase6_source_and_grid=true;
        spec->derive_own_m19_temporal_gate=true;
        spec->derive_native_common_profile_support=true;
        spec->exact_stereo_reference_grade=true; // native duration and support still independently checked
        spec->spectral_bands=common;
        spec->stereo_bands=common;
        if(!bind_product_native_profile_common(*spec,{low,high}))return {};
    }
    out.prepared.exact_m20_fft_size=p->fft;
    out.reference.exact_m20_fft_size=g->fft;
    return out;
}
} // namespace rgsml::app::l3_automatch
