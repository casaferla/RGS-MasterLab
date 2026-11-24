#pragma once
#include "l3_m09_p02_macro_reduction.hpp"
#include "l3_m09_abstract_family_enumeration.hpp"
#include "l3_m11_native_fixed_assessment.hpp"
#include <rgsml/dsp/compressor_parameters.hpp>
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeCompressorSeed final {
    dsp::CompressorParameters payload;
    double required_movement{};
    double raw_threshold_dbfs{};
    bool threshold_limited{};
};
[[nodiscard]] inline std::optional<std::vector<double>>
fixed_native_rms_for_macro(const m11_frozen::NativeFixedPopulation& fixed,
                           const m08_frozen::AuthenticatedPairSlot& d03)
{
    if(!fixed.measured_fixed_native_windows||d03.configured.size()!=1U)return {};
    const auto& series=*fixed.measured_fixed_native_windows;
    const auto& native=d03.configured.front().candidate_native;
    if(series.problem!=m05m06_real::NativeWindowProblem::none||
       native.selector!="P95-P10"||
       native.valid_count!=std::optional<std::uint64_t>{series.valid_rms_windows}||
       native.configured_count!=std::optional<std::uint64_t>{series.complete_native_windows.size()})
        return {};
    m05m06_real::NativeMembershipIdentity membership{"D03-RMS-grid400-hop100/1"};
    std::vector<double> rms;
    rms.reserve(series.complete_native_windows.size());
    for(const auto& window:series.complete_native_windows) {
        if(window.absolute_start_frame<0)return {};
        membership.append(static_cast<std::uint64_t>(window.absolute_start_frame));
        membership.append(window.rms_dbfs.has_value()?1U:0U);
        if(window.rms_dbfs) {
            if(!std::isfinite(*window.rms_dbfs))return {};
            rms.push_back(*window.rms_dbfs);
        }
    }
    if(rms.empty()||rms.size()!=series.valid_rms_windows||
       membership.finish()!=native.population_identity)return {};
    const auto selectors=m05m06_real::extract_d03_d04_type7(series);
    if(!selectors.d03_type7_rms_p95_minus_p10_db||
       selectors.d03_type7_rms_p95_minus_p10_db!=d03.configured.front().current_candidate_z)
        return {};
    return rms;
}
[[nodiscard]] inline std::optional<NativeCompressorSeed>
derive_native_macro_seed(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    MacroMode mode,bool stereo_input)
{
    if(mode==MacroMode::none||(!stereo_input&&mode==MacroMode::stereo_mean)||
       (mode!=MacroMode::linked_max&&mode!=MacroMode::stereo_mean))return {};
    constexpr auto domain=static_cast<std::size_t>(m07_frozen::Domain::dynamics);
    const auto targets=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    if(!targets.valid_current||!targets.corrective[domain])return {};
    const auto protection=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!protection)return {};
    constexpr std::array<std::string_view,6> ids{
        "rgsml.reference-match.binding.dynamics.d01","rgsml.reference-match.binding.dynamics.d02",
        "rgsml.reference-match.binding.dynamics.d03","rgsml.reference-match.binding.dynamics.d04",
        "rgsml.reference-match.binding.dynamics.d05","rgsml.reference-match.binding.dynamics.d06"};
    std::array<MacroGroup,6> groups{};
    const m08_frozen::AuthenticatedPairSlot* d03=nullptr;
    std::size_t offset=0;
    for(std::size_t i=0;i<fixed.measured_fixed.size();++i) {
        const auto& slot=fixed.measured_fixed[i];
        const auto& binding=m07_frozen::kBindings[i];
        const auto found=std::find(ids.begin(),ids.end(),slot.binding_id);
        if(found!=ids.end()) {
            const auto group=static_cast<std::size_t>(found-ids.begin());
            const auto n=slot.configured.size();
            if(n==0U||offset+n>protection->size()||!groups[group].components.empty())return {};
            if(group==2U)d03=&slot;
            const double mass=static_cast<double>(binding.domain_mass.numerator)/
                static_cast<double>(binding.domain_mass.denominator)/static_cast<double>(n);
            for(std::size_t j=0;j<n;++j) {
                const auto& part=slot.configured[j];
                const auto& guard=(*protection)[offset+j];
                MacroComponent out{};
                out.requested=guard.required_in_frozen_k;
                out.protected_from_action=guard.protected_by_m11_mask;
                if(out.requested&&!out.protected_from_action) {
                    const auto desired=m08_frozen::desired_for_component(amount,domain,part.prepared,part.reference);
                    if(!desired.value||!part.current_candidate_z)return {};
                    out.exact_current=guard.exact_current;
                    out.z_fixed=*part.current_candidate_z;
                    out.z_optimization=desired.value->constraint_adjusted_z.value_or(desired.value->desired_z);
                    out.comparison_scale=binding.scale;
                    out.frozen_deadband=binding.deadband;
                    out.configured_mass=mass;
                    out.paired_support=part.paired_native_support;
                }
                groups[group].components.push_back(out);
            }
        }
        offset+=slot.configured.size();
    }
    const auto reduction=reduce_macro_p02(groups,true);
    if(!reduction.required_movement||!d03)return {};
    const auto rms=fixed_native_rms_for_macro(fixed,*d03);
    if(!rms)return {};
    constexpr double knee=6.0;
    const auto threshold=compressor_threshold_type7_p75(*rms,knee,true);
    if(!threshold.threshold_dbfs)return {};
    const double movement=*reduction.required_movement;
    const double ratio=movement<=1.0/3.0?1.25:movement<=2.0/3.0?1.5:2.0;
    const double bounded=std::clamp(*threshold.threshold_dbfs,-120.0,0.0);
    auto payload=dsp::CompressorParameters::create(dsp::CompressorDetectorMode::RMS,
        mode==MacroMode::linked_max?dsp::CompressorChannelLink::LINKED_MAX:
        dsp::CompressorChannelLink::LINKED_MEAN,bounded,ratio,knee,30.0,200.0,50.0,5.0,100.0,0.0);
    if(!payload)return {};
    return NativeCompressorSeed{*payload.value(),movement,*threshold.threshold_dbfs,
                                bounded!=*threshold.threshold_dbfs};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
