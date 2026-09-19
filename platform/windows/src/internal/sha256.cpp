#include "internal/sha256.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>

namespace rgsml::platform::windows::internal {
namespace {

void append_u16_le(std::array<std::byte, 64>& bytes, std::size_t& cursor, std::uint16_t value)
{
    for (std::size_t index = 0; index < 2U; ++index) {
        bytes[cursor++] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
    }
}

void append_u32_le(std::array<std::byte, 64>& bytes, std::size_t& cursor, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes[cursor++] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
    }
}

void append_u64_le(std::array<std::byte, 64>& bytes, std::size_t& cursor, std::uint64_t value)
{
    for (std::size_t index = 0; index < 8U; ++index) {
        bytes[cursor++] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
    }
}

void encode_u64_le(std::uint64_t value, std::byte* destination) noexcept
{
    for (std::size_t index = 0; index < 8U; ++index) {
        destination[index] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
    }
}

template <typename T>
[[nodiscard]] core::Result<T> failure(core::ErrorCode code, std::string message)
{
    return core::Result<T>::failure(core::Error{code, std::move(message)});
}

[[nodiscard]] core::Status status_failure(core::ErrorCode code, std::string message)
{
    return core::Status::failure(core::Error{code, std::move(message)});
}

}  // namespace

core::Result<CanonicalDecodedAudioHasher> CanonicalDecodedAudioHasher::create(
    audio::AudioFormat format,
    core::FrameCount frameCount)
{
    if (frameCount.value() < 0) {
        return failure<CanonicalDecodedAudioHasher>(
            core::ErrorCode::InvalidArgument,
            "canonical_checksum_invalid_frame_count: frame count must not be negative.");
    }
    const auto channels = format.channel_count();
    const bool supported = (format.channel_layout() == audio::ChannelLayout::MONO_C && channels == 1U)
        || (format.channel_layout() == audio::ChannelLayout::STEREO_LR && channels == 2U);
    if (!supported) {
        return failure<CanonicalDecodedAudioHasher>(
            core::ErrorCode::UnsupportedAudioLayout,
            "canonical_checksum_unsupported_layout: only mono C and stereo L/R are supported.");
    }
    const auto sampleRate = format.sample_rate().value();
    if (sampleRate <= 0
        || sampleRate > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
        return failure<CanonicalDecodedAudioHasher>(
            core::ErrorCode::InvalidArgument,
            "canonical_checksum_invalid_sample_rate: sample rate is not representable.");
    }
    const auto frames = static_cast<std::uint64_t>(frameCount.value());
    const auto bytesPerFrame = static_cast<std::uint64_t>(channels) * 8U;
    if (frames != 0U && bytesPerFrame > std::numeric_limits<std::uint64_t>::max() / frames) {
        return failure<CanonicalDecodedAudioHasher>(
            core::ErrorCode::IntegerOverflow,
            "canonical_checksum_payload_overflow: payload size overflowed.");
    }

    std::array<std::byte, 64> header{};
    std::size_t cursor = 0;
    constexpr std::array<std::byte, 8> kMagic{
        std::byte{0x52}, std::byte{0x47}, std::byte{0x53}, std::byte{0x44},
        std::byte{0x41}, std::byte{0x55}, std::byte{0x31}, std::byte{0x00}};
    std::copy(kMagic.begin(), kMagic.end(), header.begin());
    cursor += kMagic.size();
    append_u16_le(header, cursor, 1U);
    append_u16_le(header, cursor, 1U);
    append_u32_le(header, cursor, static_cast<std::uint32_t>(sampleRate));
    append_u32_le(
        header,
        cursor,
        format.channel_layout() == audio::ChannelLayout::MONO_C ? 1U : 2U);
    append_u16_le(header, cursor, static_cast<std::uint16_t>(channels));
    if (format.channel_layout() == audio::ChannelLayout::MONO_C) {
        append_u16_le(header, cursor, 0x0001U);
    } else {
        append_u16_le(header, cursor, 0x0002U);
        append_u16_le(header, cursor, 0x0003U);
    }
    append_u64_le(header, cursor, frames);
    append_u64_le(header, cursor, frames * bytesPerFrame);
    core::Sha256 sha;
    sha.update(std::span<const std::byte>{header.data(), cursor});
    return core::Result<CanonicalDecodedAudioHasher>::success(
        CanonicalDecodedAudioHasher{format, frameCount, std::move(sha)});
}

