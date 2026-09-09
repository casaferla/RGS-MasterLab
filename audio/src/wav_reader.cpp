#include <rgsml/audio/wav_reader.hpp>

#include <rgsml/audio/audio_buffer.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::audio {
namespace {

using core::Error;
using core::ErrorCode;

[[nodiscard]] constexpr std::byte ascii(char value) noexcept
{
    return static_cast<std::byte>(static_cast<unsigned char>(value));
}

constexpr std::array<std::byte, 4> kRiff{
    ascii('R'), ascii('I'), ascii('F'), ascii('F')};
constexpr std::array<std::byte, 4> kRf64{
    ascii('R'), ascii('F'), ascii('6'), ascii('4')};
constexpr std::array<std::byte, 4> kRifx{
    ascii('R'), ascii('I'), ascii('F'), ascii('X')};
constexpr std::array<std::byte, 4> kWave{
    ascii('W'), ascii('A'), ascii('V'), ascii('E')};
constexpr std::array<std::byte, 4> kFmt{
    ascii('f'), ascii('m'), ascii('t'), ascii(' ')};
constexpr std::array<std::byte, 4> kData{
    ascii('d'), ascii('a'), ascii('t'), ascii('a')};
constexpr std::array<std::byte, 4> kDs64{
    ascii('d'), ascii('s'), ascii('6'), ascii('4')};

constexpr std::array<std::byte, 16> kPcmSubformat{
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x00},
    std::byte{0x80}, std::byte{0x00}, std::byte{0x00}, std::byte{0xaa},
    std::byte{0x00}, std::byte{0x38}, std::byte{0x9b}, std::byte{0x71}};
constexpr std::array<std::byte, 16> kFloatSubformat{
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x00},
    std::byte{0x80}, std::byte{0x00}, std::byte{0x00}, std::byte{0xaa},
    std::byte{0x00}, std::byte{0x38}, std::byte{0x9b}, std::byte{0x71}};

constexpr std::uint16_t kWaveFormatPcm = 0x0001U;
constexpr std::uint16_t kWaveFormatIeeeFloat = 0x0003U;
constexpr std::uint16_t kWaveFormatExtensible = 0xfffeU;
constexpr std::uint32_t kSpeakerFrontLeft = 0x00000001U;
constexpr std::uint32_t kSpeakerFrontRight = 0x00000002U;
constexpr std::uint32_t kSpeakerFrontCenter = 0x00000004U;

template <typename T>
[[nodiscard]] core::Result<T> failure(ErrorCode code, std::string_view message)
{
    return core::Result<T>::failure(Error{code, std::string(message)});
}

[[nodiscard]] bool fourcc_equals(
    std::span<const std::byte> value,
    const std::array<std::byte, 4>& expected) noexcept
{
    return value.size() >= expected.size()
        && std::equal(expected.begin(), expected.end(), value.begin());
}

[[nodiscard]] bool guid_equals(
    std::span<const std::byte> value,
    const std::array<std::byte, 16>& expected) noexcept
{
    return value.size() == expected.size()
        && std::equal(expected.begin(), expected.end(), value.begin());
}

[[nodiscard]] std::uint16_t read_u16(std::span<const std::byte> bytes) noexcept
{
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[0]))
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[1])) << 8U);
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::byte> bytes) noexcept
{
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4U; ++index) {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[index]))
            << (index * 8U);
    }
    return value;
}

[[nodiscard]] std::uint64_t read_u64(std::span<const std::byte> bytes) noexcept
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8U; ++index) {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[index]))
            << (index * 8U);
    }
    return value;
}

[[nodiscard]] core::Result<std::uint64_t> checked_add_u64(
    std::uint64_t left,
    std::uint64_t right)
{
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        return failure<std::uint64_t>(
            ErrorCode::IntegerOverflow, "Unsigned WAV offset addition overflowed.");
    }
    return core::Result<std::uint64_t>::success(left + right);
}

[[nodiscard]] core::Result<std::uint64_t> checked_multiply_u64(
    std::uint64_t left,
    std::uint64_t right)
{
    if (left != 0U && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return failure<std::uint64_t>(
            ErrorCode::IntegerOverflow, "Unsigned WAV offset multiplication overflowed.");
    }
    return core::Result<std::uint64_t>::success(left * right);
}

