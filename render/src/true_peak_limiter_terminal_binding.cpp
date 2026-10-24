#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/module_parameter_payload.hpp>

#include <string_view>
#include <utility>
#include <variant>

namespace rgsml::render {
namespace {

[[nodiscard]] core::Error bind_error(const char* category, const char* reason)
{
    return core::Error{core::ErrorCode::InvalidArgument, reason,
                       {{"category",category}}};
}
} // namespace

core::Result<TruePeakLimiterBoundTerminalCandidate>
render_true_peak_limiter_bound_terminal_candidate(
    audio::AudioBufferView input,
    const dsp::ProcessingChain& chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleRegistry& registry,
    core::FrameCount block_size,
    const TerminalRenderCancel& cancelled,
    TerminalRenderStageTimings* stage_timings,
    bool use_experimental_offline_accelerator,
    std::optional<TruePeakLimiterMeasuredNonconformingCandidate>* nonconforming_out)
{
    if(nonconforming_out)nonconforming_out->reset();
    // Mandatory exact terminal placement. A MANUAL Gain/EQ/Compressor/M15
    // chain can never masquerade as the last OUTPUT_RATE DSP segment.
    if (chain.context().stage != dsp::ProcessingStage::MASTER
        || chain.context().segment != dsp::ChainSegment::TERMINAL) {
        return core::Result<TruePeakLimiterBoundTerminalCandidate>::failure(
            bind_error("M17_TERMINAL_CHAIN_CONTEXT_INVALID",
                "True Peak Limiter must execute in the MASTER TERMINAL segment."));
    }
    const auto modules = chain.instances();
    if (modules.size()!=1
        || modules[0].module_type_id() != std::string_view{
            "rgsml.dsp.true-peak-limiter"}) {
        return core::Result<TruePeakLimiterBoundTerminalCandidate>::failure(
            bind_error("M17_TERMINAL_CHAIN_SHAPE_INVALID",
                "Expected exactly one real True Peak Limiter terminal instance."));
    }
    const auto& instance=modules[0];
    // No default Ceiling or phantom bypass fallback. Even BYP carries the
    // real parameter ownership and identity, and rejects a stale binding.
    if (bindings.size()!=1
        || bindings[0].instance_id!=instance.instance_id()) {
        return core::Result<TruePeakLimiterBoundTerminalCandidate>::failure(
            bind_error("M17_TERMINAL_TYPED_BINDING_REQUIRED",
                "One exact instance-matched True Peak Limiter payload is mandatory."));
    }
    const auto* params = std::get_if<dsp::TruePeakLimiterParameters>(
        &bindings[0].parameters);
    if (params==nullptr) {
        return core::Result<TruePeakLimiterBoundTerminalCandidate>::failure(
            bind_error("M17_TERMINAL_TYPED_CEILING_MISMATCH",
                "True Peak Limiter requires immutable TruePeakLimiterParameters."));
    }
    const auto bypass = !instance.active();
    auto checked = render_true_peak_limiter_checked_candidate(
        input, *params, registry, block_size, bypass, cancelled,
        stage_timings,use_experimental_offline_accelerator,nonconforming_out);
    if (!checked) {
        return core::Result<TruePeakLimiterBoundTerminalCandidate>::failure(
            *checked.error());
    }
    return core::Result<TruePeakLimiterBoundTerminalCandidate>::success({
        std::move(*checked.value()), instance.instance_id(),chain.revision()});
}
} // namespace rgsml::render
