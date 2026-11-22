#pragma once
// M10 exact P2 optimizer winner -> M12 nonpublishing accepted-owner guard.
// This is NOT a persisted Project receipt: the real Product owner must
// still materialize the accepted plan and authorize an M13 fresh render.
// P2 is the final artistic/streaming DSP, not M17 WAV Export.
#include "l3_m12_ref_linked_native_stage.hpp"
#include "l3_m10_p2_canonical_coordinate_port.hpp"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rgsml::app::l3_automatch::m12_frozen {
struct StagedExactP2Winner final {
    dsp::ModuleInstanceId exact_p2_instance_id;
    dsp::StreamingTruePeakLimiterParameters winning_parameters;
    core::Uuid exact_plan_id;
    std::string exact_m10_winner_topology_checksum;
    std::string exact_source_reference_scope_revision;
    std::string exact_ticket_fingerprint;
    std::uint64_t frozen_manual_revision{};
    std::vector<dsp::ModuleInstance> frozen_manual_nodes;
    std::vector<dsp::ModuleExecutionBinding> frozen_manual_bindings;
    [[nodiscard]] bool matches(
        const StagedRefLinkedSegment& stage,
        const dsp::ProcessingChain& current_manual,
        std::span<const dsp::ModuleExecutionBinding> current_bindings)
        const noexcept
    {
        const auto nodes=current_manual.instances();
        if(stage.accepted_identity.plan_id!=exact_plan_id||
           stage.accepted_identity.exact_m10_result_and_topology_checksum!=
               exact_m10_winner_topology_checksum||
           stage.accepted_identity.exact_source_reference_amount_scope_revision!=
               exact_source_reference_scope_revision||
           current_manual.revision()!=frozen_manual_revision||
           stage.frozen_protected_manual_revision!=frozen_manual_revision||
           nodes.size()!=frozen_manual_nodes.size()||
           stage.frozen_protected_manual_instances!=frozen_manual_nodes||
           stage.frozen_protected_manual_bindings!=frozen_manual_bindings||
           current_bindings.size()!=frozen_manual_bindings.size()||
           !std::equal(nodes.begin(),nodes.end(),frozen_manual_nodes.begin())||
           !std::equal(current_bindings.begin(),current_bindings.end(),
                       frozen_manual_bindings.begin())||
           nodes.empty()||
           nodes.back().instance_id()!=exact_p2_instance_id||
           !nodes.back().active()||
           nodes.back().module_type_id()!=
               "rgsml.dsp.streaming-true-peak-limiter")
            return false;
        const auto* previous=
            std::get_if<dsp::StreamingTruePeakLimiterParameters>(
                &current_bindings.back().parameters);
        return previous &&
            previous->margin_percent()==winning_parameters.margin_percent();
    }
    [[nodiscard]] constexpr bool authorizes_project_commit()
        const noexcept{return false;}
    [[nodiscard]] constexpr bool is_m13_verified()
        const noexcept{return false;}
};
[[nodiscard]] inline std::optional<StagedExactP2Winner>
preflight_exact_m12_p2_optimizing_winner(
    const StagedRefLinkedSegment& staged_linked,
    const dsp::ProcessingChain& current_protected_manual,
    std::span<const dsp::ModuleExecutionBinding> current_manual_bindings,
    const m10_frozen::BoundM10P2Candidate& exact_m10_p2_candidate,
    std::string_view exact_current_m10_winning_ticket,
    std::string_view exact_current_m10_result_topology_checksum,
    bool actual_product_m10_m11_winner_owner_and_scope_attested)
{
    const auto& accepted=staged_linked.accepted_identity;
    const auto& bound=exact_m10_p2_candidate.typed;
    const auto* p2=bound.chosen_p2_parameters();
    if(!actual_product_m10_m11_winner_owner_and_scope_attested||
       !accepted.current_source_reference_owner_and_plan_attested||
       !accepted.m11_protected_and_full_candidate_safety_passed||
       !accepted.independent_m16_measurement_and_m11_safety_attested||
       accepted.plan_id.is_nil()||
       !staged_linked.registry_owner||
       exact_current_m10_winning_ticket.empty()||
       exact_current_m10_result_topology_checksum.empty()||
       exact_current_m10_winning_ticket!=bound.base.ticket_identity||
       exact_current_m10_result_topology_checksum!=
           accepted.exact_m10_result_and_topology_checksum||
       !bound.still_matches(
           current_protected_manual,current_manual_bindings)||
       current_protected_manual.revision()!=
           staged_linked.frozen_protected_manual_revision||
       !p2||current_protected_manual.instances().empty())
        return std::nullopt;
    for(const auto& node:staged_linked.chain.instances())
        if(node.module_type_id()==
            "rgsml.dsp.streaming-true-peak-limiter")
            return std::nullopt; // no second linked P2 processor
    StagedExactP2Winner result{
        bound.base.p2_id,*p2,accepted.plan_id,
        accepted.exact_m10_result_and_topology_checksum,
        accepted.exact_source_reference_amount_scope_revision,
        bound.base.ticket_identity,
        current_protected_manual.revision(),
        {current_protected_manual.instances().begin(),
         current_protected_manual.instances().end()},
        {current_manual_bindings.begin(),current_manual_bindings.end()}};
    if(!result.matches(
         staged_linked,current_protected_manual,current_manual_bindings))
        return std::nullopt;
    return result;
}
} // namespace rgsml::app::l3_automatch::m12_frozen