[[nodiscard]] core::Status read_exact_at(
    core::IResourceReader& source,
    std::uint64_t offset,
    std::span<std::byte> destination)
{
    auto seek = source.seek_bytes(offset);
    if (!seek) {
        if (seek.error()->code() == ErrorCode::OutOfRange) {
            return core::Status::failure(Error{
                ErrorCode::TruncatedAudioData,
                "WAV data ends before the requested offset."});
        }
        return core::Status::failure(*seek.error());
    }

    std::size_t transferred = 0;
    while (transferred < destination.size()) {
        auto result = source.read(destination.subspan(transferred));
        if (!result) {
            return core::Status::failure(*result.error());
        }
        if (*result.value() == 0U) {
            return core::Status::failure(
                Error{ErrorCode::TruncatedAudioData, "WAV data is truncated."});
        }
        transferred += *result.value();
    }
    return core::Status::success();
}

struct ParsedFormat final {
    WavSampleFormat sampleFormat;
    AudioFormat audioFormat;
    std::uint16_t blockAlign;
    std::uint16_t bytesPerSample;
    bool extensible;
    std::uint16_t validBits;
    std::uint32_t channelMask;
};

[[nodiscard]] core::Result<ParsedFormat> parse_format_chunk(
    std::span<const std::byte> bytes)
{
    if (bytes.size() != 16U && bytes.size() != 18U && bytes.size() != 40U) {
        return failure<ParsedFormat>(
            ErrorCode::MalformedAudioContainer,
            "WAV fmt chunk has an unsupported structural length.");
    }

    const auto formatTag = read_u16(bytes.subspan(0U, 2U));
    const auto channels = read_u16(bytes.subspan(2U, 2U));
    const auto sampleRateValue = read_u32(bytes.subspan(4U, 4U));
    const auto byteRate = read_u32(bytes.subspan(8U, 4U));
    const auto blockAlign = read_u16(bytes.subspan(12U, 2U));
    const auto containerBits = read_u16(bytes.subspan(14U, 2U));

    ChannelLayout layout = ChannelLayout::MONO_C;
    if (channels == 1U) {
        layout = ChannelLayout::MONO_C;
    } else if (channels == 2U) {
        layout = ChannelLayout::STEREO_LR;
    } else {
        return failure<ParsedFormat>(
            ErrorCode::UnsupportedAudioLayout,
            "WAV channel count is outside the mono/stereo v1 contract.");
    }

    bool extensible = false;
    std::uint16_t validBits = containerBits;
    std::uint32_t channelMask = channels == 1U
        ? kSpeakerFrontCenter
        : kSpeakerFrontLeft | kSpeakerFrontRight;
    std::span<const std::byte> subformat{};

    if (formatTag == kWaveFormatExtensible) {
        extensible = true;
        if (bytes.size() != 40U || read_u16(bytes.subspan(16U, 2U)) != 22U) {
            return failure<ParsedFormat>(
                ErrorCode::MalformedAudioContainer,
                "WAV extensible fmt extension is malformed.");
        }
        validBits = read_u16(bytes.subspan(18U, 2U));
        channelMask = read_u32(bytes.subspan(20U, 4U));
        subformat = bytes.subspan(24U, 16U);
        if ((channels == 1U && channelMask != kSpeakerFrontCenter)
            || (channels == 2U
                && channelMask != (kSpeakerFrontLeft | kSpeakerFrontRight))) {
            return failure<ParsedFormat>(
                ErrorCode::UnsupportedAudioLayout,
                "WAV extensible channel mask is ambiguous or incompatible.");
        }
    } else if (bytes.size() == 18U) {
        if (read_u16(bytes.subspan(16U, 2U)) != 0U) {
            return failure<ParsedFormat>(
                ErrorCode::MalformedAudioContainer,
                "WAV non-extensible fmt extension is malformed.");
        }
    } else if (bytes.size() != 16U) {
        return failure<ParsedFormat>(
            ErrorCode::MalformedAudioContainer,
            "Non-extensible WAV fmt contains unexpected extension bytes.");
    }

    WavSampleFormat sampleFormat = WavSampleFormat::PCM_S16;
    std::uint16_t bytesPerSample = 0U;
    const bool pcm = formatTag == kWaveFormatPcm
        || (formatTag == kWaveFormatExtensible && guid_equals(subformat, kPcmSubformat));
    const bool ieeeFloat = formatTag == kWaveFormatIeeeFloat
        || (formatTag == kWaveFormatExtensible && guid_equals(subformat, kFloatSubformat));

    if (pcm && containerBits == 16U) {
        sampleFormat = WavSampleFormat::PCM_S16;
        bytesPerSample = 2U;
    } else if (pcm && containerBits == 24U) {
        sampleFormat = WavSampleFormat::PCM_S24;
        bytesPerSample = 3U;
    } else if (pcm && containerBits == 32U) {
        sampleFormat = WavSampleFormat::PCM_S32;
        bytesPerSample = 4U;
    } else if (ieeeFloat && containerBits == 32U) {
        sampleFormat = WavSampleFormat::IEEE_F32;
        bytesPerSample = 4U;
    } else if (ieeeFloat && containerBits == 64U) {
        sampleFormat = WavSampleFormat::IEEE_F64;
        bytesPerSample = 8U;
    } else {
        return failure<ParsedFormat>(
            ErrorCode::UnsupportedAudioEncoding,
            "WAV sample encoding is outside the v1 contract.");
    }

    if (validBits != containerBits) {
        return failure<ParsedFormat>(
            ErrorCode::UnsupportedAudioEncoding,
            "Packed WAV valid-bit variants are unsupported.");
    }

    const auto expectedBlockAlign = static_cast<std::uint32_t>(channels)
        * static_cast<std::uint32_t>(bytesPerSample);
    const auto expectedByteRate = static_cast<std::uint64_t>(sampleRateValue)
        * expectedBlockAlign;
    if (blockAlign != expectedBlockAlign
        || expectedByteRate > std::numeric_limits<std::uint32_t>::max()
        || byteRate != expectedByteRate) {
        return failure<ParsedFormat>(
            ErrorCode::MalformedAudioContainer,
            "WAV byte rate or block alignment is inconsistent.");
    }

    auto sampleRate = core::SampleRate::create(static_cast<std::int64_t>(sampleRateValue));
    if (!sampleRate) {
        return failure<ParsedFormat>(
            ErrorCode::MalformedAudioContainer, "WAV sample rate is invalid.");
    }
    auto audioFormat = AudioFormat::create(*sampleRate.value(), layout);
    if (!audioFormat) {
        return core::Result<ParsedFormat>::failure(*audioFormat.error());
    }
    return core::Result<ParsedFormat>::success(ParsedFormat{
        sampleFormat,
        *audioFormat.value(),
        blockAlign,
        bytesPerSample,
        extensible,
        validBits,
        channelMask,
    });
}

