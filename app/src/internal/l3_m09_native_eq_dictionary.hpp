#pragma once
#include "l3_m09_native_tonal_seed.hpp"
#include <set>

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeEqDictionaryIdentity final {
    int canonical_index{};
    AtomFilter filter{};
    double q_or_slope{};
};
// Allocator supplies trial UUIDs under existing owner authority. Never
// use a nearest center, interpolate missing support, or derive a new
// persistence identity from a floating-point frequency.
template<class AllocateBand>
[[nodiscard]] inline std::optional<std::vector<NativeEqAtom>>
derive_native_eq_dictionary(
    std::span<const m05m06_real::T01ConfiguredBand> grid,
    const NativeTonalFitPopulation& population,double common_low,double common_high,
    core::SampleRate rate,bool side,AllocateBand&& allocate)
{
    if(grid.size()!=population.eligible_grid.size()||grid.empty()||
       !std::isfinite(common_low)||!std::isfinite(common_high)||
       common_low<20.0||common_high<=common_low||
       common_high>std::min(20000.0,0.45*static_cast<double>(rate.value())))return {};
    const double lo=std::log2(common_low),hi=std::log2(common_high);
    const double third=(hi-lo)/3.0;
    std::vector<NativeEqAtom> result;
    std::set<core::Uuid> ids;
    for(std::size_t i=0;i<grid.size();++i) {
        const auto& b=grid[i];
        if(!population.eligible_grid[i])continue;
        if(!std::isfinite(b.center_hz)||b.center_hz<common_low||b.center_hz>common_high)
            return {};
        const auto add=[&](AtomFilter filter,double shape){
            const auto id=std::invoke(allocate,NativeEqDictionaryIdentity{b.canonical_index,filter,shape});
            if(!id||id->is_nil()||!ids.insert(*id).second)return false;
            result.push_back({*id,filter,b.center_hz,shape});return true;
        };
        const auto log=std::log2(b.center_hz);
        if(log<=lo+third)for(double slope:{0.5,1.0})
            if(!add(AtomFilter::low_shelf,slope))return {};
        for(double q:{0.35,0.50,0.707,1.0,1.4}) {
            if(side&&q>1.0)continue;
            if(!add(AtomFilter::bell,q))return {};
        }
        if(log>=hi-third)for(double slope:{0.5,1.0})
            if(!add(AtomFilter::high_shelf,slope))return {};
    }
    return result;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
