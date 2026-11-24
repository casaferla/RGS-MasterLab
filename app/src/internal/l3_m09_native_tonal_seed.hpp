#pragma once
#include "l3_m09_native_eq_prefix.hpp"
#include "l3_m11_native_fixed_assessment.hpp"
#include "l3_m20_t01_primary_binding.hpp"
namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeTonalFitPopulation final {
    std::vector<NativeEqFitCell> active;
    std::vector<std::optional<double>> eligible_grid;
};
[[nodiscard]] inline std::optional<NativeTonalFitPopulation>
native_tonal_fit_population(const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    std::span<const m05m06_real::T01ConfiguredBand> grid)
{
    const auto assessment=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    if(!assessment.valid_current||!assessment.corrective[0]||grid.empty())return {};
    const auto protection=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!protection)return {};
    const auto& slot=fixed.measured_fixed.front();
    if(slot.binding_id!="rgsml.reference-match.binding.tonal.t01"||
       slot.configured.size()!=grid.size()||protection->size()<grid.size())return {};
    const auto& binding=m07_frozen::kBindings.front();
    NativeTonalFitPopulation out;
    out.eligible_grid.resize(grid.size());
    double weight=0.0;
    for(std::size_t i=0;i<grid.size();++i) {
        const auto& b=grid[i];const auto& p=slot.configured[i];const auto& g=(*protection)[i];
        if(!std::isfinite(b.center_hz)||!(b.low_hz<b.center_hz&&b.center_hz<b.high_hz)||
           (i&&b.canonical_index!=grid[i-1].canonical_index+1))return {};
        if(!g.required_in_frozen_k)continue;
        if(!g.exact_current||!p.current_candidate_z||
           p.candidate_native.selector!=std::to_string(b.canonical_index))return {};
        const auto target=m08_frozen::desired_for_component(amount,0,p.prepared,p.reference);
        if(!target.value)return {};
        const double delta=target.value->constraint_adjusted_z.value_or(target.value->desired_z)-*p.current_candidate_z;
        if(!std::isfinite(delta))return {};
        // Protected cells stay in the full fit with zero desired movement.
        out.active.push_back({b.center_hz,g.protected_by_m11_mask?0.0:delta,p.paired_native_support});
        weight+=p.paired_native_support;
        if(!g.protected_by_m11_mask)out.eligible_grid[i]=delta;
    }
    if(weight<=0.0||!std::isfinite(weight))return {};
    const auto smoothed=smooth_selected_valid_runs(out.eligible_grid,1.0/12.0);
    if(smoothed.failure!=GridFailure::none||
       !has_three_adjacent_signed_action(smoothed.values,binding.deadband))return {};
    out.eligible_grid=smoothed.values;
    for(auto& cell:out.active)cell.frozen_normalized_weight/=weight;
    return out;
}
// Caller supplies the allocated dictionary identities. Only atoms on
// eligible canonical centers and with frozen dictionary shapes enter.
[[nodiscard]] inline std::vector<NativeEqPrefix> derive_native_tonal_prefixes(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    std::span<const m05m06_real::T01ConfiguredBand> grid,
    std::span<const NativeEqAtom> dictionary,core::SampleRate rate)
{
    const auto population=native_tonal_fit_population(fixed,amount,requested,grid);
    if(!population)return {};
    std::vector<NativeEqAtom> legal;
    for(const auto& atom:dictionary) {
        const auto it=std::find_if(grid.begin(),grid.end(),[&](const auto& band){
            return band.center_hz==atom.frequency_hz;
        });
        if(it==grid.end())return {};
        const auto i=static_cast<std::size_t>(it-grid.begin());
        if(!population->eligible_grid[i])continue;
        if(atom.filter==AtomFilter::bell) {
            constexpr std::array<double,5> q{0.35,0.50,0.707,1.0,1.4};
            if(std::find(q.begin(),q.end(),atom.q_or_slope)==q.end())return {};
        } else if(atom.filter==AtomFilter::low_shelf||atom.filter==AtomFilter::high_shelf) {
            if(atom.q_or_slope!=0.5&&atom.q_or_slope!=1.0)return {};
            // Canonical centers are ordered on the logarithmic grid.
            // The supplied dictionary retains its boundary eligibility.
        } else return {};
        legal.push_back(atom);
    }
    auto active=population->active;
    for(auto& cell:active) {
        const auto it=std::find_if(grid.begin(),grid.end(),[&](const auto& b){
            return b.center_hz==cell.frequency_hz;
        });
        if(it==grid.end())return {};
        const auto& smoothed=population->eligible_grid[static_cast<std::size_t>(it-grid.begin())];
        if(smoothed)cell.residual_db=*smoothed;
    }
    return fit_native_eq_prefixes(active,legal,rate,false,m07_frozen::kBindings.front().deadband);
}
} // namespace rgsml::app::l3_automatch::m09_frozen
