#include <rgsml/audio/wav_writer.hpp>
#include "internal/m18_wav_candidate.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::audio {
namespace {

using core::Error;
using core::ErrorCode;

constexpr std::uint64_t kMaximumLegacySize = 0xfffffffeULL;
constexpr std::uint64_t kRiffOverheadAfterSize = 50ULL;
constexpr std::uint64_t kRiffFileOverhead = 58ULL;
constexpr std::uint64_t kRf64FileOverhead = 94ULL;
constexpr std::size_t kSerializationFrames = 4096U;

template <typename T>
[[nodiscard]] core::Result<T> failure(ErrorCode code, std::string message)
{
    return core::Result<T>::failure(Error{code, std::move(message)});
}

[[nodiscard]] core::Status status_failure(ErrorCode code, std::string message)
{
    return core::Status::failure(Error{code, std::move(message)});
}

[[nodiscard]] core::Result<std::uint64_t> checked_add_u64(
    std::uint64_t left,
    std::uint64_t right,
    const char* message)
{
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        return failure<std::uint64_t>(ErrorCode::IntegerOverflow, message);
    }
    return core::Result<std::uint64_t>::success(left + right);
}

[[nodiscard]] core::Result<std::uint64_t> checked_multiply_u64(
    std::uint64_t left,
    std::uint64_t right,
    const char* message)
{
    if (left != 0U && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return failure<std::uint64_t>(ErrorCode::IntegerOverflow, message);
    }
    return core::Result<std::uint64_t>::success(left * right);
}

void append_fourcc(std::vector<std::byte>& bytes, const char (&value)[5])
{
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes.push_back(static_cast<std::byte>(
            static_cast<unsigned char>(value[index])));
    }
}

void append_u16(std::vector<std::byte>& bytes, std::uint16_t value)
{
    for (std::size_t index = 0; index < 2U; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xffU));
    }
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xffU));
    }
}

void append_u64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    for (std::size_t index = 0; index < 8U; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xffU));
    }
}

[[nodiscard]] core::Status write_all(
    core::IResourceWriter& destination,
    std::span<const std::byte> bytes)
{
    std::size_t written = 0;
    while (written < bytes.size()) {
        auto result = destination.write(bytes.subspan(written));
        if (!result) {
            return core::Status::failure(*result.error());
        }
        if (*result.value() == 0U) {
            return status_failure(
                ErrorCode::IoFailure,
                "wav_writer_zero_progress: non-empty write made no progress.");
        }
        if (*result.value() > bytes.size() - written) {
            return status_failure(
                ErrorCode::IoFailure,
                "wav_writer_invalid_progress: writer reported excess progress.");
        }
        written += *result.value();
    }
    return core::Status::success();
}

}  // namespace

