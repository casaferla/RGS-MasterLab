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
// Validate the retained carrier against the actual measured Fixed D10
// population before using it for any threshold. Selectors are positions
// in the exact ordered canonical grid, not arbitrary band UUIDs.
[[nodiscard]] inline bool native_fixed_spectral_carrier_matches(
    const NativeSpectralSeedCarrier& carrier,
    const m08_frozen::AuthenticatedPairSlot& d10)
{
    if(carrier.grid.size()!=d10.configured.size()||carrier.energy.empty()||
       carrier.ordinals.size()!=carrier.energy.size())return false;
    const auto measured=m05m06_real::reduce_native_spectral_d10_d12(
        carrier.energy,carrier.grid.size(),true,carrier.active_pcm_frames,
        carrier.sample_rate,carrier.ordinals);
    if(measured.source_reason!=m05m06_real::SpectralNativeReason::ready||
       measured.canonical_band_results.size()!=d10.configured.size())return false;
    for(std::size_t i=0;i<d10.configured.size();++i) {
        const auto& p=d10.configured[i];const auto& b=measured.canonical_band_results[i];
        if(p.candidate_native.selector!=std::to_string(i)||
           p.candidate_native.population_identity!=b.exact_frame_population_identity||
           p.candidate_native.valid_count!=std::optional<std::uint64_t>{b.valid_native_counts[0]}||
           p.candidate_native.configured_count!=std::optional<std::uint64_t>{measured.candidate_active_frames})
            return false;
        if(p.current_candidate_z&&p.current_candidate_z!=b.d10_d11_d12_z[0])return false;
    }
    return true;
}
struct NativeRankedCluster final {
    std::size_t first{},count{};
    SpectralClusterSeed seed;
};
[[nodiscard]] inline std::vector<NativeRankedCluster> rank_native_spectral_clusters(
    std::span<const SpectralSeedCell> cells)
{
    std::vector<NativeRankedCluster> out;
    const auto sign=[](const SpectralSeedCell& c){
        if(!c.d10_requested||!c.d10_exact_current||!c.d11_exact_current||
           !c.d12_exact_compatible||!std::isfinite(c.d10_optimization_minus_fixed)||
           std::abs(c.d10_optimization_minus_fixed)<=c.d10_deadband||
           c.d10_pair_support<=0.0)return 0;
        const int d=c.d10_optimization_minus_fixed>0.0?1:-1;
        if(std::abs(c.d11_optimization_minus_fixed)>c.d11_deadband&&
           (c.d11_optimization_minus_fixed>0.0?1:-1)!=d)return 0;
        return d;
    };
    for(std::size_t i=0;i<cells.size();) {
        const int d=sign(cells[i]);
        if(!d){++i;continue;}
        std::size_t end=i+1;
        while(end<cells.size()&&sign(cells[end])==d&&
              cells[end-1].canonical_high_edge_hz==cells[end].canonical_low_edge_hz)++end;
        auto seed=derive_d10_spectral_cluster_p02(cells.subspan(i,end-i),true,
            i==0U&&end==cells.size());
        if(seed.selection_score)out.push_back({i,end-i,std::move(seed)});
        i=end;
    }
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){
        if(a.seed.selection_score!=b.seed.selection_score)
            return a.seed.selection_score>b.seed.selection_score;
        return a.first<b.first;
    });
    if(out.size()>3U)out.resize(3U);
    return out;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
