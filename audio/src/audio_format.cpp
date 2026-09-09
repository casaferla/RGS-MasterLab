#include <rgsml/audio/audio_format.hpp>

namespace rgsml::audio {

core::Result<AudioFormat> AudioFormat::create(
    core::SampleRate sampleRate,
    ChannelLayout channelLayout)
{
    switch (channelLayout) {
    case ChannelLayout::MONO_C:
    case ChannelLayout::STEREO_LR:
        return core::Result<AudioFormat>::success(
            AudioFormat{sampleRate, channelLayout});
    }
    return core::Result<AudioFormat>::failure(core::Error{
        core::ErrorCode::UnsupportedAudioLayout,
        "The channel layout is not supported by the canonical audio buffer."});
}

AudioFormat::AudioFormat(
    core::SampleRate sampleRate,
    ChannelLayout channelLayout) noexcept
    : sampleRate_(sampleRate)
    , channelLayout_(channelLayout)
{
}

core::SampleRate AudioFormat::sample_rate() const noexcept
{
    return sampleRate_;
}

ChannelLayout AudioFormat::channel_layout() const noexcept
{
    return channelLayout_;
}

std::size_t AudioFormat::channel_count() const noexcept
{
    return channelLayout_ == ChannelLayout::MONO_C ? 1U : 2U;
}

core::Result<AudioChannel> AudioFormat::channel_at(std::size_t index) const
{
    if (channelLayout_ == ChannelLayout::MONO_C && index == 0U) {
        return core::Result<AudioChannel>::success(AudioChannel::C);
    }
    if (channelLayout_ == ChannelLayout::STEREO_LR && index < 2U) {
        return core::Result<AudioChannel>::success(
            index == 0U ? AudioChannel::L : AudioChannel::R);
    }
    return core::Result<AudioChannel>::failure(
        core::Error{core::ErrorCode::OutOfRange, "Audio channel index is out of range."});
}

core::Result<AudioTimebase> AudioTimebase::create(
    core::SampleRate sampleRate,
    FrameDomainId frameDomainId)
{
    switch (frameDomainId) {
    case FrameDomainId::SOURCE_PROCESSING_RATE:
    case FrameDomainId::OUTPUT_RATE:
        return core::Result<AudioTimebase>::success(
            AudioTimebase{sampleRate, frameDomainId});
    }
    return core::Result<AudioTimebase>::failure(
        core::Error{core::ErrorCode::InvalidArgument, "Invalid frame-domain identifier."});
}

AudioTimebase::AudioTimebase(
    core::SampleRate sampleRate,
    FrameDomainId frameDomainId) noexcept
    : sampleRate_(sampleRate)
    , frameDomainId_(frameDomainId)
{
}

core::SampleRate AudioTimebase::sample_rate() const noexcept
{
    return sampleRate_;
}

FrameDomainId AudioTimebase::frame_domain_id() const noexcept
{
    return frameDomainId_;
}

}  // namespace rgsml::audio
