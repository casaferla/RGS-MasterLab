#pragma once

#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <array>
#include <cstddef>
#include <span>

namespace rgsml::audio {

class AudioBuffer;
class MutableAudioBufferView;

class AudioBufferView final {
public:
    [[nodiscard]] const AudioFormat& format() const noexcept;
    [[nodiscard]] const AudioTimebase& timebase() const noexcept;
    [[nodiscard]] core::FrameCount frame_count() const noexcept;
    [[nodiscard]] core::FrameIndex absolute_start_frame() const noexcept;
    [[nodiscard]] core::FrameIndex absolute_end_frame() const noexcept;
    [[nodiscard]] core::FrameRange absolute_range() const noexcept;

    [[nodiscard]] core::Result<core::FrameIndex> absolute_frame(
        std::size_t localFrame) const;
    [[nodiscard]] core::Result<std::span<const double>> channel(
        std::size_t channelIndex) const;
    [[nodiscard]] core::Result<AudioBufferView> subview(
        core::FrameIndex absoluteStart,
        core::FrameCount frameCount) const;

private:
    friend class AudioBuffer;
    friend class MutableAudioBufferView;

    AudioBufferView(
        AudioFormat format,
        AudioTimebase timebase,
        core::FrameRange absoluteRange,
        core::FrameCount frameCount,
        std::array<std::span<const double>, 2> channels) noexcept;

    AudioFormat format_;
    AudioTimebase timebase_;
    core::FrameRange absoluteRange_;
    core::FrameCount frameCount_;
    std::array<std::span<const double>, 2> channels_;
};

class MutableAudioBufferView final {
public:
    [[nodiscard]] const AudioFormat& format() const noexcept;
    [[nodiscard]] const AudioTimebase& timebase() const noexcept;
    [[nodiscard]] core::FrameCount frame_count() const noexcept;
    [[nodiscard]] core::FrameIndex absolute_start_frame() const noexcept;
    [[nodiscard]] core::FrameIndex absolute_end_frame() const noexcept;
    [[nodiscard]] core::FrameRange absolute_range() const noexcept;

    [[nodiscard]] core::Result<core::FrameIndex> absolute_frame(
        std::size_t localFrame) const;
    [[nodiscard]] core::Result<std::span<double>> channel(std::size_t channelIndex);
    [[nodiscard]] core::Result<std::span<const double>> channel(
        std::size_t channelIndex) const;
    [[nodiscard]] core::Result<MutableAudioBufferView> subview(
        core::FrameIndex absoluteStart,
        core::FrameCount frameCount);
    [[nodiscard]] AudioBufferView as_const() const noexcept;

private:
    friend class AudioBuffer;

    MutableAudioBufferView(
        AudioFormat format,
        AudioTimebase timebase,
        core::FrameRange absoluteRange,
        core::FrameCount frameCount,
        std::array<std::span<double>, 2> channels) noexcept;

    AudioFormat format_;
    AudioTimebase timebase_;
    core::FrameRange absoluteRange_;
    core::FrameCount frameCount_;
    std::array<std::span<double>, 2> channels_;
};

}  // namespace rgsml::audio