[[nodiscard]] core::Result<double> decode_sample(
    WavSampleFormat format,
    std::span<const std::byte> bytes)
{
    switch (format) {
    case WavSampleFormat::PCM_S16: {
        const auto raw = read_u16(bytes);
        const auto code = raw < 0x8000U
            ? static_cast<std::int32_t>(raw)
            : static_cast<std::int32_t>(raw) - 0x10000;
        return core::Result<double>::success(std::ldexp(static_cast<double>(code), -15));
    }
    case WavSampleFormat::PCM_S24: {
        const auto raw = static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(bytes[0]))
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[1])) << 8U)
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[2])) << 16U);
        const auto code = raw < 0x00800000U
            ? static_cast<std::int32_t>(raw)
            : static_cast<std::int32_t>(raw) - 0x01000000;
        return core::Result<double>::success(std::ldexp(static_cast<double>(code), -23));
    }
    case WavSampleFormat::PCM_S32: {
        const auto raw = read_u32(bytes);
        const auto code = raw < 0x80000000U
            ? static_cast<std::int64_t>(raw)
            : static_cast<std::int64_t>(raw) - 0x100000000LL;
        return core::Result<double>::success(std::ldexp(static_cast<double>(code), -31));
    }
    case WavSampleFormat::IEEE_F32: {
        const auto value = std::bit_cast<float>(read_u32(bytes));
        if (!std::isfinite(value)) {
            return failure<double>(
                ErrorCode::InvalidAudioSample, "WAV contains a non-finite float sample.");
        }
        return core::Result<double>::success(static_cast<double>(value));
    }
    case WavSampleFormat::IEEE_F64: {
        const auto value = std::bit_cast<double>(read_u64(bytes));
        if (!std::isfinite(value)) {
            return failure<double>(
                ErrorCode::InvalidAudioSample, "WAV contains a non-finite float sample.");
        }
        return core::Result<double>::success(value);
    }
    }
    return failure<double>(
        ErrorCode::UnsupportedAudioEncoding, "Unknown WAV sample encoding.");
}

