#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/core/resource_io.hpp>

#include <QtCore/QtGlobal>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::tests::wav_support {

using Bytes = std::vector<std::byte>;

struct ReaderControl final {
    std::size_t maxTransfer{std::numeric_limits<std::size_t>::max()};
    std::optional<std::uint64_t> failAtOffset;
    std::size_t readCalls{0};
    std::size_t seekCalls{0};
    std::size_t closeCalls{0};
    std::size_t totalBytesRead{0};
    std::size_t largestReadRequest{0};
};

[[nodiscard]] inline core::Error test_error(
    core::ErrorCode code,
    std::string message)
{
    return core::Error{code, std::move(message)};
}

[[nodiscard]] inline core::ResourceReference make_reference(bool canRead = true)
{
    auto reference = core::ResourceReference::create(
        "test.memory", "opaque-wav", canRead, !canRead, "Synthetic WAV");
    Q_ASSERT(reference.value() != nullptr);
    return *reference.value();
}

class MemoryReader final : public core::IResourceReader {
public:
    MemoryReader(
        Bytes content,
        std::shared_ptr<ReaderControl> control,
        bool canSeek = true,
        bool hasKnownSize = true,
        bool canRead = true)
        : reference_(make_reference(canRead))
        , content_(std::move(content))
        , control_(std::move(control))
        , capabilities_(core::ResourceCapabilities::create(
              canSeek, hasKnownSize, false, false))
    {
    }

    [[nodiscard]] const core::ResourceReference& reference() const noexcept override
    {
        return reference_;
    }

    [[nodiscard]] core::ResourceCapabilities capabilities() const noexcept override
    {
        return capabilities_;
    }

    [[nodiscard]] core::Result<std::uint64_t> size_bytes() const override
    {
        if (closed_) {
            return core::Result<std::uint64_t>::failure(
                test_error(core::ErrorCode::InvalidState, "Reader is closed."));
        }
        if (!capabilities_.supports(core::ResourceCapability::HasKnownSize)) {
            return core::Result<std::uint64_t>::failure(test_error(
                core::ErrorCode::UnsupportedOperation, "Resource size is unknown."));
        }
        return core::Result<std::uint64_t>::success(
            static_cast<std::uint64_t>(content_.size()));
    }

    [[nodiscard]] core::Result<std::uint64_t> position_bytes() const override
    {
        if (closed_) {
            return core::Result<std::uint64_t>::failure(
                test_error(core::ErrorCode::InvalidState, "Reader is closed."));
        }
        return core::Result<std::uint64_t>::success(position_);
    }

    [[nodiscard]] core::Result<std::size_t> read(
        std::span<std::byte> destination) override
    {
        ++control_->readCalls;
        control_->largestReadRequest = std::max(
            control_->largestReadRequest, destination.size());
        if (closed_) {
            return core::Result<std::size_t>::failure(
                test_error(core::ErrorCode::InvalidState, "Reader is closed."));
        }
        if (!reference_.permissions().can_read()) {
            return core::Result<std::size_t>::failure(
                test_error(core::ErrorCode::AccessDenied, "Read permission is absent."));
        }
        if (control_->failAtOffset && position_ >= *control_->failAtOffset) {
            return core::Result<std::size_t>::failure(
                test_error(core::ErrorCode::IoFailure, "Injected read failure."));
        }
        if (destination.empty() || position_ >= content_.size()) {
            return core::Result<std::size_t>::success(0U);
        }

        auto transfer = std::min({
            destination.size(),
            content_.size() - static_cast<std::size_t>(position_),
            control_->maxTransfer,
        });
        if (control_->failAtOffset && position_ < *control_->failAtOffset) {
            transfer = std::min(
                transfer,
                static_cast<std::size_t>(*control_->failAtOffset - position_));
        }
        std::copy_n(
            content_.begin() + static_cast<std::ptrdiff_t>(position_),
            static_cast<std::ptrdiff_t>(transfer),
            destination.begin());
        position_ += static_cast<std::uint64_t>(transfer);
        control_->totalBytesRead += transfer;
        return core::Result<std::size_t>::success(transfer);
    }

