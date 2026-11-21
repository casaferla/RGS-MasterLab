#pragma once
// L3 Product DSP bridge, initial GAIN candidate only. No DSP mutation,
// alternate renderer, Qt call, audio callback or new parameter arithmetic.
// Product owner must separately authenticate the source PREPARED PCM,
// source/project epoch and finished M21 results before accepting any trial.
#include "l3_automatch_offline_search.hpp"
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace rgsml::app::l3_automatch {

struct BoundGainTrial final {
    std::string ticket_identity;
    dsp::ModuleInstanceId gain_id;
    std::uint64_t baseline_chain_revision{};
    std::vector<dsp::ModuleInstance> baseline_instances;
    std::vector<dsp::ModuleExecutionBinding> baseline_bindings;
    std::vector<dsp::ModuleExecutionBinding> candidate_bindings;
    double gain_db{};

    // Recheck immediately before any offline render or real Product mutation.
    // This proves the DSP source graph, NOT source WAV/Qt/M18 owner authority.
    [[nodiscard]] bool still_matches(
        const dsp::ProcessingChain& current,
        std::span<const dsp::ModuleExecutionBinding> live) const
    {
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

[[nodiscard]] inline bool binding_matches_instance(
    const dsp::ModuleInstance& instance,
    const dsp::ModuleExecutionBinding& binding)
{
    if(instance.instance_id()!=binding.instance_id)return false;
    const auto type=instance.module_type_id();
    const auto& payload=binding.parameters;
    return (type=="rgsml.dsp.gain"
                &&std::holds_alternative<dsp::GainParameters>(payload))
        || (type=="rgsml.dsp.parametric-eq"
                &&std::holds_alternative<dsp::ParametricEqParameters>(payload))
        || (type=="rgsml.dsp.compressor"
                &&std::holds_alternative<dsp::CompressorParameters>(payload))
        || (type=="rgsml.dsp.stereo-ms"
                &&std::holds_alternative<dsp::StereoMsParameters>(payload))
        || (type=="rgsml.dsp.streaming-true-peak-limiter"
                &&std::holds_alternative<dsp::StreamingTruePeakLimiterParameters>(payload));
}

// Build an immutable full-graph copy with ONE genuine GainParameters change.
// No bypass changes, no fabricated ModuleInstance, and the registry/factory
// bound to ProcessingChain remain the same for the existing RenderPreview.
[[nodiscard]] inline std::optional<BoundGainTrial> make_gain_trial(
    const dsp::ProcessingChain& chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleInstanceId& gain_id,
    double trial_db,std::string ticket_identity)
{
    if(ticket_identity.empty() ||ticket_identity.size()>128
       ||chain.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL})
        return std::nullopt;
    const auto validated=dsp::GainParameters::create(trial_db);
    if(!validated)return std::nullopt;
    const auto instances=chain.instances();
    if(instances.empty() ||instances.size()>5
       ||instances.size()!=bindings.size())
        return std::nullopt;
    std::optional<std::size_t> gainIndex;
    for(std::size_t i=0;i<instances.size();++i) {
        const auto& inst=instances[i];
        if(!binding_matches_instance(inst,bindings[i]))
            return std::nullopt;
        if(inst.instance_id()==gain_id) {
            if(gainIndex ||inst.module_type_id()!="rgsml.dsp.gain"
               ||!inst.active())
                return std::nullopt;
            gainIndex=i;
        }
    }
    if(!gainIndex)return std::nullopt;
    const auto* prior=std::get_if<dsp::GainParameters>(
        &bindings[*gainIndex].parameters);
    if(!prior ||*prior==*validated.value())return std::nullopt;
    std::vector<dsp::ModuleExecutionBinding> original(
        bindings.begin(),bindings.end());
    auto candidate=original;
    candidate[*gainIndex].parameters=*validated.value();
    return BoundGainTrial{
        std::move(ticket_identity),gain_id,chain.revision(),
        {instances.begin(),instances.end()},
        std::move(original),std::move(candidate),
        validated.value()->gain_db()
    };
}

} // namespace rgsml::app::l3_automatch
