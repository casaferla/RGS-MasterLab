#pragma once
// M19/M20 independent REFERENCE full-track PCM measurement without retaining
// decoded track-sized AudioBuffer. Internal GOLD API/Project tokens unchanged.
// PREPRODUCTION: does not confer Phase-6 or M16 True Peak release authority.
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <cstddef>
#include <functional>

namespace rgsml::analysis {
struct StreamedReferenceAnalysis final {
    BasicLevelAnalysisResult level;
    TonalStereoAnalysisResult tonal;
    // Bound only decoded PCM workspaces; retained M19 gate and M20 grid
    // outputs grow with programme duration by the existing data contract.
    std::size_t max_decoded_block_frames{0};
    std::size_t max_correlation_window_frames{0};
};

// Callback must fill the exact supplied absolute-frame, format and source
// domain view from the SAME independent immutable REFERENCE WAV. Must report
// actual frames decoded; partial/short reads fail closed. Called repeatedly,
// including nonsequential overlapping stereo/correlation reads. Thread-safe
// Qt or SOURCE access is prohibited inside the callback.
using StreamedReferenceRead = std::function<core::Result<core::FrameCount>(
    core::FrameIndex,audio::MutableAudioBufferView)>;

[[nodiscard]] core::Result<StreamedReferenceAnalysis>
measure_streamed_reference_scope(
    const audio::AudioFormat& format,
    core::FrameCount totalFrames,
    const StreamedReferenceRead& read);
} // namespace rgsml::analysis