    [[nodiscard]] core::Status seek_bytes(std::uint64_t absoluteOffset) override
    {
        ++control_->seekCalls;
        if (closed_) {
            return core::Status::failure(
                test_error(core::ErrorCode::InvalidState, "Reader is closed."));
        }
        if (!capabilities_.supports(core::ResourceCapability::CanSeek)) {
            return core::Status::failure(test_error(
                core::ErrorCode::UnsupportedOperation, "Seek is unsupported."));
        }
        if (absoluteOffset > content_.size()) {
            return core::Status::failure(
                test_error(core::ErrorCode::OutOfRange, "Seek is out of range."));
        }
        position_ = absoluteOffset;
        return core::Status::success();
    }

    [[nodiscard]] core::Status close() override
    {
        ++control_->closeCalls;
        closed_ = true;
        return core::Status::success();
    }

private:
    core::ResourceReference reference_;
    Bytes content_;
    std::shared_ptr<ReaderControl> control_;
    core::ResourceCapabilities capabilities_;
    std::uint64_t position_{0};
    bool closed_{false};
};

struct SparseSegment final {
    std::uint64_t offset;
    Bytes bytes;
};

class SparseReader final : public core::IResourceReader {
public:
    SparseReader(
        std::uint64_t logicalSize,
        std::vector<SparseSegment> segments,
        std::shared_ptr<ReaderControl> control)
        : reference_(make_reference())
        , logicalSize_(logicalSize)
        , segments_(std::move(segments))
        , control_(std::move(control))
    {
    }

    [[nodiscard]] const core::ResourceReference& reference() const noexcept override
    {
        return reference_;
    }

    [[nodiscard]] core::ResourceCapabilities capabilities() const noexcept override
    {
        return core::ResourceCapabilities::create(true, true, false, false);
    }

    [[nodiscard]] core::Result<std::uint64_t> size_bytes() const override
    {
        return core::Result<std::uint64_t>::success(logicalSize_);
    }

    [[nodiscard]] core::Result<std::uint64_t> position_bytes() const override
    {
        if (closed_) {
            return core::Result<std::uint64_t>::failure(
                test_error(core::ErrorCode::InvalidState, "Reader is closed."));
        }
        return core::Result<std::uint64_t>::success(position_);
    }

    [[nodiscard]] core::Result<std::size_t> read(
        std::span<std::byte> destination) override
    {
        ++control_->readCalls;
        control_->largestReadRequest = std::max(
            control_->largestReadRequest, destination.size());
        if (closed_) {
            return core::Result<std::size_t>::failure(
                test_error(core::ErrorCode::InvalidState, "Reader is closed."));
        }
        if (destination.empty() || position_ >= logicalSize_) {
            return core::Result<std::size_t>::success(0U);
        }
        const auto available = logicalSize_ - position_;
        const auto transfer = static_cast<std::size_t>(std::min<std::uint64_t>(
            std::min<std::size_t>(destination.size(), control_->maxTransfer),
            available));
        std::fill_n(destination.begin(), static_cast<std::ptrdiff_t>(transfer), std::byte{0});
        for (const auto& segment : segments_) {
            const auto readEnd = position_ + transfer;
            const auto segmentEnd = segment.offset + segment.bytes.size();
            const auto overlapBegin = std::max(position_, segment.offset);
            const auto overlapEnd = std::min(readEnd, segmentEnd);
            if (overlapBegin < overlapEnd) {
                const auto sourceOffset = static_cast<std::size_t>(overlapBegin - segment.offset);
                const auto destinationOffset = static_cast<std::size_t>(overlapBegin - position_);
                const auto count = static_cast<std::size_t>(overlapEnd - overlapBegin);
                std::copy_n(
                    segment.bytes.begin() + static_cast<std::ptrdiff_t>(sourceOffset),
                    static_cast<std::ptrdiff_t>(count),
                    destination.begin() + static_cast<std::ptrdiff_t>(destinationOffset));
            }
        }
        position_ += transfer;
        control_->totalBytesRead += transfer;
        return core::Result<std::size_t>::success(transfer);
    }

    [[nodiscard]] core::Status seek_bytes(std::uint64_t absoluteOffset) override
    {
        ++control_->seekCalls;
        if (closed_) {
            return core::Status::failure(
                test_error(core::ErrorCode::InvalidState, "Reader is closed."));
        }
        if (absoluteOffset > logicalSize_) {
            return core::Status::failure(
                test_error(core::ErrorCode::OutOfRange, "Seek is out of range."));
        }
        position_ = absoluteOffset;
        return core::Status::success();
    }

