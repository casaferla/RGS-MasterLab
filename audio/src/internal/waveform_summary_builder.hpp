#pragma once

#include <rgsml/audio/waveform_summary.hpp>

#include <cstddef>
#include <stop_token>

namespace rgsml::audio {

// Test seam for proving decode partition invariance. Product code always uses
// WaveformSummary::kMaximumDecodeBlockFrames through build_waveform_summary().
[[nodiscard]] core::Result<WaveformSummary> build_waveform_summary_with_block_limit(
    WavReader& reader,
    std::size_t blockFrameLimit,
    std::stop_token stopToken = {});

}  // namespace rgsml::audio
