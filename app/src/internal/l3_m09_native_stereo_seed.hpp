#pragma once
#include "l3_m09_p03_stereo_low_seed.hpp"
#include "l3_m11_native_fixed_assessment.hpp"
#include <rgsml/dsp/stereo_ms_parameters.hpp>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeStereoSeed final {
    dsp::StereoMsParameters payload;
    double raw_mid_db{},raw_side_db{};
    std::optional<double> raw_low_width_percent;
    bool parameter_limited{};
};
struct NativeWidthTarget final {
    double fixed_z{},desired_z{};
    bool fixed_mono{},target_mono{},needs_action{};
};
[[nodiscard]] inline std::optional<NativeWidthTarget>
native_width_target(const m08_frozen::AuthenticatedPairComponent& part,
    const m11_frozen::ProtectedComponent& guard,const m08_frozen::AmountConfig& amount,
    double deadband)
{
    if(!part.current_valid||!guard.required_in_frozen_k||!guard.exact_current||
       !part.current_candidate_z||!part.candidate_native.value||
       !std::isfinite(*part.current_candidate_z)||
       !std::isfinite(*part.candidate_native.value)||*part.candidate_native.value<0.0)
        return {};
    using E=m08_frozen::Endpoint;
    const bool fixed_mono=part.candidate_native.endpoint==E::mono_side_muted;
    if(fixed_mono!=(*part.candidate_native.value==0.0))return {};
    const auto target=m08_frozen::desired_for_component(amount,
        static_cast<std::size_t>(m07_frozen::Domain::stereo),part.prepared,part.reference);
    if(!target.value)return {};
    const double desired=target.value->constraint_adjusted_z.value_or(target.value->desired_z);
    const bool mono=target.value->endpoint==E::mono_side_muted;
    if(!std::isfinite(desired))return {};
    return NativeWidthTarget{*part.current_candidate_z,desired,fixed_mono,mono,
        !guard.protected_by_m11_mask&&(fixed_mono!=mono||
        std::abs(desired-*part.current_candidate_z)>deadband)};
}
// A composite insertion baseline after SIDE EQ is separately measured.
// Original target and M11 protection stay anchored to independent Fixed,
// not to the post-SIDE candidate. The optional population carries only
// the insertion readings and must match the original definition/support.
[[nodiscard]] inline std::optional<NativeStereoSeed>
derive_native_stereo_seed(const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    std::size_t mode_index,bool stereo_input,bool exact_fixed_insertion_without_side_eq,
    const std::vector<m08_frozen::AuthenticatedPairSlot>* exact_post_side=nullptr)
{
    if(!stereo_input||!exact_fixed_insertion_without_side_eq||mode_index==0U||
       mode_index>kLowBassAlternatives.size())return {};
    constexpr auto domain=static_cast<std::size_t>(m07_frozen::Domain::stereo);
    const auto targets=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    if(!targets.valid_current||!targets.corrective[domain])return {};
    const auto& insertion=exact_post_side?*exact_post_side:fixed.measured_fixed;
    const auto protection=m11_frozen::bind_native_fixed_to_candidate(fixed,insertion,amount);
    if(!protection)return {};
    const auto& low=kLowBassAlternatives[mode_index-1U];
    std::optional<NativeWidthTarget> broad,bass;
    std::size_t offset=0;
    for(std::size_t i=0;i<insertion.size();++i) {
        const auto& slot=insertion[i];
        const auto& binding=m07_frozen::kBindings[i];
        if(offset+slot.configured.size()>protection->size())return {};
        if(slot.binding_id=="rgsml.reference-match.binding.stereo.s01"&&slot.configured.size()==1U)
            broad=native_width_target(slot.configured.front(),(*protection)[offset],amount,binding.deadband);
        if(low.active()&&slot.binding_id=="rgsml.reference-match.binding.stereo.s05"&&slot.configured.size()==3U) {
            const auto index=low.cutoff_hz==80U?0U:low.cutoff_hz==120U?1U:2U;
            const auto& part=slot.configured[index];
            if(part.candidate_native.selector!=std::to_string(low.cutoff_hz))return {};
            bass=native_width_target(part,(*protection)[offset+index],amount,binding.deadband);
        }
        offset+=slot.configured.size();
    }
    if(!broad||(low.active()&&!bass))return {};
    double raw_mid=0.0,raw_side=0.0;
    bool mute=false;
    if(broad->needs_action) {
        if(broad->fixed_mono&&!broad->target_mono)return {};
        if(broad->target_mono) {
            if(low.active())return {}; // separate global Side-mute alternative
            mute=true;
        } else {
            const double delta=20.0*std::log10(2.0)*(broad->desired_z-broad->fixed_z);
            raw_mid=-delta/2.0;raw_side=delta/2.0;
        }
    }
    if(!std::isfinite(raw_mid)||!std::isfinite(raw_side))return {};
    const double mid=std::clamp(raw_mid,-12.0,12.0);
    const double side=std::clamp(raw_side,-3.0,1.5);
    bool limited=mid!=raw_mid||side!=raw_side;
    double width=100.0;
    std::optional<double> raw_width;
    if(low.active()) {
        if(!bass->needs_action||bass->fixed_mono||
           (!bass->target_mono&&bass->desired_z>=bass->fixed_z))return {};
        if(bass->target_mono) {raw_width=0.0;width=0.0;}
        else {
            const auto seed=derive_low_width_seed(
                {true,true,false,true,false,false,bass->fixed_z,bass->desired_z,mid,side,true},
                broad->needs_action);
            if(!seed.raw_percent||!seed.legal_percent)return {};
            raw_width=seed.raw_percent;width=*seed.legal_percent;
            limited=limited||seed.reason==LowSeedReason::limited_to_legal_range;
        }
    } else if(!broad->needs_action)return {};
    const auto mode=low.mode==LowBassMode::off?dsp::MonoBassMode::OFF:
        low.mode==LowBassMode::lr12?dsp::MonoBassMode::LR12:dsp::MonoBassMode::LR24;
    auto payload=dsp::StereoMsParameters::create(mid,side,mute,mode,low.cutoff_hz,width);
    if(!payload)return {};
    return NativeStereoSeed{*payload.value(),raw_mid,raw_side,raw_width,limited};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
