#pragma once
#include "l3_m09_p02_spectral_seed.hpp"
#include "l3_m11_native_fixed_assessment.hpp"
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
// The Fixed carrier and the three original M07 vectors must have
// identical canonical order. D12 is a compatibility gate, not a knob.
[[nodiscard]] inline std::optional<std::vector<SpectralSeedCell>>
native_fixed_spectral_cells(const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested)
{
    if(!fixed.measured_fixed_spectral_energy)return {};
    const auto& carrier=*fixed.measured_fixed_spectral_energy;
    const auto assessment=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    if(!assessment.valid_current||!assessment.corrective[1])return {};
    const auto guards=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!guards)return {};
    std::array<const m08_frozen::AuthenticatedPairSlot*,3> slots{};
    std::array<std::size_t,3> offsets{};
    std::array<double,3> deadbands{};
    std::size_t offset=0;
    constexpr std::array<std::string_view,3> ids{
        "rgsml.reference-match.binding.dynamics.d10",
        "rgsml.reference-match.binding.dynamics.d11",
        "rgsml.reference-match.binding.dynamics.d12"};
    for(std::size_t i=0;i<fixed.measured_fixed.size();++i) {
        const auto& s=fixed.measured_fixed[i];
        for(std::size_t d=0;d<3U;++d)if(s.binding_id==ids[d]) {
            slots[d]=&s;offsets[d]=offset;deadbands[d]=m07_frozen::kBindings[i].deadband;
        }
        offset+=s.configured.size();
    }
    for(const auto* s:slots)if(!s||s->configured.size()!=carrier.grid.size())return {};
    if(!native_fixed_spectral_carrier_matches(carrier,*slots[0]))return {};
    std::vector<SpectralSeedCell> cells;
    cells.reserve(carrier.grid.size());
    for(std::size_t i=0;i<carrier.grid.size();++i) {
        const auto& band=carrier.grid[i];
        const auto& p=slots[0]->configured[i];const auto& a=slots[1]->configured[i];
        const auto& c=slots[2]->configured[i];
        SpectralSeedCell cell;
        cell.canonical_low_edge_hz=band.low_hz;
        cell.canonical_high_edge_hz=band.high_hz;
        cell.canonical_center_hz=1000.0*std::exp2(static_cast<double>(band.exact_canonical_index)/12.0);
        cell.d10_deadband=deadbands[0];cell.d11_deadband=deadbands[1];
        cell.d10_pair_support=p.paired_native_support;
        const auto& g=(*guards)[offsets[0]+i];
        cell.d10_requested=g.required_in_frozen_k&&!g.protected_by_m11_mask;
        cell.d10_exact_current=g.exact_current&&p.current_candidate_z.has_value();
        cell.d11_exact_current=(*guards)[offsets[1]+i].required_in_frozen_k&&
            (*guards)[offsets[1]+i].exact_current&&a.current_candidate_z.has_value();
        cell.d12_exact_compatible=(*guards)[offsets[2]+i].required_in_frozen_k&&
            (*guards)[offsets[2]+i].exact_current&&c.current_candidate_z.has_value();
        if(cell.d10_requested&&cell.d10_exact_current&&cell.d11_exact_current) {
            const auto ptarget=m08_frozen::desired_for_component(amount,1,p.prepared,p.reference);
            const auto atarget=m08_frozen::desired_for_component(amount,1,a.prepared,a.reference);
            if(!ptarget.value||!atarget.value)return {};
            cell.d10_optimization_minus_fixed=ptarget.value->constraint_adjusted_z.value_or(ptarget.value->desired_z)-*p.current_candidate_z;
            cell.d11_optimization_minus_fixed=atarget.value->constraint_adjusted_z.value_or(atarget.value->desired_z)-*a.current_candidate_z;
        }
        cells.push_back(cell);
    }
    return cells;
}
struct NativeDynamicPrefix final {
    dsp::DynamicEqParameters payload;
    std::vector<NativeRankedCluster> ranked_clusters;
    std::vector<NativeDynamicSeed> seeds;
};
[[nodiscard]] inline std::vector<NativeDynamicPrefix> derive_native_dynamic_prefixes(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    std::span<const core::Uuid> allocated_ranked_band_ids,std::uint32_t channels)
{
    const auto cells=native_fixed_spectral_cells(fixed,amount,requested);
    if(!cells)return {};
    const auto& carrier=*fixed.measured_fixed_spectral_energy;
    const auto ranked=rank_native_spectral_clusters(*cells);
    if(allocated_ranked_band_ids.size()<ranked.size())return {};
    std::vector<NativeDynamicPrefix> out;
    std::vector<dsp::DynamicEqBandParameters> bands;
    std::vector<NativeRankedCluster> selected;
    std::vector<NativeDynamicSeed> seeds;
    for(std::size_t i=0;i<ranked.size();++i) {
        const auto& item=ranked[i];
        const auto threshold=native_fixed_cluster_threshold(carrier,item.first,item.count);
        const auto seed=native_dynamic_seed(item.seed,threshold,allocated_ranked_band_ids[i],
            item.first==0U,item.first+item.count==cells->size(),carrier.sample_rate,channels);
        if(!seed)break; // do not substitute a lower-ranked cluster
        bands.push_back(seed->payload);selected.push_back(item);seeds.push_back(*seed);
        auto payload=dsp::DynamicEqParameters::create(bands);
        if(!payload)return {};
        out.push_back({std::move(*payload.value()),selected,seeds});
    }
    return out;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