CanonicalDecodedAudioHasher::CanonicalDecodedAudioHasher(
    audio::AudioFormat format,
    core::FrameCount frameCount,
    core::Sha256 sha) noexcept
    : format_(format)
    , frame_count_(frameCount)
    , sha_(std::move(sha))
{
}

core::Status CanonicalDecodedAudioHasher::update(audio::AudioBufferView source)
{
    if (finalized_) {
        return status_failure(
            core::ErrorCode::InvalidState,
            "canonical_checksum_finalized: checksum is already finalized.");
    }
    if (source.format() != format_) {
        return status_failure(
            core::ErrorCode::InvalidArgument,
            "canonical_checksum_format_mismatch: audio format changed while hashing.");
    }
    const auto incoming = source.frame_count().value();
    if (incoming > frame_count_.value() - frames_hashed_) {
        return status_failure(
            core::ErrorCode::OutOfRange,
            "canonical_checksum_frame_count_exceeded: too many frames supplied.");
    }
    std::array<std::span<const double>, 2> channels{};
    for (std::size_t channel = 0; channel < format_.channel_count(); ++channel) {
        auto plane = source.channel(channel);
        if (!plane) {
            return core::Status::failure(*plane.error());
        }
        channels[channel] = *plane.value();
        for (const auto sample : channels[channel]) {
            if (!std::isfinite(sample)) {
                return status_failure(
                    core::ErrorCode::InvalidAudioSample,
                    "canonical_checksum_invalid_audio_sample: NaN or infinity is not hashable.");
            }
        }
    }
    constexpr std::size_t kFramesPerBatch = 4096U;
    std::array<std::byte, kFramesPerBatch * 2U * 8U> payload{};
    std::size_t offset = 0;
    while (offset < static_cast<std::size_t>(incoming)) {
        const auto frames = std::min(
            kFramesPerBatch,
            static_cast<std::size_t>(incoming) - offset);
        std::size_t cursor = 0;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::size_t channel = 0; channel < format_.channel_count(); ++channel) {
                const auto sample = channels[channel][offset + frame];
                const auto bits = sample == 0.0
                    ? std::uint64_t{0}
                    : std::bit_cast<std::uint64_t>(sample);
                encode_u64_le(bits, payload.data() + cursor);
                cursor += 8U;
            }
        }
        sha_.update(std::span<const std::byte>{payload.data(), cursor});
        offset += frames;
    }
    frames_hashed_ += incoming;
    return core::Status::success();
}

core::Result<std::string> CanonicalDecodedAudioHasher::finalize()
{
    if (finalized_ || frames_hashed_ != frame_count_.value()) {
        return failure<std::string>(
            core::ErrorCode::InvalidState,
            "canonical_checksum_incomplete: exact declared frame count is required.");
    }
    finalized_ = true;
    return core::Result<std::string>::success(core::sha256_hex(sha_.finalize()));
}

core::Result<std::string> canonical_decoded_audio_sha256(
    audio::AudioBufferView source)
{
    auto hasher = CanonicalDecodedAudioHasher::create(
        source.format(), source.frame_count());
    if (!hasher) {
        return core::Result<std::string>::failure(*hasher.error());
    }
    auto update = hasher.value()->update(source);
    if (!update) {
        return core::Result<std::string>::failure(*update.error());
    }
    return hasher.value()->finalize();
}

}  // namespace rgsml::platform::windows::internal
