#pragma once
// L3-M09 P02 ACCEPTED_FROZEN: D01-D06 support-aware Macro Compressor
// seed reduction, strictly from caller-authenticated Fixed/M08 operands.
// No Phase-6 feature calculation, mutable product chain, solver score,
// AutoMatch Amount-to-knob mapping, real GR safety or plan publication.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
struct MacroComponent final {
    bool requested{};
    bool protected_from_action{};
    bool exact_current{};
    double z_fixed{};
    double z_optimization{};
    double comparison_scale{};
    double frozen_deadband{};
    double configured_mass{};
    double paired_support{};
};
struct MacroGroup final {
    // Canonical order D01..D06: D01 centered P10/P90/P95 and
    // D04 local crest P10/P50/P90 are both single three-component groups.
    std::vector<MacroComponent> components;
};
enum class MacroSeedReason:std::uint8_t {
    ready,missing_exact_evidence,invalid_frozen_component,
    contradictory_expansion,insufficient_coherent_distinct_groups
};
struct MacroReductionSeed final {
    std::optional<double> required_movement;
    MacroSeedReason reason{MacroSeedReason::missing_exact_evidence};
    std::uint32_t coherent_distinct_groups{};
    [[nodiscard]] constexpr bool authorizes_compression_or_publication()
        const noexcept {return false;}
};
[[nodiscard]] inline MacroReductionSeed reduce_macro_p02(
    const std::array<MacroGroup,6>& source,
    bool exact_fixed_chain_snapshot_and_m11_protection)
{
    if(!exact_fixed_chain_snapshot_and_m11_protection)
        return {{},MacroSeedReason::missing_exact_evidence,0};
    double weighted_sum=0.0;
    double active_mass=0.0;
    std::uint32_t group_count=0;
    bool any_expansion=false;
    for(std::size_t g=0;g<source.size();++g) {
        const auto& components=source[g].components;
        if(components.size()!=((g==0U||g==3U)?3U:1U))
            return {{},MacroSeedReason::invalid_frozen_component,0};
        bool group_reduction=false;
        bool group_expansion=false;
        double group_weighted_sum=0.0;
        double group_active_mass=0.0;
        for(std::size_t j=0;j<components.size();++j) {
            const auto& item=components[j];
            if(!item.requested||item.protected_from_action)
                continue;
            if(!item.exact_current)
                return {{},MacroSeedReason::missing_exact_evidence,0};
            if(!std::isfinite(item.z_fixed)||
               !std::isfinite(item.z_optimization)||
               !std::isfinite(item.comparison_scale)||
               item.comparison_scale<=0.0||
               !std::isfinite(item.frozen_deadband)||
               item.frozen_deadband<0.0||
               !std::isfinite(item.configured_mass)||
               item.configured_mass<=0.0||
               !std::isfinite(item.paired_support)||
               item.paired_support<0.0||item.paired_support>1.0)
                return {{},MacroSeedReason::invalid_frozen_component,0};
            // Unsupported components remain in M05/M06 objective mass,
            // but cannot vote or have mass redistributed by this seed.
            if(item.paired_support==0.0)continue;
            const double signed_delta=
                item.z_fixed-item.z_optimization;
            if(!std::isfinite(signed_delta))
                return {{},MacroSeedReason::invalid_frozen_component,0};
            if(std::abs(signed_delta)<=item.frozen_deadband)
                continue; // no action vote; objective is unaffected
            const double orientation=g==0&&j==0?-1.0:1.0;
            const double reduction=
                orientation*signed_delta/item.comparison_scale;
            if(!std::isfinite(reduction))
                return {{},MacroSeedReason::invalid_frozen_component,0};
            if(reduction<0.0) {
                group_expansion=true;
                any_expansion=true;
                continue;
            }
            if(reduction>0.0) {
                group_reduction=true;
                const double weight=
                    item.configured_mass*item.paired_support;
                group_weighted_sum+=weight*reduction;
                group_active_mass+=weight;
            }
        }
        // Each vector is ONE coherent group, not three independent votes.
        if(group_reduction&&!group_expansion&&group_active_mass>0.0) {
            ++group_count;
            weighted_sum+=group_weighted_sum;
            active_mass+=group_active_mass;
        }
    }
    if(any_expansion)
        return {{},MacroSeedReason::contradictory_expansion,group_count};
    if(group_count<2||active_mass<=0.0)
        return {{},MacroSeedReason::insufficient_coherent_distinct_groups,
                group_count};
    const double movement=weighted_sum/active_mass;
    if(!std::isfinite(movement)||movement<=0.0)
        return {{},MacroSeedReason::invalid_frozen_component,group_count};
    return {movement,MacroSeedReason::ready,group_count};
}
enum class ThresholdSeedReason:std::uint8_t {
    ready,missing_exact_native_rms,invalid_native_series,
    invalid_knee_or_nonfinite_result
};
struct ThresholdSeed final {
    std::optional<double> threshold_dbfs;
    ThresholdSeedReason reason{ThresholdSeedReason::missing_exact_native_rms};
};
// Type7 quantile of an exact complete native 400ms/Grid100 Fixed RMS
// series; caller attests source-frame membership and units. No D03
// spread scalar, M19 UI history or fabricated sample joins are allowed.
[[nodiscard]] inline ThresholdSeed compressor_threshold_type7_p75(
    std::span<const double> exact_native_fixed_rms_dbfs,
    double canonical_knee_db,
    bool exact_native_series_and_fixed_snapshot)
{
    if(!exact_native_series_and_fixed_snapshot||
       exact_native_fixed_rms_dbfs.empty())
        return {{},ThresholdSeedReason::missing_exact_native_rms};
    if(!std::isfinite(canonical_knee_db)||canonical_knee_db<0.0)
        return {{},ThresholdSeedReason::invalid_knee_or_nonfinite_result};
    std::vector<double> ordered{
        exact_native_fixed_rms_dbfs.begin(),
        exact_native_fixed_rms_dbfs.end()};
    for(const double v:ordered)
        if(!std::isfinite(v))
            return {{},ThresholdSeedReason::invalid_native_series};
    std::sort(ordered.begin(),ordered.end());
    const double rank=0.75*static_cast<double>(ordered.size()-1U);
    const auto lo=static_cast<std::size_t>(std::floor(rank));
    const auto hi=static_cast<std::size_t>(std::ceil(rank));
    const double fraction=rank-static_cast<double>(lo);
    const double p75=ordered[lo]*(1.0-fraction)+
                     ordered[hi]*fraction;
    const double threshold=p75-canonical_knee_db/2.0;
    if(!std::isfinite(threshold))
        return {{},ThresholdSeedReason::invalid_knee_or_nonfinite_result};
    return {threshold,ThresholdSeedReason::ready};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
