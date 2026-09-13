#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

namespace rgsml::audio::internal {

// Private, playback-only frame source. Frame indices are local to the prepared
// realization; the backing may be a decoder or an immutable canonical view.
class PlaybackSrcInput {
public:
    virtual ~PlaybackSrcInput() noexcept = default;

    [[nodiscard]] virtual const AudioFormat& format() const noexcept = 0;
    [[nodiscard]] virtual core::FrameCount frame_count() const noexcept = 0;
    [[nodiscard]] virtual core::Result<core::FrameCount> read_frames(
        core::FrameIndex localStart,
        MutableAudioBufferView destination) = 0;
};

[[nodiscard]] core::Result<core::FrameCount> read_playback_src_frames(
    const PlaybackSampleRateAdapter& adapter,
    PlaybackSrcInput& source,
    core::FrameIndex absoluteOutputStart,
    MutableAudioBufferView destination);

}  // namespace rgsml::audio::internal