    [[nodiscard]] core::Status close() override
    {
        ++control_->closeCalls;
        closed_ = true;
        return core::Status::success();
    }

private:
    core::ResourceReference reference_;
    std::uint64_t logicalSize_;
    std::vector<SparseSegment> segments_;
    std::shared_ptr<ReaderControl> control_;
    std::uint64_t position_{0};
    bool closed_{false};
};

[[nodiscard]] inline std::array<char, 4> fourcc(const char (&text)[5]) noexcept
{
    return {text[0], text[1], text[2], text[3]};
}

inline void append_fourcc(Bytes& output, std::array<char, 4> id)
{
    for (const auto value : id) {
        output.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }
}

inline void append_u16(Bytes& output, std::uint16_t value)
{
    output.push_back(static_cast<std::byte>(value & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
}

inline void append_u32(Bytes& output, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4U; ++index) {
        output.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xffU));
    }
}

inline void append_u64(Bytes& output, std::uint64_t value)
{
    for (std::size_t index = 0; index < 8U; ++index) {
        output.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xffU));
    }
}

inline void write_u32(Bytes& output, std::size_t offset, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4U; ++index) {
        output[offset + index] = static_cast<std::byte>(
            (value >> (index * 8U)) & 0xffU);
    }
}

inline void write_u16(Bytes& output, std::size_t offset, std::uint16_t value)
{
    output[offset] = static_cast<std::byte>(value & 0xffU);
    output[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xffU);
}

inline void write_u64(Bytes& output, std::size_t offset, std::uint64_t value)
{
    for (std::size_t index = 0; index < 8U; ++index) {
        output[offset + index] = static_cast<std::byte>(
            (value >> (index * 8U)) & 0xffU);
    }
}

inline void append_chunk(
    Bytes& output,
    std::array<char, 4> id,
    std::span<const std::byte> payload,
    bool includePadding = true,
    std::optional<std::uint32_t> declaredSize = std::nullopt)
{
    append_fourcc(output, id);
    append_u32(
        output,
        declaredSize.value_or(static_cast<std::uint32_t>(payload.size())));
    output.insert(output.end(), payload.begin(), payload.end());
    if (includePadding && (payload.size() & 1U) != 0U) {
        output.push_back(std::byte{0});
    }
}

