#pragma once

#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/realization_identity.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/render/render_result.hpp>

#include <cstdint>
#include <memory>

namespace rgsml::app {

// A caller must obtain all three values from ONE authoritative source
// selection: this does not infer whole-track coverage from RenderResult alone.
struct TruePeakExpectedSource final {
    core::FrameCount frameCount;
    core::SampleRate sampleRate;
    audio::ChannelLayout layout;
};

// A static full-resource reading is linked to the immutable published render
// and realization. READY/accepted is NOT the same as audible NEW in the
// independent PlaybackSnapshot. No live cursor claim is embedded here.
struct BoundStaticTruePeak final {
    analysis::TruePeakMeasurement measurement;
    std::shared_ptr<const render::RenderResult> owner;
    core::RealizationId realizationId;
    core::FrameRange coveredSourceFrames;
    std::uint64_t chainRevision;

    [[nodiscard]] bool matches_audible(const core::PlaybackSnapshot& snapshot) const noexcept;
};

// Worker-callable C1 boundary; the UI MUST NOT call this synchronously.
// A successful result proves whole-source coverage relative to the explicit
// expected source contract. It remains SUPPORTED_UNQUALIFIED while Gate G open.
[[nodiscard]] core::Result<BoundStaticTruePeak>
measure_accepted_processed_true_peak(
    std::shared_ptr<const render::RenderResult> accepted,
    core::RealizationId processedRealizationId,
    const TruePeakExpectedSource& expected);

} // namespace rgsml::app
