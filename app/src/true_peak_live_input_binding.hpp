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
    // Absolute source-domain seek origin; output consumption stays relative
    // to the new device-clock generation.
    std::int64_t consumedSourceBegin{0};
    std::int64_t consumedSourceEnd{0};
    std::int64_t consumedOutputEnd{0};
    audio::ChannelLayout layout{audio::ChannelLayout::STEREO_LR};
    core::SampleRate sampleRate;
    // Sink-audible traversal identity, never UI/checkbox-derived.
    std::uint64_t loopWrapCount{0};
};

// Strictly verified immutable PROCESSED SOURCE_PROCESSING_RATE target. Direct
// 1:1 consumed suffixes remain eligible under the existing seek/loop/handoff
// rules. C2D18 SRC permits a sink-consumed, owner-stamped 44.1↔48-kHz
// traversal, after seek and within a verified Loop Region epoch. The accepted
// OUTPUT ledger must retain SRC decoder origins, generation and wrap identity;
// each new wrap discards older FIR/RECENT/HOLD history. A conservative
// SOURCE_PROCESSING_RATE interval is retired from ALL future current-traversal
// FIR dependencies. Raw SRC OUTPUT slices remain GAP_UNPROVEN; no blanket
// certification comes from a GUI cursor or loop checkbox. Unknown owner,
// rate, sink clock, source boundary, stale wrap or handoff fails closed.
[[nodiscard]] core::Result<BoundLiveTruePeakInput>
bind_processed_live_input(
    std::shared_ptr<const render::RenderResult> accepted,
    core::RealizationId expectedRealization,
    const TruePeakExpectedSource& expectedSource,
    const core::PlaybackSnapshot& playback,
    const platform::windows::WindowsConsumedOutputPrefix& consumed);

} // namespace rgsml::app
