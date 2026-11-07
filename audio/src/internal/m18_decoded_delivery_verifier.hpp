#pragma once
// L1-M18 isolated post-quantization decoded-WAV verifier.
// This compares EXACT delivered binary64 decoded samples with the M18
// deterministic PCM24/Float32 mathematical contract, not the unquantized
// M17 PCM. Not a True Peak certificate, file hash or export permission.
#include "m18_terminal_sample_codec.hpp"
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/wav_format.hpp>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace rgsml::audio::internal {

enum class M18DecodedVerifyStatus : std::uint8_t {
    MATCH, INVALID_DOMAIN, INVALID_SHAPE, INVALID_ENCODING,
    INVALID_SEED, CODEC_REJECTED, SAMPLE_MISMATCH
};

struct M18DecodedVerifyResult final {
    M18DecodedVerifyStatus status{M18DecodedVerifyStatus::INVALID_SHAPE};
    std::uint64_t absolute_frame{0};
    std::uint32_t channel{0};
    [[nodiscard]] bool matched() const noexcept
    {return status==M18DecodedVerifyStatus::MATCH;}
};

[[nodiscard]] constexpr std::uint64_t m18_binary64_bits_from_f32_bits(
    std::uint32_t bits) noexcept
{
    const std::uint64_t sign=
        (bits&UINT32_C(0x80000000))!=0U ?UINT64_C(1)<<63:0U;
    const std::uint32_t exponent=(bits>>23)&UINT32_C(0xFF);
    const std::uint32_t fraction=bits&UINT32_C(0x7FFFFF);
    if (exponent==0U) {
        if (fraction==0U)return sign; // signed zero
        // An exact binary32 subnormal is still a NORMAL binary64 value.
        const unsigned top=31U-static_cast<unsigned>(
            std::countl_zero(fraction));
        const std::uint64_t doubleExp=
            static_cast<std::uint64_t>(static_cast<int>(top)-149+1023);
        const std::uint64_t doubleFrac=
            (static_cast<std::uint64_t>(fraction)<<(52U-top))
            &UINT64_C(0x000FFFFFFFFFFFFF);
        return sign|(doubleExp<<52)|doubleFrac;
    }
    // Caller already rejected NaN / +/-Infinity and any Float32 overflow.
    if (exponent==255U)return UINT64_C(0x7FF8000000000000);
    return sign
        |((static_cast<std::uint64_t>(exponent)+896U)<<52)
        |(static_cast<std::uint64_t>(fraction)<<29);
}

[[nodiscard]] inline M18DecodedVerifyResult m18_verify_decoded_delivery(
    audio::AudioBufferView authoritativeMaster,
    audio::AudioBufferView deliveredDecoded,
    audio::WavSampleFormat encoding,
    std::optional<std::uint64_t> ditherSeed)
{
    if (encoding!=audio::WavSampleFormat::PCM_S24
        &&encoding!=audio::WavSampleFormat::IEEE_F32)
        return {M18DecodedVerifyStatus::INVALID_ENCODING};
    const bool pcm=encoding==audio::WavSampleFormat::PCM_S24;
    if (pcm!=ditherSeed.has_value())
        return {M18DecodedVerifyStatus::INVALID_SEED};
    const auto m=authoritativeMaster;
    const auto d=deliveredDecoded;
    if (m.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
        ||d.timebase().frame_domain_id()
            !=audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return {M18DecodedVerifyStatus::INVALID_DOMAIN};
    const auto count=m.frame_count().value();
    const auto frameStart=m.absolute_start_frame().value();
    const auto channels=m.format().channel_count();
    if (frameStart<0 ||count<=0 ||d.absolute_start_frame()!=m.absolute_start_frame()
        ||d.frame_count()!=m.frame_count()
        ||m.format()!=d.format()
        ||m.timebase().sample_rate()!=m.format().sample_rate()
        ||d.timebase().sample_rate()!=d.format().sample_rate()
        ||(channels!=1U &&channels!=2U))
        return {M18DecodedVerifyStatus::INVALID_SHAPE};
    std::array<std::span<const double>,2> mPlanes{};
    std::array<std::span<const double>,2> dPlanes{};
    for(std::size_t ch=0;ch<channels;++ch) {
        const auto left=m.channel(ch),right=d.channel(ch);
        if (!left ||!right ||left.value()->size()!=static_cast<std::size_t>(count)
            ||right.value()->size()!=static_cast<std::size_t>(count))
            return {M18DecodedVerifyStatus::INVALID_SHAPE};
        mPlanes[ch]=*left.value();
        dPlanes[ch]=*right.value();
    }
    for(std::int64_t i=0;i<count;++i) {
        const auto abs=static_cast<std::uint64_t>(frameStart)
            +static_cast<std::uint64_t>(i);
        for(std::size_t ch=0;ch<channels;++ch) {
            const auto value=mPlanes[ch][static_cast<std::size_t>(i)];
            std::uint64_t expectedBits=0U;
            if(pcm) {
                const auto quantized=m18_quantize_pcm24(value,*ditherSeed,
                    abs,static_cast<std::uint32_t>(ch));
                if(quantized.status!=M18CodecStatus::OK)
                    return {M18DecodedVerifyStatus::CODEC_REJECTED,
                            abs,static_cast<std::uint32_t>(ch)};
                const double decoded=static_cast<double>(quantized.code)
                    /8388608.0;
                expectedBits=std::bit_cast<std::uint64_t>(decoded);
            } else {
                const auto converted=m18_convert_float32(value);
                if(converted.status!=M18CodecStatus::OK)
                    return {M18DecodedVerifyStatus::CODEC_REJECTED,
                            abs,static_cast<std::uint32_t>(ch)};
                expectedBits=m18_binary64_bits_from_f32_bits(converted.bits);
            }
            const auto actualBits=std::bit_cast<std::uint64_t>(
                dPlanes[ch][static_cast<std::size_t>(i)]);
            if(actualBits!=expectedBits)
                return {M18DecodedVerifyStatus::SAMPLE_MISMATCH,
                        abs,static_cast<std::uint32_t>(ch)};
        }
    }
    return {M18DecodedVerifyStatus::MATCH};
}
} // namespace rgsml::audio::internal
