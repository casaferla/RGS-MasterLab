#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <array>
#include <cstddef>
#include <memory>

namespace rgsml::audio {

// Owning binary64 planar storage for rgsml.internal-audio-buffer/1.0.0.
// Runtime samples preserve every binary64 bit, including signed zero and
// subnormals. No checksum-view canonicalization is applied here.
class AudioBuffer final {
public:
    [[nodiscard]] static core::Result<AudioBuffer> create(
        AudioFormat format,
        FrameDomainId domain,
        core::FrameIndex absoluteStart,
        core::FrameCount frameCount);

    AudioBuffer(AudioBuffer&&) noexcept = default;
    AudioBuffer& operator=(AudioBuffer&&) noexcept = default;
    AudioBuffer(const AudioBuffer&) = delete;
    AudioBuffer& operator=(const AudioBuffer&) = delete;
    ~AudioBuffer() = default;

    [[nodiscard]] AudioBufferView view() const noexcept;
    [[nodiscard]] MutableAudioBufferView mutable_view() noexcept;

private:
    struct AlignedPlaneDelete final {
        void operator()(double* pointer) const noexcept;
    };

    using AlignedPlane = std::unique_ptr<double[], AlignedPlaneDelete>;

    AudioBuffer(
        AudioFormat format,
        AudioTimebase timebase,
        core::FrameRange absoluteRange,
        core::FrameCount frameCount,
        std::array<AlignedPlane, 2> planes) noexcept;

    AudioFormat format_;
    AudioTimebase timebase_;
    core::FrameRange absoluteRange_;
    core::FrameCount frameCount_;
    std::array<AlignedPlane, 2> planes_;
};

}  // namespace rgsml::audio