[[nodiscard]] std::size_t bytes_per_sample(WavSampleFormat format) noexcept
{
    switch (format) {
    case WavSampleFormat::PCM_S16:
        return 2U;
    case WavSampleFormat::PCM_S24:
        return 3U;
    case WavSampleFormat::PCM_S32:
    case WavSampleFormat::IEEE_F32:
        return 4U;
    case WavSampleFormat::IEEE_F64:
        return 8U;
    }
    return 0U;
}

}  // namespace

static_assert(std::numeric_limits<float>::is_iec559);
static_assert(std::numeric_limits<double>::is_iec559);
static_assert(sizeof(float) == 4U);
static_assert(sizeof(double) == 8U);

WavStreamInfo::WavStreamInfo(
    WavContainerKind containerKind,
    WavSampleFormat encodedSampleFormat,
    AudioFormat audioFormat,
    core::FrameCount frameCount,
    std::uint16_t blockAlignBytes,
    std::uint64_t dataOffsetBytes,
    std::uint64_t dataSizeBytes,
    bool extensible,
    std::uint16_t validBitsPerSample,
    std::uint32_t channelMask) noexcept
    : containerKind_(containerKind)
    , encodedSampleFormat_(encodedSampleFormat)
    , audioFormat_(audioFormat)
    , frameCount_(frameCount)
    , blockAlignBytes_(blockAlignBytes)
    , dataOffsetBytes_(dataOffsetBytes)
    , dataSizeBytes_(dataSizeBytes)
    , extensible_(extensible)
    , validBitsPerSample_(validBitsPerSample)
    , channelMask_(channelMask)
{
}

std::string_view WavStreamInfo::decoder_contract_version() const noexcept
{
    return kWavDecoderContractVersion;
}

std::string_view WavStreamInfo::source_conversion_version() const noexcept
{
    return kSourcePcmConversionVersion;
}

WavContainerKind WavStreamInfo::container_kind() const noexcept
{
    return containerKind_;
}

WavSampleFormat WavStreamInfo::encoded_sample_format() const noexcept
{
    return encodedSampleFormat_;
}

const AudioFormat& WavStreamInfo::audio_format() const noexcept
{
    return audioFormat_;
}

core::FrameCount WavStreamInfo::frame_count() const noexcept
{
    return frameCount_;
}

std::uint16_t WavStreamInfo::block_align_bytes() const noexcept
{
    return blockAlignBytes_;
}

std::uint64_t WavStreamInfo::data_offset_bytes() const noexcept
{
    return dataOffsetBytes_;
}

std::uint64_t WavStreamInfo::data_size_bytes() const noexcept
{
    return dataSizeBytes_;
}

bool WavStreamInfo::is_extensible() const noexcept
{
    return extensible_;
}

std::uint16_t WavStreamInfo::valid_bits_per_sample() const noexcept
{
    return validBitsPerSample_;
}

std::uint32_t WavStreamInfo::channel_mask() const noexcept
{
    return channelMask_;
}

