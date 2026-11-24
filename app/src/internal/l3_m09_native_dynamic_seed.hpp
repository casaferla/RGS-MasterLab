#pragma once
#include "l3_m09_p02_spectral_seed.hpp"
#include "l3_m11_native_fixed_population.hpp"
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeDynamicSeed final {
    dsp::DynamicEqBandParameters payload;
    double raw_range_db{},raw_q{},raw_threshold_dbfs{};
    bool parameter_limited{};
};
[[nodiscard]] inline std::optional<NativeDynamicSeed> native_dynamic_seed(
    const SpectralClusterSeed& cluster,const NativeClusterThreshold& threshold,
    const core::Uuid& id,bool lower_boundary,bool upper_boundary,
    std::uint32_t rate,std::uint32_t channels)
{
    if((cluster.reason!=SpectralSeedReason::ready&&
        cluster.reason!=SpectralSeedReason::limited_to_seed_range)||
       threshold.reason!=NativeClusterSeriesReason::ready||!threshold.p75_dbfs||
       !cluster.weighted_center_hz||!cluster.cluster_lower_edge_hz||
       !cluster.cluster_upper_edge_hz||!cluster.raw_spread_delta||
       !cluster.limited_dynamic_range_db)return {};
    const double width=*cluster.cluster_upper_edge_hz-*cluster.cluster_lower_edge_hz;
    if(!(width>0.0)||!std::isfinite(width))return {};
    const double center=*cluster.weighted_center_hz;
    const double q=center/width;
    if(!std::isfinite(q))return {};
    const double threshold_db=std::clamp(*threshold.p75_dbfs,-120.0,0.0);
    dsp::DynamicEqBandSpec band;
    band.band_id=id;
    band.filter_type=lower_boundary?dsp::DynamicEqFilterType::LOW_SHELF:
        upper_boundary?dsp::DynamicEqFilterType::HIGH_SHELF:dsp::DynamicEqFilterType::BELL;
    const double bounded_q=std::clamp(q,0.35,4.0);
    if(lower_boundary||upper_boundary)band.shape=dsp::DynamicEqShelfShape{center,1.0};
    else band.shape=dsp::DynamicEqBellShape{center,bounded_q};
    // C01 exposes only these controls; legacy proposed knee/lookahead
    // are not represented and are not silently added to the DSP schema.
    band.control={0.0,*cluster.limited_dynamic_range_db,threshold_db,1.5,50.0,30.0,200.0};
    auto payload=dsp::DynamicEqBandParameters::create(std::move(band));
    if(!payload||!payload.value()->validate_for_audio(rate,channels))return {};
    return NativeDynamicSeed{std::move(*payload.value()),*cluster.raw_spread_delta,q,
        *threshold.p75_dbfs,*cluster.raw_spread_delta!=*cluster.limited_dynamic_range_db||
        (!lower_boundary&&!upper_boundary&&q!=bounded_q)||threshold_db!=*threshold.p75_dbfs};
}
// Retain only rows with all native members valid under the original
// D10-D12 power floor. Never infer absolute energy from spread values.
[[nodiscard]] inline NativeClusterThreshold native_fixed_cluster_threshold(
    const NativeSpectralSeedCarrier& carrier,std::size_t first,std::size_t count)
{
    if(count<3U||first>carrier.grid.size()||count>carrier.grid.size()-first||
       carrier.energy.size()!=carrier.ordinals.size()||carrier.energy.empty()||
       std::adjacent_find(carrier.ordinals.begin(),carrier.ordinals.end(),
         [](auto a,auto b){return a>=b;})!=carrier.ordinals.end())return {};
    std::vector<std::vector<std::optional<double>>> rows;
    rows.reserve(carrier.energy.size());
    for(const auto& frame:carrier.energy) {
        if(frame.size()!=carrier.grid.size())return {};
        double total=0.0;bool complete=true;
        for(const auto& e:frame) {
            if(!e){complete=false;continue;}
            if(!std::isfinite(*e)||*e<0.0)return {};
            total+=*e;
        }
        if(!std::isfinite(total))return {};
        std::vector<std::optional<double>> row(count);
        if(complete&&total>=m05m06_real::kSpectralBandMinPower) {
            const double floor=std::max(m05m06_real::kSpectralBandMinPower,
                total*m05m06_real::kSpectralBandMinPower);
            for(std::size_t k=0;k<count;++k)
                if(*frame[first+k]>=floor)row[k]=frame[first+k];
        }
        rows.push_back(std::move(row));
    }
    return derive_cluster_type7_p75(rows,count,true);
}
} // namespace rgsml::app::l3_automatch::m09_frozen
