#pragma once
#include "l3_m09_p02_micro_seed.hpp"
#include "l3_m09_abstract_family_enumeration.hpp"
#include "l3_m11_native_fixed_assessment.hpp"
#include <rgsml/dsp/transient_shaper_parameters.hpp>

namespace rgsml::app::l3_automatch::m09_frozen {
[[nodiscard]] inline MicroSeed derive_native_micro_seed(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,
    const std::array<bool,4>& requested,
    bool exact_event_context)
{
    constexpr auto domain=static_cast<std::size_t>(m07_frozen::Domain::dynamics);
    const auto targets=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    if(!targets.valid_current||!targets.corrective[domain]||!exact_event_context)return {};
    const auto protection=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!protection)return {};
    std::array<MicroTriplet,3> groups{};
    std::array<bool,3> found{};
    std::size_t offset=0;
    for(std::size_t i=0;i<fixed.measured_fixed.size();++i) {
        const auto& slot=fixed.measured_fixed[i];
        const auto& binding=m07_frozen::kBindings[i];
        std::optional<std::size_t> group;
        if(slot.binding_id=="rgsml.reference-match.binding.dynamics.d07")group=0U;
        if(slot.binding_id=="rgsml.reference-match.binding.dynamics.d08")group=1U;
        if(slot.binding_id=="rgsml.reference-match.binding.dynamics.d09")group=2U;
        if(group) {
            if(found[*group]||slot.configured.size()!=3U||offset+3U>protection->size())return {};
            found[*group]=true;
            const double mass=static_cast<double>(binding.domain_mass.numerator)/
                static_cast<double>(binding.domain_mass.denominator)/3.0;
            for(std::size_t j=0;j<3U;++j) {
                const auto& p=slot.configured[j];
                const auto& guard=(*protection)[offset+j];
                auto& out=groups[*group][j];
                out.requested=guard.required_in_frozen_k;
                out.protected_from_action=guard.protected_by_m11_mask;
                if(!out.requested||out.protected_from_action)continue;
                const auto desired=m08_frozen::desired_for_component(amount,domain,p.prepared,p.reference);
                if(!desired.value||!p.current_candidate_z)return {};
                out.exact_current=guard.exact_current;
                out.fixed_z=*p.current_candidate_z;
                out.optimization_z=desired.value->constraint_adjusted_z.value_or(desired.value->desired_z);
                out.frozen_deadband=binding.deadband;
                out.frozen_component_mass=mass;
                out.paired_support=p.paired_native_support;
            }
        }
        offset+=slot.configured.size();
    }
    for(bool yes:found)if(!yes)return {};
    return derive_micro_p02(groups[0],groups[1],groups[2],true,exact_event_context);
}
// Map frozen analytical variants onto the actual accepted C00 knobs.
[[nodiscard]] inline std::optional<dsp::TransientShaperParameters>
native_micro_variant_payload(const MicroSeed& seed,MicroMode mode)
{
    if(!seed.limited_db||(seed.reason!=MicroReason::ready&&
       seed.reason!=MicroReason::limited_to_seed_range)||
       !std::isfinite(*seed.limited_db)||std::abs(*seed.limited_db)>3.0)return {};
    const double d=*seed.limited_db;
    if(mode==MicroMode::none)return {};
    if(mode!=MicroMode::attack&&mode!=MicroMode::sustain&&mode!=MicroMode::balanced)return {};
    auto payload=dsp::TransientShaperParameters::create(
        mode==MicroMode::attack?d:mode==MicroMode::balanced?d/2.0:0.0,
        mode==MicroMode::sustain?-d:mode==MicroMode::balanced?-d/2.0:0.0);
    if(!payload)return {};
    return *payload.value();
}
} // namespace rgsml::app::l3_automatch::m09_frozen
