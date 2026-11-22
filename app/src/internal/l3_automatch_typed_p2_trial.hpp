#pragma once
// L3 P2 typed candidate adapter. This bridges an externally authorized
// M10 scalar proposal to the existing Product streaming limiter binding.
// No fabricated score, no implicit M17 WAV Export or source mutation.
// Current manual owner and exact full DSP graph MUST be rechecked by
// the Product worker immediately before any real candidate render.
#include "l3_automatch_typed_gain_trial.hpp"
#include <rgsml/dsp/streaming_true_peak_limiter_parameters.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
enum class P2ScalarCoordinate : std::uint8_t {
    ceiling_db_tp, drive_db
};
struct BoundP2Trial final {
    // Not a minted canonical identity. Product owner supplies an exact M10
    // ticket fingerprint covering source, reference, full chain and policy.
    std::string ticket_identity;
    dsp::ModuleInstanceId p2_id;
    P2ScalarCoordinate coordinate{};
    double candidate_value{};
    std::uint64_t baseline_chain_revision{};
    std::vector<dsp::ModuleInstance> baseline_instances;
    std::vector<dsp::ModuleExecutionBinding> baseline_bindings;
    std::vector<dsp::ModuleExecutionBinding> candidate_bindings;

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
    [[nodiscard]] const dsp::StreamingTruePeakLimiterParameters*
    chosen_p2_parameters() const noexcept
    {
        if(candidate_bindings.empty())return nullptr;
        return std::get_if<dsp::StreamingTruePeakLimiterParameters>(
            &candidate_bindings.back().parameters);
    }
};
[[nodiscard]] inline std::optional<BoundP2Trial> make_p2_scalar_trial(
    const dsp::ProcessingChain& chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleInstanceId& p2_id,
    P2ScalarCoordinate coordinate,
    double proposed_value,std::string ticket_identity,
    bool allow_identity_candidate=false)
{
    if(ticket_identity.empty()||ticket_identity.size()>128U
       ||chain.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL})
        return std::nullopt;
    const auto nodes=chain.instances();
    if(nodes.empty()||nodes.size()!=bindings.size())
        return std::nullopt;
    // P2 must be unique, active, and LAST. M13 preserves the full
    // protected MANUAL prefix, then applies REF_LINKED and P2 as a
    // separately-rendered musical suffix. Never move a manual node here.
    std::size_t target_count=0;
    for(std::size_t i=0;i<nodes.size();++i) {
        if(!binding_matches_instance(nodes[i],bindings[i]))
            return std::nullopt;
        if(nodes[i].module_type_id()=="rgsml.dsp.streaming-true-peak-limiter") {
            ++target_count;
            if(i!=nodes.size()-1U ||nodes[i].instance_id()!=p2_id
               ||!nodes[i].active()||
               nodes[i].owner()!=dsp::ModuleOwner::USER||
               nodes[i].provenance()!=dsp::ModuleProvenance::MANUAL||
               nodes[i].link_state()!=dsp::ModuleLinkState::UNLINKED)
                return std::nullopt;
        }
    }
    if(target_count!=1U)return std::nullopt;
    const auto* original=
        std::get_if<dsp::StreamingTruePeakLimiterParameters>(
            &bindings.back().parameters);
    if(!original)return std::nullopt;
    double ceiling=original->ceiling_db_tp();
    double drive=original->drive_db();
    switch(coordinate) {
    case P2ScalarCoordinate::ceiling_db_tp:
        ceiling=proposed_value;break;
    case P2ScalarCoordinate::drive_db:
        drive=proposed_value;break;
    default:
        return std::nullopt;
    }
    // Margin remains the independent user-configured 2/5 policy.
    const auto validated=dsp::StreamingTruePeakLimiterParameters::create(
        ceiling,drive,original->margin_percent());
    if(!validated||(*validated.value()==*original&&!allow_identity_candidate))
        return std::nullopt;
    std::vector<dsp::ModuleExecutionBinding> baseline(
        bindings.begin(),bindings.end());
    auto candidate=baseline;
    candidate.back().parameters=*validated.value();
    return BoundP2Trial{
        std::move(ticket_identity),p2_id,coordinate,proposed_value,
        chain.revision(),{nodes.begin(),nodes.end()},
        std::move(baseline),std::move(candidate)};
}

// A full M10 candidate may move BOTH coordinates while the original
// immutable MANUAL snapshot stays fixed. Use the same validation as a
// single-coordinate trial, then build the complete authentic typed pair.
// This is one candidate with two independent frozen coordinates, not a
// new solver step-size policy or two artificial measured evaluations.
struct BoundP2JointTrial final {
    BoundP2Trial base;
    double chosen_ceiling_db_tp{};
    double chosen_drive_db{};
    [[nodiscard]] bool still_matches(
        const dsp::ProcessingChain& chain,
        std::span<const dsp::ModuleExecutionBinding> live) const
    {
        return base.still_matches(chain,live);
    }
    [[nodiscard]] bool matches_ticket(const Ticket& ticket) const {
        return base.matches_ticket(ticket);
    }
    [[nodiscard]] const dsp::StreamingTruePeakLimiterParameters*
    chosen_p2_parameters() const noexcept
    {
        return base.chosen_p2_parameters();
    }
};
[[nodiscard]] inline std::optional<BoundP2JointTrial>
make_p2_joint_trial(
    const dsp::ProcessingChain& chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleInstanceId& p2_id,
    double chosen_ceiling_db_tp,double chosen_drive_db,
    std::string ticket_identity,
    bool allow_identity_candidate=false)
{
    if(bindings.empty())return std::nullopt;
    const auto* previous=
        std::get_if<dsp::StreamingTruePeakLimiterParameters>(
            &bindings.back().parameters);
    if(!previous)return std::nullopt;
    const auto both=dsp::StreamingTruePeakLimiterParameters::create(
        chosen_ceiling_db_tp,chosen_drive_db,previous->margin_percent());
    if(!both||(*both.value()==*previous&&!allow_identity_candidate))
        return std::nullopt;
    // The first non-identity coordinate chooses a real Product/owner
    // baseline and exact graph check, not a synthetic candidate shortcut.
    const auto coordinate=
        chosen_ceiling_db_tp==previous->ceiling_db_tp()
        ?P2ScalarCoordinate::drive_db
        :P2ScalarCoordinate::ceiling_db_tp;
    const double first_value=
        coordinate==P2ScalarCoordinate::drive_db
        ?chosen_drive_db:chosen_ceiling_db_tp;
    auto single=make_p2_scalar_trial(
        chain,bindings,p2_id,coordinate,first_value,
        std::move(ticket_identity),allow_identity_candidate);
    if(!single)return std::nullopt;
    single->candidate_bindings.back().parameters=*both.value();
    return BoundP2JointTrial{
        std::move(*single),both.value()->ceiling_db_tp(),
        both.value()->drive_db()};
}
} // namespace rgsml::app::l3_automatch
