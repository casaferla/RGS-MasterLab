#pragma once

#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <cstddef>

namespace rgsml::audio {

enum class ChannelLayout {
    MONO_C,
    STEREO_LR,
};

enum class AudioChannel {
    C,
    L,
    R,
};

enum class FrameDomainId {
    SOURCE_PROCESSING_RATE,
    OUTPUT_RATE,
};

// Logical decoded format. Container packing is represented separately by the
// WAV metadata contract.
class AudioFormat final {
public:
    [[nodiscard]] static core::Result<AudioFormat> create(
        core::SampleRate sampleRate,
        ChannelLayout channelLayout);

    [[nodiscard]] core::SampleRate sample_rate() const noexcept;
    [[nodiscard]] ChannelLayout channel_layout() const noexcept;
    [[nodiscard]] std::size_t channel_count() const noexcept;
    [[nodiscard]] core::Result<AudioChannel> channel_at(std::size_t index) const;

    [[nodiscard]] bool operator==(const AudioFormat&) const = default;

private:
    AudioFormat(core::SampleRate sampleRate, ChannelLayout channelLayout) noexcept;

    core::SampleRate sampleRate_;
    ChannelLayout channelLayout_;
};

class AudioTimebase final {
public:
    [[nodiscard]] static core::Result<AudioTimebase> create(
        core::SampleRate sampleRate,
        FrameDomainId frameDomainId);

    [[nodiscard]] core::SampleRate sample_rate() const noexcept;
    [[nodiscard]] FrameDomainId frame_domain_id() const noexcept;

    [[nodiscard]] bool operator==(const AudioTimebase&) const = default;

private:
    AudioTimebase(core::SampleRate sampleRate, FrameDomainId frameDomainId) noexcept;

    core::SampleRate sampleRate_;
    FrameDomainId frameDomainId_;
};

}  // namespace rgsml::audio
