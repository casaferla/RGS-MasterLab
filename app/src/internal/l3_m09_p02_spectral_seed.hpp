#pragma once
// L3-M09 P02 frozen D10/D11/D12 Spectral Dynamic-EQ analytical seeds.
// Consumes only authenticated M05/M06/M07/M08 fixed/target features.
// D10 drives seed; D11 is agreement gate; D12 compatibility only.
// Threshold E[t,b] is an independent exact native seed-only carrier.
// None of this asserts real detector response, effect or M13 safety.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
struct SpectralSeedCell final {
    bool d10_requested{};
    bool d10_exact_current{};
    bool d11_exact_current{};
    bool d12_exact_compatible{};
    double canonical_center_hz{};
    double canonical_low_edge_hz{};
    double canonical_high_edge_hz{};
    double d10_optimization_minus_fixed{};
    double d10_deadband{};
    double d10_pair_support{};
    double d11_optimization_minus_fixed{};
    double d11_deadband{};
};
enum class SpectralSeedReason:std::uint8_t {
    ready,limited_to_seed_range,unbound_canonical_grid_or_fixed,
    fewer_than_three_eligible_cells,invalid_feature_or_grid,
    incoherent_d10_direction,conflicting_d11_direction,
    missing_d12_compatibility
};
struct SpectralClusterSeed final {
    std::optional<double> weighted_center_hz;
    std::optional<double> raw_spread_delta;
    std::optional<double> limited_dynamic_range_db;
    std::optional<double> selection_score;
    // Only caller-attested canonical cell edges; later actual DSP
    // filter-family legality, Q, shelf default and topology are external.
    std::optional<double> cluster_lower_edge_hz;
    std::optional<double> cluster_upper_edge_hz;
    bool lower_bound_priority{};
    SpectralSeedReason reason{SpectralSeedReason::unbound_canonical_grid_or_fixed};
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept {return false;}
};
[[nodiscard]] inline SpectralClusterSeed derive_d10_spectral_cluster_p02(
    std::span<const SpectralSeedCell> cells,
    bool exact_grid_cluster_fixed_and_m11,
    bool touches_both_canonical_comparison_boundaries)
{
    if(!exact_grid_cluster_fixed_and_m11)
        return {{},{},{},{},{},{},false,
                SpectralSeedReason::unbound_canonical_grid_or_fixed};
    if(cells.size()<3U)
        return {{},{},{},{},{},{},false,
                SpectralSeedReason::fewer_than_three_eligible_cells};
    double score=0.0;
    double weighted_log_frequency=0.0;
    double weighted_delta=0.0;
    int sign=0;
    double prior_high=0.0;
    for(std::size_t i=0;i<cells.size();++i) {
        const auto& c=cells[i];
        if(!c.d10_requested||!c.d10_exact_current||
           !c.d11_exact_current)
            return {{},{},{},{},{},{},false,
                    SpectralSeedReason::fewer_than_three_eligible_cells};
        if(!c.d12_exact_compatible)
            return {{},{},{},{},{},{},false,
                    SpectralSeedReason::missing_d12_compatibility};
        if(!std::isfinite(c.canonical_center_hz)||
           !std::isfinite(c.canonical_low_edge_hz)||
           !std::isfinite(c.canonical_high_edge_hz)||
           c.canonical_low_edge_hz<=0.0||
           c.canonical_low_edge_hz>=c.canonical_center_hz||
           c.canonical_center_hz>=c.canonical_high_edge_hz||
           !std::isfinite(c.d10_optimization_minus_fixed)||
           !std::isfinite(c.d11_optimization_minus_fixed)||
           !std::isfinite(c.d10_deadband)||c.d10_deadband<0.0||
           !std::isfinite(c.d11_deadband)||c.d11_deadband<0.0||
           !std::isfinite(c.d10_pair_support)||
           c.d10_pair_support<=0.0||c.d10_pair_support>1.0)
            return {{},{},{},{},{},{},false,
                    SpectralSeedReason::invalid_feature_or_grid};
        // Exact canonical contiguous edge equality: never resample or
        // bridge missing bands. Grid creator attests identical endpoint
        // provenance and frequency units, not a newly sampled grid.
        if(i>0U&&c.canonical_low_edge_hz!=prior_high)
            return {{},{},{},{},{},{},false,
                    SpectralSeedReason::unbound_canonical_grid_or_fixed};
        prior_high=c.canonical_high_edge_hz;
        const double d10=c.d10_optimization_minus_fixed;
        if(std::abs(d10)<=c.d10_deadband)
            return {{},{},{},{},{},{},false,
                    SpectralSeedReason::fewer_than_three_eligible_cells};
        const int current_sign=d10>0.0?1:-1;
        if(sign!=0&&sign!=current_sign)
            return {{},{},{},{},{},{},false,
                    SpectralSeedReason::incoherent_d10_direction};
        sign=current_sign;
        const double d11=c.d11_optimization_minus_fixed;
        if(std::abs(d11)>c.d11_deadband&&
           (d11>0.0?1:-1)!=current_sign)
            return {{},{},{},{},{},{},false,
                    SpectralSeedReason::conflicting_d11_direction};
        const double w=std::abs(d10)*c.d10_pair_support;
        score+=w;
        weighted_log_frequency+=w*std::log(c.canonical_center_hz);
        weighted_delta+=w*d10;
        if(!std::isfinite(score)||!std::isfinite(weighted_delta)||
           !std::isfinite(weighted_log_frequency))
            return {{},{},{},{},{},{},false,
                    SpectralSeedReason::invalid_feature_or_grid};
    }
    if(score<=0.0)
        return {{},{},{},{},{},{},false,
                SpectralSeedReason::invalid_feature_or_grid};
    const double center=std::exp(weighted_log_frequency/score);
    const double raw=weighted_delta/score;
    if(!std::isfinite(center)||center<=0.0||!std::isfinite(raw))
        return {{},{},{},{},{},{},false,
                SpectralSeedReason::invalid_feature_or_grid};
    const double limited=raw>2.0?2.0:raw< -2.0?-2.0:raw;
    return {center,raw,limited,score,
            cells.front().canonical_low_edge_hz,
            cells.back().canonical_high_edge_hz,
            touches_both_canonical_comparison_boundaries,
            raw==limited?SpectralSeedReason::ready:
                         SpectralSeedReason::limited_to_seed_range};
}
enum class NativeClusterSeriesReason:std::uint8_t {
    ready,missing_exact_native_carrier,invalid_member_population,
    invalid_native_energy,no_common_positive_energy_frames
};
struct NativeClusterThreshold final {
    std::optional<double> p75_dbfs;
    std::size_t common_valid_frames{};
    std::size_t excluded_missing_frames{};
    NativeClusterSeriesReason reason{
        NativeClusterSeriesReason::missing_exact_native_carrier};
    [[nodiscard]] constexpr bool authorizes_dsp_detector_equivalence()
        const noexcept {return false;}
};
// Per-frame native energies are already canonical and comparable.
// Each row corresponds to ONE exact STFT frame, each column to ONE
// exact cluster member in canonical order. All members must be valid
// for a frame to participate. Missing rows are skipped, not imputed.
// This does NOT create a new artistic feature or assume dynamic EQ's
// true filter/detector responds as an ideal energy cluster.
[[nodiscard]] inline NativeClusterThreshold derive_cluster_type7_p75(
    const std::vector<std::vector<std::optional<double>>>& native_e_tb,
    std::size_t canonical_cluster_members,
    bool exact_common_stft_grid_and_level_convention)
{
    if(!exact_common_stft_grid_and_level_convention||
       native_e_tb.empty())
        return {{},0,0,
                NativeClusterSeriesReason::missing_exact_native_carrier};
    if(canonical_cluster_members<3U)
        return {{},0,0,
                NativeClusterSeriesReason::invalid_member_population};
    std::vector<double> common_dbfs;
    common_dbfs.reserve(native_e_tb.size());
    std::size_t excluded=0U;
    for(const auto& frame:native_e_tb) {
        if(frame.size()!=canonical_cluster_members)
            return {{},0,excluded,
                    NativeClusterSeriesReason::invalid_member_population};
        double sum=0.0;
        bool has_missing=false;
        for(const auto& item:frame) {
            if(!item) {
                has_missing=true;
                continue;
            }
            if(!std::isfinite(*item)||*item<0.0)
                return {{},0,excluded,
                        NativeClusterSeriesReason::invalid_native_energy};
            sum+=*item;
        }
        if(has_missing) {
            ++excluded;
            continue;
        }
        if(!std::isfinite(sum)||sum<=0.0)
            return {{},0,excluded,
                    NativeClusterSeriesReason::invalid_native_energy};
        const double level=10.0*std::log10(sum);
        if(!std::isfinite(level))
            return {{},0,excluded,
                    NativeClusterSeriesReason::invalid_native_energy};
        common_dbfs.push_back(level);
    }
    if(common_dbfs.empty())
        return {{},0,excluded,
                NativeClusterSeriesReason::no_common_positive_energy_frames};
    std::sort(common_dbfs.begin(),common_dbfs.end());
    const double rank=0.75*static_cast<double>(common_dbfs.size()-1U);
    const auto lo=static_cast<std::size_t>(std::floor(rank));
    const auto hi=static_cast<std::size_t>(std::ceil(rank));
    const double fraction=rank-static_cast<double>(lo);
    const double threshold=common_dbfs[lo]*(1.0-fraction)
                          +common_dbfs[hi]*fraction;
    if(!std::isfinite(threshold))
        return {{},0,excluded,
                NativeClusterSeriesReason::invalid_native_energy};
    return {threshold,common_dbfs.size(),excluded,
            NativeClusterSeriesReason::ready};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
