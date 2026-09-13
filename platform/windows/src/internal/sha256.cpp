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

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

[[nodiscard]] constexpr std::uint32_t choose(
    std::uint32_t x,
    std::uint32_t y,
    std::uint32_t z) noexcept
{
    return (x & y) ^ (~x & z);
}

[[nodiscard]] constexpr std::uint32_t majority(
    std::uint32_t x,
    std::uint32_t y,
    std::uint32_t z) noexcept
{
    return (x & y) ^ (x & z) ^ (y & z);
}

[[nodiscard]] constexpr std::uint32_t big_sigma0(std::uint32_t x) noexcept
{
    return std::rotr(x, 2) ^ std::rotr(x, 13) ^ std::rotr(x, 22);
}

[[nodiscard]] constexpr std::uint32_t big_sigma1(std::uint32_t x) noexcept
{
    return std::rotr(x, 6) ^ std::rotr(x, 11) ^ std::rotr(x, 25);
}

[[nodiscard]] constexpr std::uint32_t small_sigma0(std::uint32_t x) noexcept
{
    return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3U);
}

[[nodiscard]] constexpr std::uint32_t small_sigma1(std::uint32_t x) noexcept
{
    return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10U);
}

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

Sha256::Sha256() noexcept
    : state_{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U}
{
}

void Sha256::transform(const std::byte* block) noexcept
{
    std::array<std::uint32_t, 64> words{};
    for (std::size_t index = 0; index < 16U; ++index) {
        const auto offset = index * 4U;
        words[index] = (static_cast<std::uint32_t>(block[offset]) << 24U)
            | (static_cast<std::uint32_t>(block[offset + 1U]) << 16U)
            | (static_cast<std::uint32_t>(block[offset + 2U]) << 8U)
            | static_cast<std::uint32_t>(block[offset + 3U]);
    }
    for (std::size_t index = 16U; index < words.size(); ++index) {
        words[index] = small_sigma1(words[index - 2U]) + words[index - 7U]
            + small_sigma0(words[index - 15U]) + words[index - 16U];
    }

    auto a = state_[0];
    auto b = state_[1];
    auto c = state_[2];
    auto d = state_[3];
    auto e = state_[4];
    auto f = state_[5];
    auto g = state_[6];
    auto h = state_[7];
    for (std::size_t index = 0; index < words.size(); ++index) {
        const auto t1 = h + big_sigma1(e) + choose(e, f, g)
            + kRoundConstants[index] + words[index];
        const auto t2 = big_sigma0(a) + majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(std::span<const std::byte> bytes) noexcept
{
    if (finalized_ || bytes.empty()) {
        return;
    }
    total_bytes_ += static_cast<std::uint64_t>(bytes.size());
    std::size_t offset = 0;
    if (buffered_ != 0U) {
        const auto copied = std::min(bytes.size(), buffer_.size() - buffered_);
        std::copy_n(bytes.data(), copied, buffer_.data() + buffered_);
        buffered_ += copied;
        offset += copied;
        if (buffered_ == buffer_.size()) {
            transform(buffer_.data());
            buffered_ = 0U;
        }
    }
    while (bytes.size() - offset >= buffer_.size()) {
        transform(bytes.data() + offset);
        offset += buffer_.size();
    }
    const auto remaining = bytes.size() - offset;
    if (remaining != 0U) {
        std::copy_n(bytes.data() + offset, remaining, buffer_.data());
        buffered_ = remaining;
    }
}

Sha256Digest Sha256::finalize() noexcept
{
    if (!finalized_) {
        const auto bitLength = total_bytes_ * 8U;
        buffer_[buffered_++] = std::byte{0x80};
        if (buffered_ > 56U) {
            std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.end(), std::byte{0});
            transform(buffer_.data());
            buffered_ = 0U;
        }
        std::fill(
            buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_),
            buffer_.begin() + 56,
            std::byte{0});
        for (std::size_t index = 0; index < 8U; ++index) {
            buffer_[56U + index] = static_cast<std::byte>(
                (bitLength >> ((7U - index) * 8U)) & 0xffU);
        }
        transform(buffer_.data());
        finalized_ = true;
    }
    Sha256Digest digest{};
    for (std::size_t word = 0; word < state_.size(); ++word) {
        for (std::size_t byte = 0; byte < 4U; ++byte) {
            digest[word * 4U + byte] = static_cast<std::byte>(
                (state_[word] >> ((3U - byte) * 8U)) & 0xffU);
        }
    }
    return digest;
}

std::string sha256_hex(const Sha256Digest& digest)
{
    constexpr char kHex[] = "0123456789abcdef";
    std::string text;
    text.resize(digest.size() * 2U);
    for (std::size_t index = 0; index < digest.size(); ++index) {
        const auto value = static_cast<unsigned char>(digest[index]);
        text[index * 2U] = kHex[value >> 4U];
        text[index * 2U + 1U] = kHex[value & 0x0fU];
    }
    return text;
}

std::string sha256_hex(std::span<const std::byte> bytes)
{
    Sha256 sha;
    sha.update(bytes);
    return sha256_hex(sha.finalize());
}

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
    Sha256 sha;
    sha.update(std::span<const std::byte>{header.data(), cursor});
    return core::Result<CanonicalDecodedAudioHasher>::success(
        CanonicalDecodedAudioHasher{format, frameCount, std::move(sha)});
}

CanonicalDecodedAudioHasher::CanonicalDecodedAudioHasher(
    audio::AudioFormat format,
    core::FrameCount frameCount,
    Sha256 sha) noexcept
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
    return core::Result<std::string>::success(sha256_hex(sha_.finalize()));
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
