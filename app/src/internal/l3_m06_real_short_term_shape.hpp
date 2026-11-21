#pragma once
// Frozen L3-M06 D01: short-term loudness distribution shape derived
// solely from M05 L2's authentic complete, UNGATED 3s/100ms series
// already computed by the original M19 K-weighting pass.
// D01's P10−P50/P90−P50/P95−P50 are three components sharing one
// frozen D01 domain mass 0.15. This pure observation is NOT itself a
// Phase6 valid/reference-grade verdict, M07 feature or M10 cost.
#include "l3_m05m06_real_analysis_port.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
[[nodiscard]] inline std::optional<double>
type7_percentile(std::span<const double> sorted,double p) noexcept {
    if(sorted.empty()||!std::isfinite(p)||p<0.0||p>1.0)
        return std::nullopt;
    for(std::size_t i=0;i<sorted.size();++i)
        if(!std::isfinite(sorted[i])
           ||(i>0&&sorted[i]<sorted[i-1]))return std::nullopt;
    if(sorted.size()==1)return sorted.front();
    const double index=(static_cast<double>(sorted.size())-1.0)*p;
    const auto floor_index=static_cast<std::size_t>(std::floor(index));
    const auto upper_index=std::min(
        sorted.size()-1,floor_index+1);
    const double delta=index-static_cast<double>(floor_index);
    return sorted[floor_index]*(1.0-delta)+
           sorted[upper_index]*delta;
}
struct D01ShortTermShape final {
    std::array<double,3> relative_to_p50_lu{};
    std::array<double,4> absolute_type7_lufs{}; // P10 P50 P90 P95
    std::uint64_t complete_windows{};
    std::uint64_t valid_windows{};
    double native_support{}; // valid native / complete native
};
[[nodiscard]] inline std::optional<D01ShortTermShape> observe_d01(
    const SourceReadings& source)
{
    if(source.short_term_complete_windows==0
       ||source.short_term_3s_grid100.size()!=
           source.short_term_complete_windows
       ||!source.short_term_native_support
       ||!source.short_term_p50_lufs.ready())
        return std::nullopt;
    std::vector<double> sorted;
    sorted.reserve(source.short_term_3s_grid100.size());
    for(const auto& w:source.short_term_3s_grid100)
        if(w.lufs.ready())
            sorted.push_back(*w.lufs.unrounded);
    if(sorted.size()!=source.short_term_valid_windows||
       sorted.empty())return std::nullopt;
    std::sort(sorted.begin(),sorted.end());
    const auto p10=type7_percentile(sorted,0.10);
    const auto p50=type7_percentile(sorted,0.50);
    const auto p90=type7_percentile(sorted,0.90);
    const auto p95=type7_percentile(sorted,0.95);
    if(!p10||!p50||!p90||!p95)return std::nullopt;
    if(std::abs(*p50-*source.short_term_p50_lufs.unrounded)>1e-8)
        return std::nullopt; // Type7 mismatch with original M19
    const double support=static_cast<double>(sorted.size())/
        static_cast<double>(source.short_term_complete_windows);
    if(!std::isfinite(support)||support<0.0||support>1.0
       ||std::abs(support-*source.short_term_native_support)>1e-12)
        return std::nullopt;
    return D01ShortTermShape{
        {*p10-*p50,*p90-*p50,*p95-*p50},
        {*p10,*p50,*p90,*p95},
        source.short_term_complete_windows,
        source.short_term_valid_windows,
        support};
}
struct PairedD01Shape final {
    D01ShortTermShape candidate;
    D01ShortTermShape reference;
    double pair_support{}; // frozen B03 min(sPrepared,sGold)
    [[nodiscard]] constexpr bool authorizes_m10()
        const noexcept {return false;}
};
[[nodiscard]] inline std::optional<PairedD01Shape> pair_d01(
    const SourceReadings& candidate,const SourceReadings& reference)
{
    if(candidate.owner.resource_identity.empty()
       ||reference.owner.resource_identity.empty()
       ||candidate.owner.resource_identity==
           reference.owner.resource_identity
       ||!candidate.owner.full_track_owner_confirmed
       ||!reference.owner.full_track_owner_confirmed
       ||candidate.owner.generation==0||reference.owner.generation==0)
        return std::nullopt;
    const auto a=observe_d01(candidate);
    const auto b=observe_d01(reference);
    if(!a||!b)return std::nullopt;
    return PairedD01Shape{*a,*b,
                          std::min(a->native_support,b->native_support)};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
