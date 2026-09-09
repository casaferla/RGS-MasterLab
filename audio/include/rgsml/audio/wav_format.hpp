#pragma once

#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>

#include <cstdint>
#include <string_view>

namespace rgsml::audio {

inline constexpr std::string_view kWavDecoderContractVersion =
    "rgsml.wav-decoder/1.0.0";
inline constexpr std::string_view kSourcePcmConversionVersion =
    "rgsml.source-pcm-conversion/1.0.0";

enum class WavSampleFormat {
    PCM_S16,
    PCM_S24,
    PCM_S32,
    IEEE_F32,
    IEEE_F64,
};

enum class WavContainerKind {
    RIFF,
    RF64,
};

class WavStreamInfo final {
public:
    [[nodiscard]] std::string_view decoder_contract_version() const noexcept;
    [[nodiscard]] std::string_view source_conversion_version() const noexcept;
    [[nodiscard]] WavContainerKind container_kind() const noexcept;
    [[nodiscard]] WavSampleFormat encoded_sample_format() const noexcept;
    [[nodiscard]] const AudioFormat& audio_format() const noexcept;
    [[nodiscard]] core::FrameCount frame_count() const noexcept;
    [[nodiscard]] std::uint16_t block_align_bytes() const noexcept;
    [[nodiscard]] std::uint64_t data_offset_bytes() const noexcept;
    [[nodiscard]] std::uint64_t data_size_bytes() const noexcept;
    [[nodiscard]] bool is_extensible() const noexcept;
    [[nodiscard]] std::uint16_t valid_bits_per_sample() const noexcept;
    [[nodiscard]] std::uint32_t channel_mask() const noexcept;

private:
    friend class WavReader;

    WavStreamInfo(
        WavContainerKind containerKind,
        WavSampleFormat encodedSampleFormat,
        AudioFormat audioFormat,
        core::FrameCount frameCount,
        std::uint16_t blockAlignBytes,
        std::uint64_t dataOffsetBytes,
        std::uint64_t dataSizeBytes,
        bool extensible,
        std::uint16_t validBitsPerSample,
        std::uint32_t channelMask) noexcept;

    WavContainerKind containerKind_;
    WavSampleFormat encodedSampleFormat_;
    AudioFormat audioFormat_;
    core::FrameCount frameCount_;
    std::uint16_t blockAlignBytes_;
    std::uint64_t dataOffsetBytes_;
    std::uint64_t dataSizeBytes_;
    bool extensible_;
    std::uint16_t validBitsPerSample_;
    std::uint32_t channelMask_;
};

}  // namespace rgsml::audio