core::Result<WavWriter::Layout> WavWriter::make_layout(
    const WavWriteSpec& spec, bool m18Mode)
{
    if (m18Mode) {
        // Reuse the M18 self-tested format, RIFF/RF64 and size plan.
        // This opt-in path cannot weaken the legacy F64 writer contract.
        if (spec.sample_format!=WavSampleFormat::PCM_S24
            &&spec.sample_format!=WavSampleFormat::IEEE_F32) {
            return failure<WavWriter::Layout>(
                ErrorCode::UnsupportedAudioEncoding,
                "wav_writer_m18_encoding_invalid: PCM24 or Float32 only.");
        }
        const auto fs=spec.audio_format.sample_rate().value();
        const auto frames=spec.frame_count.value();
        if (fs<=0 ||fs>static_cast<std::int64_t>(UINT32_MAX)
            ||frames<0) {
            return failure<WavWriter::Layout>(
                ErrorCode::InvalidArgument,
                "wav_writer_m18_rate_or_frames_invalid.");
        }
        const auto channels=spec.audio_format.channel_count();
        const auto channelLayout=spec.audio_format.channel_layout();
        if ((channelLayout!=ChannelLayout::MONO_C ||channels!=1U)
            &&(channelLayout!=ChannelLayout::STEREO_LR ||channels!=2U)) {
            return failure<WavWriter::Layout>(
                ErrorCode::UnsupportedAudioLayout,
                "wav_writer_m18_layout_invalid.");
        }
        const auto m18Plan=internal::m18_plan_wav({
            spec.sample_format==WavSampleFormat::PCM_S24
                ?internal::M18WavEncoding::PCM24
                :internal::M18WavEncoding::IEEE_FLOAT32,
            static_cast<std::uint32_t>(fs),
            static_cast<std::uint32_t>(channels),
            static_cast<std::uint64_t>(frames),0U});
        if (m18Plan.status!=internal::M18WavStatus::OK) {
            return failure<WavWriter::Layout>(
                m18Plan.status==internal::M18WavStatus::SIZE_OVERFLOW
                    ?ErrorCode::IntegerOverflow
                    :ErrorCode::UnsupportedAudioEncoding,
                "wav_writer_m18_plan_rejected: unsupported rate or size.");
        }
        const auto& p=m18Plan.plan;
        return core::Result<WavWriter::Layout>::success(WavWriter::Layout{
            p.rf64?WavContainerKind::RF64:WavContainerKind::RIFF,
            static_cast<std::uint16_t>(channels),p.block_align,
            p.spec.sample_rate,p.byte_rate,p.data_bytes,p.file_bytes});
    }
    if (spec.sample_format != WavSampleFormat::IEEE_F64) {
        return failure<WavWriter::Layout>(
            ErrorCode::UnsupportedAudioEncoding,
            "wav_writer_unsupported_encoding: L1-M09 supports IEEE_F64 only.");
    }

    const auto channels = spec.audio_format.channel_count();
    if ((spec.audio_format.channel_layout() != ChannelLayout::MONO_C || channels != 1U)
        && (spec.audio_format.channel_layout() != ChannelLayout::STEREO_LR || channels != 2U)) {
        return failure<WavWriter::Layout>(
            ErrorCode::UnsupportedAudioLayout,
            "wav_writer_unsupported_layout: only mono C and stereo L/R are supported.");
    }

    const auto sampleRate = spec.audio_format.sample_rate().value();
    if (sampleRate <= 0) {
        return failure<WavWriter::Layout>(
            ErrorCode::InvalidArgument,
            "wav_writer_invalid_sample_rate: sample rate must be positive.");
    }
    if (sampleRate > static_cast<std::int64_t>(
            std::numeric_limits<std::uint32_t>::max())) {
        return failure<WavWriter::Layout>(
            ErrorCode::IntegerOverflow,
            "wav_writer_byte_rate_overflow: sample rate does not fit the WAV fields.");
    }

    const auto frameCount = spec.frame_count.value();
    if (frameCount < 0) {
        return failure<WavWriter::Layout>(
            ErrorCode::InvalidArgument,
            "wav_writer_invalid_frame_count: frame count must not be negative.");
    }

    const auto blockAlign64 = static_cast<std::uint64_t>(channels) * 8ULL;
    const auto byteRate64 = static_cast<std::uint64_t>(sampleRate) * blockAlign64;
    if (blockAlign64 > std::numeric_limits<std::uint16_t>::max()
        || byteRate64 > std::numeric_limits<std::uint32_t>::max()) {
        return failure<WavWriter::Layout>(
            ErrorCode::IntegerOverflow,
            "wav_writer_byte_rate_overflow: WAV byte rate is not representable.");
    }

    auto dataBytes = checked_multiply_u64(
        static_cast<std::uint64_t>(frameCount),
        blockAlign64,
        "wav_writer_data_size_overflow: WAV payload size overflowed.");
    if (!dataBytes) {
        return core::Result<WavWriter::Layout>::failure(*dataBytes.error());
    }
    auto riffSize = checked_add_u64(
        kRiffOverheadAfterSize,
        *dataBytes.value(),
        "wav_writer_riff_size_overflow: RIFF size overflowed.");
    if (!riffSize) {
        return core::Result<WavWriter::Layout>::failure(*riffSize.error());
    }

    const bool useRiff = *dataBytes.value() <= kMaximumLegacySize
        && *riffSize.value() <= kMaximumLegacySize;
    const auto fileOverhead = useRiff ? kRiffFileOverhead : kRf64FileOverhead;
    auto fileSize = checked_add_u64(
        fileOverhead,
        *dataBytes.value(),
        "wav_writer_file_size_overflow: final WAV size overflowed.");
    if (!fileSize) {
        return core::Result<WavWriter::Layout>::failure(*fileSize.error());
    }

    return core::Result<WavWriter::Layout>::success(WavWriter::Layout{
        useRiff ? WavContainerKind::RIFF : WavContainerKind::RF64,
        static_cast<std::uint16_t>(channels),
        static_cast<std::uint16_t>(blockAlign64),
        static_cast<std::uint32_t>(sampleRate),
        static_cast<std::uint32_t>(byteRate64),
        *dataBytes.value(),
        *fileSize.value(),
    });
}

