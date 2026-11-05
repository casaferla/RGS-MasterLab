#pragma once
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <vector>
#include <cstdint>
namespace rgsml::app {
// Candidate product state: an authentic MASTER TERMINAL segment, separate
// from the M11/M12/M14/M15 MANUAL processing chain. No audition/export
// or final product qualification is inferred from this class alone.
class TerminalLimiterState final {
public:
    [[nodiscard]] static core::Result<TerminalLimiterState> create_default(
        const dsp::ModuleRegistry& registry, dsp::ModuleInstanceId id);
    [[nodiscard]] static core::Result<TerminalLimiterState> create(
        const dsp::ModuleRegistry& registry, dsp::ModuleInstanceId id,
        dsp::TruePeakLimiterParameters params, bool bypassed);
    [[nodiscard]] const dsp::ProcessingChain& chain() const noexcept
    { return chain_; }
    [[nodiscard]] const dsp::ModuleInstanceId& instance_id() const noexcept
    { return id_; }
    [[nodiscard]] const dsp::TruePeakLimiterParameters& parameters() const noexcept
    { return params_; }
    [[nodiscard]] std::uint64_t parameter_revision() const noexcept
    { return parameterRevision_; }
    [[nodiscard]] bool bypassed() const noexcept
    { return chain_.instances()[0].user_bypass(); }
    [[nodiscard]] core::Status set_ceiling_db_tp(double value);
    [[nodiscard]] core::Status set_user_bypass(bool bypass);
    [[nodiscard]] std::vector<dsp::ModuleExecutionBinding> execution_bindings() const;
private:
    TerminalLimiterState(dsp::ProcessingChain chain, dsp::ModuleInstanceId id,
                         dsp::TruePeakLimiterParameters params) noexcept;
    dsp::ProcessingChain chain_;
    dsp::ModuleInstanceId id_;
    dsp::TruePeakLimiterParameters params_;
    // Ceiling is not a ProcessingChain topology edit. Track it separately
    // to reject stale renderer completions, including A->B->A changes.
    std::uint64_t parameterRevision_{0};
};
} // namespace rgsml::app
