#pragma once
// Read-only diagnostic for independent M17 WAV Export terminal setup.
// NOT a precondition for AutoMatch M12 materialization or M13 M16
// verification; WAV Export ceiling is independent of P2/AutoMatch.
// This legacy diagnostic cannot grant product export authority.
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

namespace rgsml::app::l3_automatch::m12_frozen {
enum class TerminalNativeReason:std::uint8_t {
    pass,stale_or_unverified_owner,wrong_terminal_context,
    missing_or_multiple_terminal_true_peak,
    bypassed_or_suspended_terminal_safety,
    incompatible_terminal_ceiling,
    unprotected_or_ambiguous_terminal_owner
};
struct TerminalNativeVerdict final {
    TerminalNativeReason reason{
        TerminalNativeReason::stale_or_unverified_owner};
    std::optional<dsp::ModuleInstanceId> valid_existing_limiter;
    [[nodiscard]] constexpr bool admissible_without_new_terminal_action()
        const noexcept{return reason==TerminalNativeReason::pass;}
    [[nodiscard]] constexpr bool authorizes_master_export()
        const noexcept{return false;}
};
[[nodiscard]] inline TerminalNativeVerdict
audit_independent_m17_wav_export_only(
    const dsp::ProcessingChain& actual_terminal,
    std::span<const dsp::ModuleExecutionBinding> existing_terminal_bindings,
    double frozen_accepted_ceiling_db_tp,
    bool exact_current_product_owner_and_final_safety_approved)
{
    if(!exact_current_product_owner_and_final_safety_approved||
       !std::isfinite(frozen_accepted_ceiling_db_tp))
        return {};
    if(actual_terminal.context()!=dsp::ProcessingChainContext{
            dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL})
        return {TerminalNativeReason::wrong_terminal_context,{}};
    std::size_t count=0U;
    const dsp::ModuleInstance* truePeak=nullptr;
    for(const auto& instance:actual_terminal.instances()) {
        const auto type=instance.module_type_id();
        if(type=="rgsml.dsp.true-peak-limiter"||
           type=="rgsml.dsp.streaming-true-peak-limiter") {
            ++count;
            if(type=="rgsml.dsp.true-peak-limiter")
                truePeak=&instance;
        }
    }
    if(count!=1U||!truePeak)
        return {TerminalNativeReason::
            missing_or_multiple_terminal_true_peak,{}};
    if(!truePeak->active())
        return {TerminalNativeReason::
            bypassed_or_suspended_terminal_safety,{}};
    // Existing protected terminal is NEVER silently adopted as
    // controller-owned REF; user bypass/state remain independent.
    if(truePeak->provenance()!=dsp::ModuleProvenance::MANUAL||
       truePeak->owner()!=dsp::ModuleOwner::USER||
       truePeak->link_state()!=dsp::ModuleLinkState::UNLINKED)
        return {TerminalNativeReason::
            unprotected_or_ambiguous_terminal_owner,{}};
    std::size_t matched=0U;
    for(const auto& binding:existing_terminal_bindings) {
        if(binding.instance_id!=truePeak->instance_id())continue;
        ++matched;
        const auto* p=std::get_if<dsp::TruePeakLimiterParameters>(
            &binding.parameters);
        if(!p||p->ceiling_db_tp()!=frozen_accepted_ceiling_db_tp)
            return {TerminalNativeReason::
                incompatible_terminal_ceiling,{}};
    }
    if(matched!=1U)return {
        TerminalNativeReason::unprotected_or_ambiguous_terminal_owner,{}};
    return {TerminalNativeReason::pass,truePeak->instance_id()};
}
} // namespace rgsml::app::l3_automatch::m12_frozen
