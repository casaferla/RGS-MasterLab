#pragma once
#include "l3_m09_native_tonal_seed.hpp"

namespace rgsml::app::l3_automatch::m09_frozen {
[[nodiscard]] inline std::optional<NativeTonalFitPopulation>
native_side_fit_population(const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    std::span<const m05m06_real::T01ConfiguredBand> grid,int denominator)
{
    if(grid.empty()||(denominator!=3&&denominator!=12))return {};
    const auto assessment=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    if(!assessment.valid_current||!assessment.corrective[2])return {};
    const auto protection=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!protection)return {};
    const m08_frozen::AuthenticatedPairSlot* slot=nullptr;
    std::size_t offset=0;
    for(const auto& s:fixed.measured_fixed) {
        if(s.binding_id=="rgsml.reference-match.binding.stereo.s03"){slot=&s;break;}
        offset+=s.configured.size();
    }
    if(!slot||slot->configured.size()!=grid.size()||offset+grid.size()>protection->size())return {};
    const auto* binding=m07_frozen::find_binding(slot->binding_id);
    if(!binding)return {};
    NativeTonalFitPopulation out;
    out.eligible_grid.resize(grid.size());
    const double db_per_octave=20.0*std::log10(2.0);
    double weight=0.0;
    for(std::size_t i=0;i<grid.size();++i) {
        const auto& b=grid[i];const auto& p=slot->configured[i];const auto& g=(*protection)[offset+i];
        if(!std::isfinite(b.center_hz)||!(b.low_hz<b.center_hz&&b.center_hz<b.high_hz)||
           (i&&b.canonical_index!=grid[i-1].canonical_index+1)||
           std::abs(std::log2(b.center_hz/1000.0)-static_cast<double>(b.canonical_index)/denominator)>1e-10)
            return {};
        if(!g.required_in_frozen_k)continue;
        if(!g.exact_current||!p.current_candidate_z||
           p.candidate_native.selector!=std::to_string(b.canonical_index))return {};
        const auto target=m08_frozen::desired_for_component(amount,2,p.prepared,p.reference);
        if(!target.value)return {};
        using E=m08_frozen::Endpoint;
        // Neither a muted lane nor a requested exact mute is represented
        // by a finite parametric EQ gain. Split the eligible run.
        if(p.candidate_native.endpoint==E::mono_side_muted||
           target.value->endpoint==E::mono_side_muted)continue;
        const double delta=db_per_octave*(target.value->constraint_adjusted_z.value_or(target.value->desired_z)-*p.current_candidate_z);
        if(!std::isfinite(delta))return {};
        out.active.push_back({b.center_hz,g.protected_by_m11_mask?0.0:delta,p.paired_native_support});
        weight+=p.paired_native_support;
        if(!g.protected_by_m11_mask)out.eligible_grid[i]=delta;
    }
    if(!std::isfinite(weight)||weight<=0.0)return {};
    const auto smoothed=smooth_selected_valid_runs(out.eligible_grid,1.0/denominator);
    if(smoothed.failure!=GridFailure::none||
       !has_three_adjacent_signed_action(smoothed.values,db_per_octave*binding->deadband))return {};
    out.eligible_grid=smoothed.values;
    for(auto& c:out.active)c.frozen_normalized_weight/=weight;
    return out;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