inline constexpr std::array<std::uint8_t, 16> kPcmGuid{
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
inline constexpr std::array<std::uint8_t, 16> kFloatGuid{
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};

[[nodiscard]] inline Bytes make_fmt_payload(
    std::uint16_t sourceTag,
    std::uint16_t channels,
    std::uint32_t sampleRate,
    std::uint16_t bits,
    bool extensible = false,
    std::uint16_t validBits = 0U,
    std::uint32_t channelMask = 0U)
{
    const auto bytesPerSample = static_cast<std::uint16_t>(bits / 8U);
    const auto blockAlign = static_cast<std::uint16_t>(channels * bytesPerSample);
    const auto byteRate = sampleRate * static_cast<std::uint32_t>(blockAlign);
    Bytes payload;
    append_u16(payload, extensible ? 0xfffeU : sourceTag);
    append_u16(payload, channels);
    append_u32(payload, sampleRate);
    append_u32(payload, byteRate);
    append_u16(payload, blockAlign);
    append_u16(payload, bits);
    if (extensible) {
        append_u16(payload, 22U);
        append_u16(payload, validBits == 0U ? bits : validBits);
        const auto defaultMask = channels == 1U ? 0x4U : 0x3U;
        append_u32(payload, channelMask == 0U ? defaultMask : channelMask);
        const auto& guid = sourceTag == 3U ? kFloatGuid : kPcmGuid;
        for (const auto value : guid) {
            payload.push_back(static_cast<std::byte>(value));
        }
    } else if (sourceTag == 3U) {
        append_u16(payload, 0U);
    }
    return payload;
}

[[nodiscard]] inline Bytes wrap_riff(std::span<const std::byte> chunks)
{
    Bytes output;
    append_fourcc(output, fourcc("RIFF"));
    append_u32(output, static_cast<std::uint32_t>(4U + chunks.size()));
    append_fourcc(output, fourcc("WAVE"));
    output.insert(output.end(), chunks.begin(), chunks.end());
    return output;
}

[[nodiscard]] inline Bytes make_wav(
    std::uint16_t sourceTag,
    std::uint16_t bits,
    std::uint16_t channels,
    std::uint32_t sampleRate,
    std::span<const std::byte> payload,
    bool extensible = false,
    bool rf64 = false,
    std::span<const std::byte> unknownEven = {},
    std::span<const std::byte> unknownOdd = {})
{
    const auto fmt = make_fmt_payload(
        sourceTag, channels, sampleRate, bits, extensible);
    Bytes chunks;
    if (!unknownEven.empty()) {
        append_chunk(chunks, fourcc("JUNK"), unknownEven);
    }
    if (!unknownOdd.empty()) {
        append_chunk(chunks, fourcc("LIST"), unknownOdd);
    }
    append_chunk(chunks, fourcc("fmt "), fmt);

    if (!rf64) {
        append_chunk(chunks, fourcc("data"), payload);
        return wrap_riff(chunks);
    }

    Bytes output;
    append_fourcc(output, fourcc("RF64"));
    append_u32(output, std::numeric_limits<std::uint32_t>::max());
    append_fourcc(output, fourcc("WAVE"));
    Bytes ds64(28U, std::byte{0});
    append_chunk(output, fourcc("ds64"), ds64);
    output.insert(output.end(), chunks.begin(), chunks.end());
    append_chunk(
        output,
        fourcc("data"),
        payload,
        true,
        std::numeric_limits<std::uint32_t>::max());

    const auto blockAlign = static_cast<std::uint64_t>(channels) * (bits / 8U);
    write_u64(output, 20U, static_cast<std::uint64_t>(output.size() - 8U));
    write_u64(output, 28U, static_cast<std::uint64_t>(payload.size()));
    write_u64(
        output,
        36U,
        blockAlign == 0U ? 0U : static_cast<std::uint64_t>(payload.size()) / blockAlign);
    return output;
}

[[nodiscard]] inline Bytes pcm_payload(
    std::span<const std::int64_t> codes,
    std::uint16_t bits)
{
    Bytes output;
    const auto bytesPerSample = static_cast<std::size_t>(bits / 8U);
    for (const auto code : codes) {
        const auto raw = static_cast<std::uint64_t>(code);
        for (std::size_t index = 0; index < bytesPerSample; ++index) {
            output.push_back(static_cast<std::byte>(
                (raw >> (index * 8U)) & 0xffU));
        }
    }
    return output;
}

[[nodiscard]] inline Bytes f32_payload(std::span<const std::uint32_t> patterns)
{
    Bytes output;
    for (const auto pattern : patterns) {
        append_u32(output, pattern);
    }
    return output;
}

[[nodiscard]] inline Bytes f64_payload(std::span<const std::uint64_t> patterns)
{
    Bytes output;
    for (const auto pattern : patterns) {
        append_u64(output, pattern);
    }
    return output;
}

template <std::size_t Size>
[[nodiscard]] inline Bytes from_u8_array(
    const std::array<std::uint8_t, Size>& values)
{
    Bytes output;
    output.reserve(Size);
    for (const auto value : values) {
        output.push_back(static_cast<std::byte>(value));
    }
    return output;
}

[[nodiscard]] inline std::unique_ptr<MemoryReader> memory_reader(
    Bytes bytes,
    const std::shared_ptr<ReaderControl>& control,
    bool canSeek = true,
    bool hasKnownSize = true,
    bool canRead = true)
{
    return std::make_unique<MemoryReader>(
        std::move(bytes), control, canSeek, hasKnownSize, canRead);
}

[[nodiscard]] inline core::FrameCount frame_count(std::int64_t value)
{
    auto count = core::FrameCount::create(value);
    Q_ASSERT(count.value() != nullptr);
    return *count.value();
}

[[nodiscard]] inline core::Result<audio::AudioBuffer> make_destination(
    const audio::WavStreamInfo& info,
    std::int64_t absoluteStart,
    std::int64_t frames)
{
    return audio::AudioBuffer::create(
        info.audio_format(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{absoluteStart},
        frame_count(frames));
}

[[nodiscard]] inline std::vector<std::uint64_t> channel_bits(
    const audio::AudioBuffer& buffer,
    std::size_t channel)
{
    auto plane = buffer.view().channel(channel);
    Q_ASSERT(plane.value() != nullptr);
    std::vector<std::uint64_t> bits;
    bits.reserve(plane.value()->size());
    for (const auto sample : *plane.value()) {
        bits.push_back(std::bit_cast<std::uint64_t>(sample));
    }
    return bits;
}

}  // namespace rgsml::tests::wav_support