core::Result<WavStreamInfo> WavReader::parse(core::IResourceReader& source)
{
    const auto capabilities = source.capabilities();
    if (!source.reference().permissions().can_read()) {
        return failure<WavStreamInfo>(
            ErrorCode::AccessDenied, "WAV decoding requires read permission.");
    }
    if (!capabilities.supports(core::ResourceCapability::CanSeek)) {
        return failure<WavStreamInfo>(
            ErrorCode::UnsupportedOperation,
            "WAV decoding requires a seekable or previously staged resource.");
    }

    std::optional<std::uint64_t> knownSize;
    if (capabilities.supports(core::ResourceCapability::HasKnownSize)) {
        auto size = source.size_bytes();
        if (!size) {
            return core::Result<WavStreamInfo>::failure(*size.error());
        }
        knownSize = *size.value();
    }

    std::array<std::byte, 12> header{};
    auto headerRead = read_exact_at(source, 0U, header);
    if (!headerRead) {
        return core::Result<WavStreamInfo>::failure(*headerRead.error());
    }
    const auto headerSpan = std::span<const std::byte>{header};
    if (fourcc_equals(headerSpan.first<4>(), kRifx)) {
        return failure<WavStreamInfo>(
            ErrorCode::UnsupportedAudioEncoding, "Big-endian RIFX is unsupported.");
    }
    if (!fourcc_equals(headerSpan.subspan<8U, 4U>(), kWave)) {
        return failure<WavStreamInfo>(
            ErrorCode::MalformedAudioContainer, "WAV form type is invalid.");
    }

    WavContainerKind containerKind = WavContainerKind::RIFF;
    std::uint64_t formEnd = 0U;
    std::uint64_t scanOffset = 12U;
    std::optional<std::uint64_t> rf64DataSize;
    std::optional<std::uint64_t> rf64SampleCount;
    bool seenDs64 = false;

    if (fourcc_equals(headerSpan.first<4>(), kRiff)) {
        const auto declaredSize = read_u32(headerSpan.subspan<4U, 4U>());
        if (declaredSize == std::numeric_limits<std::uint32_t>::max()) {
            return failure<WavStreamInfo>(
                ErrorCode::MalformedAudioContainer, "RIFF uses an unresolved size sentinel.");
        }
        auto end = checked_add_u64(8U, declaredSize);
        if (!end) {
            return core::Result<WavStreamInfo>::failure(*end.error());
        }
        formEnd = *end.value();
    } else if (fourcc_equals(headerSpan.first<4>(), kRf64)) {
        containerKind = WavContainerKind::RF64;
        if (read_u32(headerSpan.subspan<4U, 4U>())
            != std::numeric_limits<std::uint32_t>::max()) {
            return failure<WavStreamInfo>(
                ErrorCode::MalformedAudioContainer, "RF64 form size sentinel is missing.");
        }

        std::array<std::byte, 8> ds64Header{};
        auto ds64HeaderRead = read_exact_at(source, scanOffset, ds64Header);
        if (!ds64HeaderRead) {
            return core::Result<WavStreamInfo>::failure(*ds64HeaderRead.error());
        }
        const auto ds64HeaderSpan = std::span<const std::byte>{ds64Header};
        if (!fourcc_equals(ds64HeaderSpan.first<4>(), kDs64)
            || read_u32(ds64HeaderSpan.subspan<4U, 4U>()) != 28U) {
            return failure<WavStreamInfo>(
                ErrorCode::MalformedAudioContainer, "RF64 requires a canonical ds64 chunk.");
        }

        std::array<std::byte, 28> ds64Payload{};
        auto ds64Read = read_exact_at(source, 20U, ds64Payload);
        if (!ds64Read) {
            return core::Result<WavStreamInfo>::failure(*ds64Read.error());
        }
        const auto ds64Span = std::span<const std::byte>{ds64Payload};
        if (read_u32(ds64Span.subspan<24U, 4U>()) != 0U) {
            return failure<WavStreamInfo>(
                ErrorCode::MalformedAudioContainer,
                "RF64 ds64 tables are outside the v1 contract.");
        }
        auto end = checked_add_u64(8U, read_u64(ds64Span.subspan<0U, 8U>()));
        if (!end) {
            return core::Result<WavStreamInfo>::failure(*end.error());
        }
        formEnd = *end.value();
        rf64DataSize = read_u64(ds64Span.subspan<8U, 8U>());
        rf64SampleCount = read_u64(ds64Span.subspan<16U, 8U>());
        seenDs64 = true;
        scanOffset = 48U;
    } else {
        return failure<WavStreamInfo>(
            ErrorCode::MalformedAudioContainer,
            "Resource is not a RIFF/RF64 WAV container.");
    }

    if (formEnd < scanOffset) {
        return failure<WavStreamInfo>(
            ErrorCode::MalformedAudioContainer, "WAV form length is structurally invalid.");
    }
    if (knownSize && formEnd > *knownSize) {
        return failure<WavStreamInfo>(
            ErrorCode::TruncatedAudioData, "WAV form extends beyond the resource.");
    }

    std::optional<ParsedFormat> parsedFormat;
    std::optional<std::uint64_t> dataOffset;
    std::optional<std::uint64_t> dataSize;

    while (scanOffset < formEnd) {
        if (formEnd - scanOffset < 8U) {
            return failure<WavStreamInfo>(
                ErrorCode::MalformedAudioContainer,
                "WAV has trailing partial chunk metadata.");
        }

        std::array<std::byte, 8> chunkHeader{};
        auto chunkHeaderRead = read_exact_at(source, scanOffset, chunkHeader);
        if (!chunkHeaderRead) {
            return core::Result<WavStreamInfo>::failure(*chunkHeaderRead.error());
        }
        auto payloadOffsetResult = checked_add_u64(scanOffset, 8U);
        if (!payloadOffsetResult) {
            return core::Result<WavStreamInfo>::failure(*payloadOffsetResult.error());
        }
        const auto payloadOffset = *payloadOffsetResult.value();
        const auto chunkHeaderSpan = std::span<const std::byte>{chunkHeader};
        const auto size32 = read_u32(chunkHeaderSpan.subspan<4U, 4U>());
        const bool isData = fourcc_equals(chunkHeaderSpan.first<4>(), kData);
        std::uint64_t logicalSize = size32;
        if (size32 == std::numeric_limits<std::uint32_t>::max()) {
            if (containerKind != WavContainerKind::RF64 || !isData || !rf64DataSize) {
                return failure<WavStreamInfo>(
                    ErrorCode::MalformedAudioContainer,
                    "WAV chunk size sentinel is unresolved.");
            }
            logicalSize = *rf64DataSize;
        } else if (containerKind == WavContainerKind::RF64 && isData) {
            return failure<WavStreamInfo>(
                ErrorCode::MalformedAudioContainer, "RF64 data size sentinel is missing.");
        }

        auto chunkEndResult = checked_add_u64(payloadOffset, logicalSize);
        if (!chunkEndResult) {
            return core::Result<WavStreamInfo>::failure(*chunkEndResult.error());
        }
        auto paddedEndResult = checked_add_u64(*chunkEndResult.value(), logicalSize & 1U);
        if (!paddedEndResult) {
            return core::Result<WavStreamInfo>::failure(*paddedEndResult.error());
        }
        const auto paddedEnd = *paddedEndResult.value();
        if (paddedEnd > formEnd) {
            return failure<WavStreamInfo>(
                ErrorCode::MalformedAudioContainer,
                "WAV chunk crosses the declared form boundary.");
        }

        const auto chunkId = chunkHeaderSpan.first<4>();
        if (fourcc_equals(chunkId, kFmt)) {
            if (parsedFormat) {
                return failure<WavStreamInfo>(
                    ErrorCode::MalformedAudioContainer, "WAV contains duplicate fmt chunks.");
            }
            if (logicalSize > 64U) {
                return failure<WavStreamInfo>(
                    ErrorCode::MalformedAudioContainer,
                    "WAV fmt chunk exceeds the bounded v1 size.");
            }
            std::vector<std::byte> formatBytes;
            try {
                formatBytes.resize(static_cast<std::size_t>(logicalSize));
            } catch (const std::bad_alloc&) {
                return failure<WavStreamInfo>(
                    ErrorCode::IoFailure, "Unable to allocate bounded WAV metadata.");
            }
            auto formatRead = read_exact_at(source, payloadOffset, formatBytes);
            if (!formatRead) {
                return core::Result<WavStreamInfo>::failure(*formatRead.error());
            }
            auto format = parse_format_chunk(formatBytes);
            if (!format) {
                return core::Result<WavStreamInfo>::failure(*format.error());
            }
            parsedFormat = *format.value();
        } else if (isData) {
            if (dataOffset) {
                return failure<WavStreamInfo>(
                    ErrorCode::MalformedAudioContainer, "WAV contains duplicate data chunks.");
            }
            dataOffset = payloadOffset;
            dataSize = logicalSize;
        } else if (fourcc_equals(chunkId, kDs64)) {
            if (seenDs64) {
                return failure<WavStreamInfo>(
                    ErrorCode::MalformedAudioContainer, "WAV contains duplicate ds64 chunks.");
            }
            return failure<WavStreamInfo>(
                ErrorCode::MalformedAudioContainer,
                "ds64 is valid only as the RF64 leading chunk.");
        }

        scanOffset = paddedEnd;
    }

    if (!parsedFormat) {
        return failure<WavStreamInfo>(
            ErrorCode::MalformedAudioContainer, "WAV fmt chunk is missing.");
    }
    if (!dataOffset || !dataSize) {
        return failure<WavStreamInfo>(
            ErrorCode::MalformedAudioContainer, "WAV data chunk is missing.");
    }
    if (*dataSize % parsedFormat->blockAlign != 0U) {
        return failure<WavStreamInfo>(
            ErrorCode::MalformedAudioContainer,
            "WAV data is not an integral number of frames.");
    }
    const auto frameCountValue = *dataSize / parsedFormat->blockAlign;
    if (frameCountValue
        > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return failure<WavStreamInfo>(
            ErrorCode::IntegerOverflow, "WAV frame count exceeds the core frame domain.");
    }
    if (rf64SampleCount && *rf64SampleCount != frameCountValue) {
        return failure<WavStreamInfo>(
            ErrorCode::MalformedAudioContainer,
            "RF64 sample count is inconsistent with data size.");
    }
    auto frameCount = core::FrameCount::create(static_cast<std::int64_t>(frameCountValue));
    if (!frameCount) {
        return core::Result<WavStreamInfo>::failure(*frameCount.error());
    }

    return core::Result<WavStreamInfo>::success(WavStreamInfo{
        containerKind,
        parsedFormat->sampleFormat,
        parsedFormat->audioFormat,
        *frameCount.value(),
        parsedFormat->blockAlign,
        *dataOffset,
        *dataSize,
        parsedFormat->extensible,
        parsedFormat->validBits,
        parsedFormat->channelMask,
    });
}

