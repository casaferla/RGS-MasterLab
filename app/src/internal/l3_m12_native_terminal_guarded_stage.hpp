#pragma once
// L3 M12 actual Product accepted-plan staging requires independent
// fresh terminal M17 safety before REF_LINKED DSP construction.
// A boolean can never replace this real current terminal preflight.
#include "l3_m12_ref_linked_native_stage.hpp"
#include "l3_m12_terminal_native_hard_preflight.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m12_frozen {
[[nodiscard]] inline core::Result<StagedRefLinkedSegment>
stage_m12_with_real_current_terminal_composition(
    AcceptedPlanAndOwner accepted,
    const dsp::ProcessingChain& protected_manual,
    std::span<const dsp::ModuleExecutionBinding> protected_bindings,
    const dsp::ProcessingChain& current_real_terminal,
    std::span<const dsp::ModuleExecutionBinding> current_terminal_bindings,
    double accepted_exact_ceiling_db_tp,
    bool current_terminal_product_owner_and_m11_safety,
    std::shared_ptr<const dsp::ModuleRegistry> live_registry,
    std::vector<ExactLinkedNode> exact_winner_nodes)
{
    using R=core::Result<StagedRefLinkedSegment>;
    const auto terminal=verify_current_m12_terminal_limiter_composition(
        current_real_terminal,current_terminal_bindings,
        accepted_exact_ceiling_db_tp,
        current_terminal_product_owner_and_m11_safety);
    if(!terminal.admissible_without_new_terminal_action())
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "L3_M12_ACTUAL_TERMINAL_COMPOSITION_CONFLICT"});
    accepted.anchor_and_separate_terminal_limiter_admissible=true;
    return stage_exact_m12_native_ref_linked(
        accepted,protected_manual,protected_bindings,
        std::move(live_registry),std::move(exact_winner_nodes));
}
} // namespace rgsml::app::l3_automatch::m12_frozen
