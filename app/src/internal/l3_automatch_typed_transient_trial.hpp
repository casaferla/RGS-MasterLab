#pragma once
// L3 Transient Shaper: exactly one frozen scalar amount change on an
// existing, active MANUAL module; no new topology or automatic policy.
#include "l3_automatch_typed_gain_trial.hpp"
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
enum class TransientCoordinate : std::uint8_t {
    attack_amount_db,
    sustain_amount_db
};
struct BoundTransientTrial final {
    std::string ticket_identity;
    dsp::ModuleInstanceId module_id;
    TransientCoordinate coordinate{};
    double candidate_value{};
    std::uint64_t baseline_chain_revision{};
    std::vector<dsp::ModuleInstance> baseline_instances;
    std::vector<dsp::ModuleExecutionBinding> baseline_bindings;
    std::vector<dsp::ModuleExecutionBinding> candidate_bindings;
    [[nodiscard]] bool still_matches(
        const dsp::ProcessingChain& current,
        std::span<const dsp::ModuleExecutionBinding> live) const {
        const auto instances=current.instances();
        return current.revision()==baseline_chain_revision
            &&instances.size()==baseline_instances.size()
            &&std::equal(instances.begin(),instances.end(),
                          baseline_instances.begin())
            &&live.size()==baseline_bindings.size()
            &&std::equal(live.begin(),live.end(),
                          baseline_bindings.begin());
    }
    [[nodiscard]] bool matches_ticket(const Ticket& ticket) const {
        return ticket.typed_dsp_fingerprint==ticket_identity;
    }
};

[[nodiscard]] inline std::optional<BoundTransientTrial>
make_transient_scalar_trial(
    const dsp::ProcessingChain& chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleInstanceId& module_id,
    TransientCoordinate coordinate,double value,
    std::string ticket_identity)
{
    if(ticket_identity.empty()||ticket_identity.size()>128
       ||!std::isfinite(value)
       ||chain.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL})
        return std::nullopt;
    const auto instances=chain.instances();
    if(instances.empty()||instances.size()!=bindings.size())
        return std::nullopt;
    std::optional<std::size_t> at;
    for(std::size_t i=0;i<instances.size();++i) {
        if(!binding_matches_instance(instances[i],bindings[i]))
            return std::nullopt;
        if(instances[i].instance_id()==module_id) {
            if(at||!instances[i].active()
               ||instances[i].module_type_id()!="rgsml.dsp.transient-shaper")
                return std::nullopt;
            at=i;
        }
    }
    if(!at)return std::nullopt;
    const auto* before=std::get_if<dsp::TransientShaperParameters>(
        &bindings[*at].parameters);
    if(!before)return std::nullopt;
    double a=before->attack_amount_db(),s=before->sustain_amount_db();
    switch(coordinate) {
    case TransientCoordinate::attack_amount_db:a=value;break;
    case TransientCoordinate::sustain_amount_db:s=value;break;
    default:return std::nullopt;
    }
    const auto changed=dsp::TransientShaperParameters::create(a,s);
    if(!changed||*changed.value()==*before)
        return std::nullopt;
    std::vector<dsp::ModuleExecutionBinding> baseline(
        bindings.begin(),bindings.end());
    auto candidate=baseline;
    candidate[*at].parameters=*changed.value();
    return BoundTransientTrial{
        std::move(ticket_identity),module_id,coordinate,value,
        chain.revision(),{instances.begin(),instances.end()},
        std::move(baseline),std::move(candidate)};
}
} // namespace rgsml::app::l3_automatch