core::Result<std::unique_ptr<WavReader>> WavReader::open(
    std::unique_ptr<core::IResourceReader> source)
{
    if (!source) {
        return failure<std::unique_ptr<WavReader>>(
            ErrorCode::InvalidArgument, "WAV source reader is required.");
    }
    auto info = parse(*source);
    if (!info) {
        return core::Result<std::unique_ptr<WavReader>>::failure(*info.error());
    }
    try {
        return core::Result<std::unique_ptr<WavReader>>::success(
            std::unique_ptr<WavReader>{
                new WavReader{std::move(source), *info.value()}});
    } catch (const std::bad_alloc&) {
        return failure<std::unique_ptr<WavReader>>(
            ErrorCode::IoFailure, "Unable to allocate WAV reader state.");
    }
}

WavReader::WavReader(
    std::unique_ptr<core::IResourceReader> source,
    WavStreamInfo info) noexcept
    : source_(std::move(source))
    , info_(info)
{
}

const WavStreamInfo& WavReader::info() const noexcept
{
    return info_;
}

core::Result<core::FrameCount> WavReader::read_frames(
    core::FrameIndex absoluteStart,
    MutableAudioBufferView destination)
{
    if (closed_) {
        return failure<core::FrameCount>(
            ErrorCode::InvalidState, "WAV reader is closed.");
    }
    if (absoluteStart.value() < 0
        || absoluteStart.value() > info_.frame_count().value()) {
        return failure<core::FrameCount>(
            ErrorCode::OutOfRange, "Requested WAV frame range is out of bounds.");
    }
    if (destination.absolute_start_frame() != absoluteStart
        || destination.format() != info_.audio_format()
        || destination.timebase().sample_rate() != info_.audio_format().sample_rate()
        || destination.timebase().frame_domain_id()
            != FrameDomainId::SOURCE_PROCESSING_RATE) {
        return failure<core::FrameCount>(
            ErrorCode::InvalidArgument, "WAV destination metadata is incoherent.");
    }

    const auto available = info_.frame_count().value() - absoluteStart.value();
    const auto requested = destination.frame_count().value();
    const auto framesToRead = std::min(available, requested);
    auto frameCount = core::FrameCount::create(framesToRead);
    if (!frameCount) {
        return core::Result<core::FrameCount>::failure(*frameCount.error());
    }
    if (framesToRead == 0) {
        return core::Result<core::FrameCount>::success(*frameCount.value());
    }

    auto staging = AudioBuffer::create(
        info_.audio_format(),
        FrameDomainId::SOURCE_PROCESSING_RATE,
        absoluteStart,
        *frameCount.value());
    if (!staging) {
        return core::Result<core::FrameCount>::failure(*staging.error());
    }
    auto stagingView = staging.value()->mutable_view();
    std::array<std::span<double>, 2> stagingChannels{};
    for (std::size_t channel = 0;
         channel < info_.audio_format().channel_count();
         ++channel) {
        auto plane = stagingView.channel(channel);
        if (!plane) {
            return core::Result<core::FrameCount>::failure(*plane.error());
        }
        stagingChannels[channel] = *plane.value();
    }

    constexpr std::int64_t kFramesPerBatch = 256;
    const auto channelCount = info_.audio_format().channel_count();
    const auto sampleBytes = bytes_per_sample(info_.encoded_sample_format());
    std::int64_t decodedFrames = 0;
    while (decodedFrames < framesToRead) {
        const auto batchFrames = std::min(
            framesToRead - decodedFrames, kFramesPerBatch);
        auto batchByteCount = checked_multiply_u64(
            static_cast<std::uint64_t>(batchFrames),
            info_.block_align_bytes());
        if (!batchByteCount) {
            return core::Result<core::FrameCount>::failure(*batchByteCount.error());
        }

        std::vector<std::byte> encoded;
        try {
            encoded.resize(static_cast<std::size_t>(*batchByteCount.value()));
        } catch (const std::bad_alloc&) {
            return failure<core::FrameCount>(
                ErrorCode::IoFailure,
                "Unable to allocate bounded WAV decode staging.");
        }

        auto firstFrame = checked_add_u64(
            static_cast<std::uint64_t>(absoluteStart.value()),
            static_cast<std::uint64_t>(decodedFrames));
        if (!firstFrame) {
            return core::Result<core::FrameCount>::failure(*firstFrame.error());
        }
        auto relativeByteOffset = checked_multiply_u64(
            *firstFrame.value(), info_.block_align_bytes());
        if (!relativeByteOffset) {
            return core::Result<core::FrameCount>::failure(*relativeByteOffset.error());
        }
        auto sourceOffset = checked_add_u64(
            info_.data_offset_bytes(), *relativeByteOffset.value());
        if (!sourceOffset) {
            return core::Result<core::FrameCount>::failure(*sourceOffset.error());
        }
        auto read = read_exact_at(*source_, *sourceOffset.value(), encoded);
        if (!read) {
            return core::Result<core::FrameCount>::failure(*read.error());
        }

        for (std::size_t localFrame = 0;
             localFrame < static_cast<std::size_t>(batchFrames);
             ++localFrame) {
            for (std::size_t channel = 0; channel < channelCount; ++channel) {
                const auto sampleIndex = localFrame * channelCount + channel;
                const auto sampleOffset = sampleIndex * sampleBytes;
                auto sample = decode_sample(
                    info_.encoded_sample_format(),
                    std::span<const std::byte>{encoded}.subspan(
                        sampleOffset, sampleBytes));
                if (!sample) {
                    return core::Result<core::FrameCount>::failure(*sample.error());
                }
                stagingChannels[channel][static_cast<std::size_t>(decodedFrames)
                    + localFrame] = *sample.value();
            }
        }
        decodedFrames += batchFrames;
    }

    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        auto destinationPlane = destination.channel(channel);
        if (!destinationPlane) {
            return core::Result<core::FrameCount>::failure(*destinationPlane.error());
        }
        std::copy(
            stagingChannels[channel].begin(),
            stagingChannels[channel].end(),
            destinationPlane.value()->begin());
    }
    return core::Result<core::FrameCount>::success(*frameCount.value());
}

core::Status WavReader::close()
{
    if (closed_) {
        return core::Status::success();
    }
    auto result = source_->close();
    if (!result) {
        return result;
    }
    closed_ = true;
    return core::Status::success();
}

}  // namespace rgsml::audio