std::vector<std::byte> WavWriter::make_header(
    const WavWriteSpec& spec,
    const WavWriter::Layout& layout, bool m18Mode)
{
    if (m18Mode) {
        const auto p=internal::m18_plan_wav({
            spec.sample_format==WavSampleFormat::PCM_S24
                ?internal::M18WavEncoding::PCM24
                :internal::M18WavEncoding::IEEE_FLOAT32,
            static_cast<std::uint32_t>(spec.audio_format.sample_rate().value()),
            static_cast<std::uint32_t>(spec.audio_format.channel_count()),
            static_cast<std::uint64_t>(spec.frame_count.value()),0U});
        // A prior successful make_layout guarantees a valid M18 plan.
        const auto header=internal::m18_wav_header(p.plan);
        std::vector<std::byte> result;
        result.reserve(header.size());
        for (const auto b:header)result.push_back(static_cast<std::byte>(b));
        return result;
    }
    std::vector<std::byte> bytes;
    bytes.reserve(layout.container_kind == WavContainerKind::RIFF ? 58U : 94U);
    if (layout.container_kind == WavContainerKind::RIFF) {
        append_fourcc(bytes, "RIFF");
        append_u32(bytes, static_cast<std::uint32_t>(layout.file_size - 8U));
        append_fourcc(bytes, "WAVE");
    } else {
        append_fourcc(bytes, "RF64");
        append_u32(bytes, 0xffffffffU);
        append_fourcc(bytes, "WAVE");
        append_fourcc(bytes, "ds64");
        append_u32(bytes, 28U);
        append_u64(bytes, layout.file_size - 8U);
        append_u64(bytes, layout.data_bytes);
        append_u64(bytes, static_cast<std::uint64_t>(spec.frame_count.value()));
        append_u32(bytes, 0U);
    }

    append_fourcc(bytes, "fmt ");
    append_u32(bytes, 18U);
    append_u16(bytes, 3U);
    append_u16(bytes, layout.channel_count);
    append_u32(bytes, layout.sample_rate);
    append_u32(bytes, layout.byte_rate);
    append_u16(bytes, layout.block_align);
    append_u16(bytes, 64U);
    append_u16(bytes, 0U);

    append_fourcc(bytes, "fact");
    append_u32(bytes, 4U);
    const auto frames = static_cast<std::uint64_t>(spec.frame_count.value());
    append_u32(bytes, frames <= std::numeric_limits<std::uint32_t>::max()
            ? static_cast<std::uint32_t>(frames)
            : 0xffffffffU);

    append_fourcc(bytes, "data");
    append_u32(bytes, layout.container_kind == WavContainerKind::RIFF
            ? static_cast<std::uint32_t>(layout.data_bytes)
            : 0xffffffffU);
    return bytes;
}

namespace {

void encode_u64_le(std::uint64_t value, std::byte* destination) noexcept
{
    for (std::size_t index = 0; index < 8U; ++index) {
        destination[index] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
    }
}

}  // namespace

static_assert(std::numeric_limits<double>::is_iec559);
static_assert(sizeof(double) == 8U);

core::Result<std::unique_ptr<WavWriter>> WavWriter::open(
    std::unique_ptr<core::IResourceWriter> destination,
    WavWriteSpec spec)
{
    return open_impl(std::move(destination),std::move(spec),false,std::nullopt);
}

core::Result<std::unique_ptr<WavWriter>> WavWriter::open_m18(
    std::unique_ptr<core::IResourceWriter> destination,
    WavWriteSpec spec, std::optional<std::uint64_t> ditherSeed)
{
    return open_impl(std::move(destination),std::move(spec),true,ditherSeed);
}

