#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rgsml::audio {

inline constexpr std::string_view kPlaybackSrcCapabilityId =
    "rgsml.audio.src.playback";
inline constexpr std::string_view kPlaybackSrcAlgorithmVersion =
    "rgsml.playback-src.polyphase-fir/1.0.0";
inline constexpr std::string_view kPlaybackSrcKernelPolicyId =
    "rgsml.playback-src.kernel.kaiser-sinc/1.0.0";
inline constexpr std::string_view kPlaybackSrcRatePolicyId =
    "rgsml.playback-src.rates.44100-48000/1.0.0";
inline constexpr std::string_view kPlaybackSrcBoundaryPolicyId =
    "rgsml.src.boundary.even-reflect/1.0.0";
inline constexpr std::string_view kPlaybackSrcFrameMapPolicyId =
    "rgsml.src.frame-map.half-open-ceil/1.0.0";
inline constexpr std::string_view kPlaybackSrcKernelManifestId =
    "rgsml.playback-src.kernel-manifest/1.0.0";

struct PlaybackRateSpec final {
    core::SampleRate inputRate;
    core::SampleRate outputRate;
    ChannelLayout channelLayout;
    core::FrameCount totalInputFrames;
};

// Playback-only exact-rational adapter for the frozen 44.1 <-> 48 kHz policy.
// It reads bounded canonical windows through WavReader and writes ephemeral
// OUTPUT_RATE blocks. It is not a render node or a persisted audio artifact.
class PlaybackSampleRateAdapter final {
public:
    static constexpr std::int64_t kMaximumOutputBlockFrames = 1024;
    static constexpr std::int64_t kTapsPerPhase = 192;

    [[nodiscard]] static core::Result<PlaybackSampleRateAdapter> create(
        const PlaybackRateSpec& spec);

    [[nodiscard]] core::SampleRate input_rate() const noexcept;
    [[nodiscard]] core::SampleRate output_rate() const noexcept;
    [[nodiscard]] ChannelLayout channel_layout() const noexcept;
    [[nodiscard]] core::FrameCount input_frame_count() const noexcept;
    [[nodiscard]] core::FrameCount output_frame_count() const noexcept;
    [[nodiscard]] std::int64_t interpolation_factor() const noexcept;
    [[nodiscard]] std::int64_t decimation_factor() const noexcept;
    [[nodiscard]] std::size_t kernel_size() const noexcept;
    [[nodiscard]] std::int64_t group_delay_high_rate_frames() const noexcept;
    [[nodiscard]] std::string_view kernel_sha256() const noexcept;
    [[nodiscard]] std::uint64_t coefficient_bits(std::size_t index) const noexcept;

    [[nodiscard]] core::Result<core::FrameIndex> map_input_frame_to_output(
        core::FrameIndex inputFrame) const;
    [[nodiscard]] core::Result<core::FrameRange> map_input_range_to_output(
        core::FrameRange inputRange) const;
    [[nodiscard]] core::Result<core::FrameIndex> map_output_frame_to_input_cursor(
        core::FrameIndex outputFrame) const;

    [[nodiscard]] core::Result<core::FrameCount> read_frames(
        WavReader& source,
        core::FrameIndex absoluteOutputStart,
        MutableAudioBufferView destination) const;

private:
    PlaybackSampleRateAdapter(
        PlaybackRateSpec spec,
        std::int64_t interpolation,
        std::int64_t decimation,
        const std::uint64_t* coefficientBits,
        std::size_t coefficientCount,
        std::string_view kernelSha256,
        core::FrameCount outputFrames) noexcept;

    PlaybackRateSpec spec_;
    std::int64_t interpolation_;
    std::int64_t decimation_;
    const std::uint64_t* coefficientBits_;
    std::size_t coefficientCount_;
    std::string_view kernelSha256_;
    core::FrameCount outputFrames_;
};

}  // namespace rgsml::audio
