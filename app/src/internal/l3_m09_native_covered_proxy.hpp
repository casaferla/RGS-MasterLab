#pragma once
#include "l3_m09_native_family_exact_content.hpp"
#include "l3_m09_pre_render_proxy.hpp"

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeComponentCoverage final {
    std::string exact_binding_id;
    std::string exact_feature_dependency;
    std::vector<bool> covered;
};
// Coverage is an analytical capability statement, not an observed
// candidate value. Keep every configured component and its original
// support, including covered residual mass. Never mark covered as
// protected: that would change the M10 residual denominator.
[[nodiscard]] inline std::optional<double> native_covered_target_loss(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    std::span<const NativeComponentCoverage> coverage)
{
    const auto assessed=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    if(!assessed.valid_current||coverage.size()!=fixed.measured_fixed.size())return {};
    auto pop=m08_frozen::build_targeted_population(amount,fixed.measured_fixed,assessed.corrective);
    const auto guards=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!pop.value||!guards)return {};
    std::size_t offset=0;
    std::array<std::size_t,4> domain_offsets{};
    for(std::size_t i=0;i<coverage.size();++i) {
        const auto& c=coverage[i];const auto& slot=fixed.measured_fixed[i];
        const auto d=static_cast<std::size_t>(m07_frozen::kBindings[i].domain);
        if(c.exact_binding_id!=slot.binding_id||c.exact_feature_dependency!=slot.exact_feature_dependency||
           c.covered.size()!=slot.configured.size())return {};
        for(std::size_t k=0;k<c.covered.size();++k) {
            const auto& guard=(*guards)[offset++];
            auto& objective=pop.value->objective_input[d].configured[domain_offsets[d]++];
            objective.protected_from_correction=guard.protected_by_m11_mask;
            if(!c.covered[k])continue;
            if(!assessed.corrective[d]||!guard.required_in_frozen_k||
               !guard.exact_current||guard.protected_by_m11_mask||
               !objective.optimization_target_z)return {};
            objective.candidate_z=objective.optimization_target_z;
        }
    }
    if(offset!=guards->size())return {};
    const auto loss=m10_frozen::compute_frozen_target_cost(pop.value->objective_input);
    if(!loss.valid_measured())return {};
    return loss.j_target;
}
[[nodiscard]] inline PreRenderTopologyProxy native_covered_proxy(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    const NativeTypedSeed& typed,std::span<const NativeComponentCoverage> coverage,
    const ExactBytes& topology)
{
    ProxyOperands operands;
    operands.canonical_unsigned_topology_key=topology;
    const auto inventory=native_seed_inventory(typed);
    const auto complexity=native_artistic_complexity(typed.seed);
    if(!inventory||!complexity)return make_pre_render_proxy(operands);
    operands.active_owned_modules=complexity->modules;
    operands.enabled_artistic_bands=complexity->bands;
    std::vector<m10_frozen::ActiveReferenceCoordinate> coordinates;
    for(const auto& c:typed.coordinates)
        coordinates.push_back({c.canonical.normalized_u,c.immutable_reference_u,c.reference_weight});
    operands.frozen_covered_target_loss=native_covered_target_loss(fixed,amount,requested,coverage);
    operands.m10_seed_normalized_parameter_displacement=m10_frozen::normalized_reference_distance(coordinates);
    operands.m10_topology_complexity=m10_frozen::complexity_cost(complexity->modules,complexity->bands);
    operands.exact_m05_m08_and_m10_operands_attested=true;
    return make_pre_render_proxy(operands);
}
// The first complete capability adapter covers only L01 with a
// directionally compatible uniform Gain. Neither L02 nor L03 is
// inferred from this knob; other roles need their own evidence map.
[[nodiscard]] inline std::optional<std::vector<NativeComponentCoverage>>
native_gain_coverage(const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    const NativeTypedSeed& typed)
{
    const auto assessment=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    const auto guards=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!assessment.valid_current||!guards||!native_seed_inventory(typed))return {};
    const auto nodes=typed.seed.linked.instances();
    const dsp::GainParameters* gain=nullptr;
    for(std::size_t i=0;i<nodes.size();++i) {
        if(const auto* p=std::get_if<dsp::GainParameters>(&typed.seed.bindings[i].parameters)) {
            if(gain||!nodes[i].active())return {};
            gain=p;
        } else return {}; // only the NONE / Gain topology is attested here
    }
    std::vector<NativeComponentCoverage> out;
    std::size_t offset=0;
    for(const auto& slot:fixed.measured_fixed) {
        NativeComponentCoverage c{slot.binding_id,slot.exact_feature_dependency,
            std::vector<bool>(slot.configured.size(),false)};
        if(gain&&assessment.corrective[3]&&slot.binding_id=="rgsml.reference-match.binding.loudness.l01") {
            if(slot.configured.size()!=1U)return {};
            const auto& p=slot.configured.front();
            const auto& guard=(*guards)[offset];
            if(guard.required_in_frozen_k&&!guard.protected_by_m11_mask&&guard.exact_current) {
                const auto target=m08_frozen::desired_for_component(amount,3,p.prepared,p.reference);
                if(!target.value||!p.current_candidate_z)return {};
                const double delta=target.value->constraint_adjusted_z.value_or(target.value->desired_z)-*p.current_candidate_z;
                c.covered[0]=(delta>0.0&&gain->gain_db()>0.0)||(delta<0.0&&gain->gain_db()<0.0);
            }
        }
        offset+=slot.configured.size();out.push_back(std::move(c));
    }
    return out;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
