#pragma once

#include "terminal_candidate_gate.hpp"

#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <optional>

namespace rgsml::app {

// Pure, candidate-only coordinate bridge. The SOURCE waveform, Region,
// playhead and UI remain on SOURCE_PROCESSING_RATE frames. OUTPUT_RATE is
// the ONLY coordinate domain of final terminal PCM. The live playback
// service does not consume this class until a separate integration gate.
class TerminalPlaybackFrameMap final {
public:
    // Derive only from privately ACCEPTED terminal materialization and its
    // original RenderResult owner, never from an arbitrary sample-rate pair.
    [[nodiscard]] static core::Result<TerminalPlaybackFrameMap> create(
        const TerminalCandidateGate::StagedCandidate& accepted);

    [[nodiscard]] core::SampleRate source_rate() const noexcept
    { return sourceRate_; }
    [[nodiscard]] core::SampleRate output_rate() const noexcept
    { return outputRate_; }
    [[nodiscard]] core::FrameCount source_frames() const noexcept
    { return sourceFrames_; }
    [[nodiscard]] core::FrameCount output_frames() const noexcept
    { return outputFrames_; }
    [[nodiscard]] bool converted() const noexcept
    { return adapter_.has_value(); }

    // SOURCE playhead seeking is mapped by the FROZEN half-open ceil rule.
    [[nodiscard]] core::Result<core::FrameIndex> source_to_output(
        core::FrameIndex sourceFrame) const;
    // Display-only cursor inverse, NOT consumed-sample TP Meter/GR proof.
    [[nodiscard]] core::Result<core::FrameIndex> output_to_source_display(
        core::FrameIndex outputFrame) const;
    // Loop region remains SOURCE-anchored; map BOTH half-open boundaries.
    // Tiny regions collapsing at the lower Fs are refused rather than
    // silently shortening or disabling the loop.
    [[nodiscard]] core::Result<core::FrameRange> source_loop_to_output(
        core::FrameRange sourceLoop) const;

private:
    TerminalPlaybackFrameMap(
        core::SampleRate src, core::SampleRate dst,
        core::FrameCount srcFrames, core::FrameCount dstFrames,
        std::optional<audio::PlaybackSampleRateAdapter> adapter) noexcept
      : sourceRate_(src),outputRate_(dst),sourceFrames_(srcFrames),
        outputFrames_(dstFrames),adapter_(std::move(adapter)) {}

    core::SampleRate sourceRate_;
    core::SampleRate outputRate_;
    core::FrameCount sourceFrames_;
    core::FrameCount outputFrames_;
    std::optional<audio::PlaybackSampleRateAdapter> adapter_;
};

} // namespace rgsml::app