core::Result<std::unique_ptr<WavWriter>> WavWriter::open_impl(
    std::unique_ptr<core::IResourceWriter> destination,
    WavWriteSpec spec, bool m18Mode,
    std::optional<std::uint64_t> ditherSeed)
{
    if (m18Mode) {
        if ((spec.sample_format==WavSampleFormat::PCM_S24
                &&!ditherSeed.has_value())
            ||(spec.sample_format==WavSampleFormat::IEEE_F32
                &&ditherSeed.has_value())) {
            return failure<std::unique_ptr<WavWriter>>(
                ErrorCode::InvalidArgument,
                "wav_writer_m18_seed_contract: PCM24 requires an exact seed; "
                "Float32 forbids a dither seed.");
        }
    } else if (ditherSeed.has_value()) {
        return failure<std::unique_ptr<WavWriter>>(
            ErrorCode::InvalidArgument,
            "wav_writer_f64_seed_forbidden.");
    }
    if (!destination) {
        return failure<std::unique_ptr<WavWriter>>(
            ErrorCode::InvalidArgument,
            "wav_writer_missing_destination: destination writer is required.");
    }
    const auto capabilities = destination->capabilities();
    if (!capabilities.supports(core::ResourceCapability::CanSeek)
        || !capabilities.supports(core::ResourceCapability::CanResize)
        || !capabilities.supports(core::ResourceCapability::CanFlush)) {
        return failure<std::unique_ptr<WavWriter>>(
            ErrorCode::UnsupportedOperation,
            "wav_writer_insufficient_capabilities: seek, resize, and flush are required.");
    }

    auto layout = make_layout(spec,m18Mode);
    if (!layout) {
        return core::Result<std::unique_ptr<WavWriter>>::failure(*layout.error());
    }

    auto resized = destination->resize_bytes(0U);
    if (!resized) {
        return core::Result<std::unique_ptr<WavWriter>>::failure(*resized.error());
    }
    auto seek = destination->seek_bytes(0U);
    if (!seek) {
        return core::Result<std::unique_ptr<WavWriter>>::failure(*seek.error());
    }

    try {
        const auto header = make_header(spec, *layout.value(),m18Mode);
        auto headerWrite = write_all(*destination, header);
        if (!headerWrite) {
            static_cast<void>(destination->close());
            return core::Result<std::unique_ptr<WavWriter>>::failure(*headerWrite.error());
        }
        return core::Result<std::unique_ptr<WavWriter>>::success(
            std::unique_ptr<WavWriter>{new WavWriter{
                std::move(destination), std::move(spec), *layout.value(),
                m18Mode,ditherSeed}});
    } catch (const std::bad_alloc&) {
        static_cast<void>(destination->close());
        return failure<std::unique_ptr<WavWriter>>(
            ErrorCode::IoFailure,
            "wav_writer_allocation_failed: unable to allocate deterministic writer state.");
    }
}

WavWriter::WavWriter(
    std::unique_ptr<core::IResourceWriter> destination,
    WavWriteSpec spec,
    Layout layout,bool m18Mode,
    std::optional<std::uint64_t> ditherSeed) noexcept
    : destination_(std::move(destination))
    , spec_(std::move(spec))
    , layout_(layout)
    , m18_mode_(m18Mode)
    , m18_dither_seed_(ditherSeed)
{
}

WavWriter::~WavWriter() noexcept
{
    if (!closed_ && destination_) {
        static_cast<void>(destination_->close());
    }
}

const WavWriteSpec& WavWriter::spec() const noexcept
{
    return spec_;
}

WavContainerKind WavWriter::container_kind() const noexcept
{
    return layout_.container_kind;
}

std::uint64_t WavWriter::expected_file_size_bytes() const noexcept
{
    return layout_.file_size;
}

core::FrameCount WavWriter::frames_written() const noexcept
{
    auto count = core::FrameCount::create(frames_written_);
    return *count.value();
}

