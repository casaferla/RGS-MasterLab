#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/true_peak_limiter_module.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <rgsml/render/render_result.hpp>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::render {

// UNQUALIFIED M17 terminal-render integration seam, intentionally separate
// from SOURCE_PROCESSING_RATE Render Preview. Caller must already own a full,
// contiguous OUTPUT_RATE resource. This low-level entrypoint does NOT
// itself confer SRC, export, verified trim, audible-realization routing
// or final product availability. The separate checked-candidate entrypoint
// performs M16 measurement and guarded trim but is still UNQUALIFIED.
struct TruePeakLimiterTerminalCandidate final {
    audio::AudioBuffer compensated_pcm;
    std::vector<dsp::TruePeakLimiterAppliedGainFrame> applied_gain_frames;
    std::int64_t algorithmic_latency_frames{0};
    bool bypassed{true};
};

// Process a COMPLETE single-resource OUTPUT_RATE programme exactly once.
// Active: stream genuine M17 DSP, drain exactly D at EOS, discard the first
// D raw placeholders, deliver N original-timeline PCM frames and N actual
// post-limiter applied-gain witnesses (excluding verification trim).
// BYP: bit-exact identity, no M17 DSP instantiation/GR/latency/trim.
// Fail closed on wrong domain, invalid PCM, missing factory, invalid timing,
// incomplete gain witness or any DSP error. Nothing is published on failure.
[[nodiscard]] core::Result<TruePeakLimiterTerminalCandidate>
render_true_peak_limiter_terminal_candidate(
    audio::AudioBufferView full_output_rate_source,
    dsp::TruePeakLimiterParameters parameters,
    const dsp::ModuleRegistry& registry,
    core::FrameCount maximum_block_frames,
    bool bypassed);

 // A measurement is NOT official qualification, irrespective of numeric peak.
 // It is attached only to an ACTIVE, final M17 output-rate candidate.
 struct TruePeakLimiterFinalCheck final {
     double before_linear_peak{0.0};
     double after_linear_peak{0.0};
     double global_trim_gain{1.0};
     bool trim_used{false};
     analysis::TruePeakQualificationStatus qualification_status{
         analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED};
 };

 struct TruePeakLimiterCheckedCandidate final {
     TruePeakLimiterTerminalCandidate terminal;
     // No measurement/trim at all for bypassed M17. Do not fabricate GR.
     std::optional<TruePeakLimiterFinalCheck> final_check;
 };

 // UNQUALIFIED, candidate-only complete realization path. Produces no
 // publishable PROCESSED/export audio: owner and UI integration gates remain.
 // Active requires REAL M16 full-resource final measurement and at most one
 // common stereo/mono verification trim <= 0.10dB; trim success requires an
 // independent M16 remeasurement. Failure never publishes staged PCM.
 // BYP keeps binary64 identity and has no meter/trim/GR/delay.
 [[nodiscard]] core::Result<TruePeakLimiterCheckedCandidate>
 render_true_peak_limiter_checked_candidate(
     audio::AudioBufferView full_output_rate_source,
     dsp::TruePeakLimiterParameters parameters,
     const dsp::ModuleRegistry& registry,
     core::FrameCount maximum_block_frames,
     bool bypassed);

 
// Tested but UNQUALIFIED bridge from the actual terminal-chain INSTANCE
// and immutable typed payload to the already separate OUTPUT_RATE renderer.
// This is not a Product processed-realization or export activation: it has
// no trusted source resource/realization ID, SRC or audible publication owner.
struct TruePeakLimiterBoundTerminalCandidate final {
    TruePeakLimiterCheckedCandidate checked;
    dsp::ModuleInstanceId limiter_instance_id;
    std::uint64_t terminal_chain_revision{0};
};

// Exactly one authentic terminal M17 instance and one exact typed parameter
// binding. Any absent, wrong-id, duplicate, wrong-type, or manual-chain input
// FAILS CLOSED before PCM processing, including when instance is bypassed.
// The bypass state comes exclusively from ProcessingChain, never a caller bool.
[[nodiscard]] core::Result<TruePeakLimiterBoundTerminalCandidate>
render_true_peak_limiter_bound_terminal_candidate(
    audio::AudioBufferView full_output_rate_source,
    const dsp::ProcessingChain& terminal_chain,
    std::span<const dsp::ModuleExecutionBinding> immutable_bindings,
    const dsp::ModuleRegistry& registry,
    core::FrameCount maximum_block_frames);


// First identity-preserving candidate handoff from a complete, OWNED,
// SOURCE_PROCESSING_RATE manual RenderResult to one contiguous OUTPUT_RATE
// resource. Only the proven 1:1 rate path is implemented. 44.1->48k
// (or ANY rate conversion) MUST FAIL CLOSED until a complete-resource SRC
// with exact source frame-map / Loop Region contracts exists.
// This does not publish PROCESSED audio or confer M17 qualification.
struct TruePeakLimiterOwnedSourceCandidate final {
    std::shared_ptr<const RenderResult> manual_owner;
    TruePeakLimiterBoundTerminalCandidate terminal;
    core::SampleRate output_rate;
    core::FrameCount programme_frames;
    std::uint64_t manual_chain_revision{0};
    bool used_candidate_src{false};
    std::string_view src_kernel_sha256;
};

// Caller must supply the currently accepted, immutable manual owner, real
// MASTER/MANUAL chain snapshot, and expected FULL length/revision. The function
// guards against stale, partial and mixed-domain PCM, copies every binary64
// bit into an owned OUTPUT_RATE buffer (only for EXACT same rate), invokes
// the REAL typed terminal DSP/verification and retains source lifetime.
// Still not a product realization: no playback identity or source resource
// authorization can be fabricated from this result.
[[nodiscard]] core::Result<TruePeakLimiterOwnedSourceCandidate>
render_true_peak_limiter_owned_same_rate_candidate(
    std::shared_ptr<const RenderResult> accepted_manual_owner,
    const dsp::ProcessingChain& manual_chain_snapshot,
    core::FrameCount original_full_resource_frames,
    std::uint64_t expected_manual_chain_revision,
    core::SampleRate required_output_rate,
    const dsp::ProcessingChain& terminal_chain,
    std::span<const dsp::ModuleExecutionBinding> immutable_terminal_bindings,
    const dsp::ModuleRegistry& registry,
    core::FrameCount maximum_block_frames);

// Experimental FULL-rate path: real complete SOURCE_RATE owner -> frozen
// 44.1<->48 kHz FIR offline materialization -> exact typed M17 terminal.
// Leaves existing strict same-rate seam unchanged, refuses unsupported pairs.
// This is NOT authorized for audible Processed publishing or export.
[[nodiscard]] core::Result<TruePeakLimiterOwnedSourceCandidate>
render_true_peak_limiter_owned_output_rate_candidate(
    std::shared_ptr<const RenderResult> accepted_manual_owner,
    const dsp::ProcessingChain& manual_chain_snapshot,
    core::FrameCount original_full_resource_frames,
    std::uint64_t expected_manual_chain_revision,
    core::SampleRate required_output_rate,
    const dsp::ProcessingChain& terminal_chain,
    std::span<const dsp::ModuleExecutionBinding> immutable_terminal_bindings,
    const dsp::ModuleRegistry& registry,
    core::FrameCount maximum_block_frames);

} // namespace rgsml::render
