#pragma once

#include "true_peak_static_binding.hpp"

#include <rgsml/platform/windows/windows_consumed_output_prefix.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace rgsml::app {

// C2D6: fail-closed PROCESSED resource ownership gate for consumed-source
// candidates. This is an application-worker input, NOT a GUI reading and not
// a fully-qualified LIVE True Peak status. The owner extends source lifetime.
struct BoundLiveTruePeakInput final {
    std::shared_ptr<const render::RenderResult> owner;
    core::RealizationId realizationId;
    std::uint64_t chainRevision{0};
    std::uint64_t playbackGeneration{0};
    std::int64_t consumedSourceEnd{0};
    std::int64_t consumedOutputEnd{0};
    audio::ChannelLayout layout{audio::ChannelLayout::STEREO_LR};
    core::SampleRate sampleRate;
};

// Contract accepts only a single uninterrupted, no-SRC/no-loop/no-handoff,
// source-zero one-to-one run. More complex real timelines remain unqualified
// and explicitly unavailable until independent continuous-segment proofs.
[[nodiscard]] core::Result<BoundLiveTruePeakInput>
bind_processed_live_input(
    std::shared_ptr<const render::RenderResult> accepted,
    core::RealizationId expectedRealization,
    const TruePeakExpectedSource& expectedSource,
    const core::PlaybackSnapshot& playback,
    const platform::windows::WindowsConsumedOutputPrefix& consumed);

} // namespace rgsml::app
