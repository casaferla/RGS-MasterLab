#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/true_peak_limiter_module.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>

#include <cstdint>
#include <vector>

namespace rgsml::render {

// UNQUALIFIED M17 terminal-render integration seam, intentionally separate
// from SOURCE_PROCESSING_RATE Render Preview. Caller must already own a full,
// contiguous OUTPUT_RATE resource. No SRC, export, M16 full-resource TP
// verification, permitted global verification trim, audible-realization
// routing or final product availability is conferred by this API.
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

} // namespace rgsml::render