core::Status WavWriter::write_frames(AudioBufferView source)
{
    if (closed_ || finalized_ || failed_) {
        return status_failure(
            ErrorCode::InvalidState,
            "wav_writer_invalid_state: writer cannot accept more frames.");
    }
    if (m18_mode_ &&
        (source.timebase().frame_domain_id()!=FrameDomainId::OUTPUT_RATE
         ||source.absolute_start_frame()!=core::FrameIndex{frames_written_})) {
        return status_failure(
            ErrorCode::InvalidFrameRange,
            "wav_writer_m18_master_frame_address_mismatch: expected contiguous "
            "absolute OUTPUT_RATE frames.");
    }
    if (source.format() != spec_.audio_format) {
        return status_failure(
            ErrorCode::InvalidArgument,
            "wav_writer_format_mismatch: source format differs from the declared spec.");
    }
    const auto incoming = source.frame_count().value();
    if (incoming > spec_.frame_count.value() - frames_written_) {
        return status_failure(
            ErrorCode::OutOfRange,
            "wav_writer_frame_count_exceeded: write exceeds the declared frame count.");
    }

    std::array<std::span<const double>, 2> channels{};
    for (std::size_t channel = 0; channel < layout_.channel_count; ++channel) {
        auto plane = source.channel(channel);
        if (!plane) {
            return core::Status::failure(*plane.error());
        }
        channels[channel] = *plane.value();
        for (const auto sample : channels[channel]) {
            if (!std::isfinite(sample)) {
                failed_ = true;
                return status_failure(
                    ErrorCode::InvalidAudioSample,
                    "wav_writer_invalid_audio_sample: source contains NaN or infinity.");
            }
        }
    }

    try {
        const auto frameCapacity = std::min<std::size_t>(
            kSerializationFrames,
            static_cast<std::size_t>(incoming));
        std::vector<std::byte> interleaved(
            frameCapacity * static_cast<std::size_t>(layout_.block_align));
        std::vector<double> m18Samples;
        if (m18_mode_)
            m18Samples.resize(frameCapacity*layout_.channel_count);
        std::size_t offset = 0;
        while (offset < static_cast<std::size_t>(incoming)) {
            const auto frames = std::min(
                frameCapacity,
                static_cast<std::size_t>(incoming) - offset);
            const auto byteCount = frames * static_cast<std::size_t>(layout_.block_align);
            if (m18_mode_) {
                for (std::size_t frame=0;frame<frames;++frame) {
                    for (std::size_t ch=0;ch<layout_.channel_count;++ch)
                        m18Samples[frame*layout_.channel_count+ch]
                            =channels[ch][offset+frame];
                }
                const auto samples=std::span<const double>{
                    m18Samples.data(),frames*layout_.channel_count};
                auto output=std::span<std::uint8_t>{
                    reinterpret_cast<std::uint8_t*>(interleaved.data()),byteCount};
                const auto first=static_cast<std::uint64_t>(frames_written_)
                    +static_cast<std::uint64_t>(offset);
                const auto codec=spec_.sample_format==WavSampleFormat::PCM_S24
                    ?internal::m18_pcm24_block(
                        samples,layout_.channel_count,first,
                        *m18_dither_seed_,output)
                    :internal::m18_float32_block(
                        samples,layout_.channel_count,first,output);
                if (codec!=internal::M18CodecStatus::OK) {
                    failed_=true; // no incomplete M18 output may finalize
                    return status_failure(
                        codec==internal::M18CodecStatus::FRAME_ADDRESS_OVERFLOW
                            ?ErrorCode::IntegerOverflow
                            :ErrorCode::InvalidAudioSample,
                        "wav_writer_m18_terminal_quantization_failed: "
                        "invalid layout/input, PCM24 overload or Float32 overflow.");
                }
            } else {
                std::size_t cursor = 0;
                for (std::size_t frame = 0; frame < frames; ++frame) {
                    for (std::size_t channel = 0; channel < layout_.channel_count; ++channel) {
                        encode_u64_le(
                            std::bit_cast<std::uint64_t>(channels[channel][offset + frame]),
                            interleaved.data() + cursor);
                        cursor += 8U;
                    }
                }
            }
            auto write = write_all(
                *destination_,
                std::span<const std::byte>{interleaved.data(), byteCount});
            if (!write) {
                failed_ = true;
                return write;
            }
            offset += frames;
        }
        frames_written_ += incoming;
        return core::Status::success();
    } catch (const std::bad_alloc&) {
        failed_ = true;
        return status_failure(
            ErrorCode::IoFailure,
            "wav_writer_allocation_failed: unable to allocate bounded interleave storage.");
    }
}

core::Status WavWriter::finalize()
{
    if (closed_ || finalized_ || failed_) {
        return status_failure(
            ErrorCode::InvalidState,
            "wav_writer_finalize_invalid_state: writer cannot be finalized.");
    }
    if (frames_written_ != spec_.frame_count.value()) {
        return status_failure(
            ErrorCode::InvalidState,
            "wav_writer_incomplete_frame_count: exact declared frame count is required.");
    }
    auto position = destination_->position_bytes();
    if (!position) {
        failed_ = true;
        return core::Status::failure(*position.error());
    }
    if (*position.value() != layout_.file_size) {
        failed_ = true;
        return status_failure(
            ErrorCode::InvalidState,
            "wav_writer_size_mismatch: emitted byte count differs from the preflight plan.");
    }
    auto resize = destination_->resize_bytes(layout_.file_size);
    if (!resize) {
        failed_ = true;
        return resize;
    }
    auto flush = destination_->flush();
    if (!flush) {
        failed_ = true;
        return flush;
    }
    finalized_ = true;
    return core::Status::success();
}

core::Status WavWriter::close()
{
    if (closed_) {
        return core::Status::success();
    }
    auto result = destination_->close();
    closed_ = true;
    if (!result) {
        failed_ = true;
    }
    return result;
}

}  // namespace rgsml::audio
