#pragma once
// L1-M18 private canonical WavReader candidate readback (NOT publication).
// Bounded 4096-frame decode, exact POST-DITHER/QUANTIZED sample comparison.
// No full-programme PCM reallocation and no audio/analysis dependency inversion.
#include "m18_decoded_delivery_verifier.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace rgsml::audio::internal {

struct M18DecodedWavCandidateEvidence final {
    WavContainerKind container_kind{WavContainerKind::RIFF};
    WavSampleFormat encoding{WavSampleFormat::PCM_S24};
    std::uint64_t frame_count{0};
    std::uint64_t data_bytes{0};
};

// A mathematical readback assertion, not an M16 TP measurement or a Windows
// file transaction. Calling code must verify source M17 owner/epoch first.
// A separately-owned Windows service controls file staging and commit.
[[nodiscard]] inline core::Result<M18DecodedWavCandidateEvidence>
m18_verify_decoded_wav_candidate(
    WavReader& reader, AudioBufferView authoritativeMaster,
    WavSampleFormat expectedEncoding,
    std::optional<std::uint64_t> seed)
{
    using R=core::Result<M18DecodedWavCandidateEvidence>;
    auto reject=[](core::ErrorCode code,const char* message)->R {
        return R::failure(core::Error{code,message});
    };
    const bool pcm=expectedEncoding==WavSampleFormat::PCM_S24;
    if ((expectedEncoding!=WavSampleFormat::PCM_S24
         &&expectedEncoding!=WavSampleFormat::IEEE_F32)
        ||pcm!=seed.has_value())
        return reject(core::ErrorCode::InvalidArgument,
            "M18_DECODED_READBACK_ENCODING_OR_SEED_INVALID");
    const auto& info=reader.info();
    const auto total=authoritativeMaster.frame_count().value();
    if (info.encoded_sample_format()!=expectedEncoding
        ||info.audio_format()!=authoritativeMaster.format()
        ||info.frame_count()!=authoritativeMaster.frame_count()
        ||authoritativeMaster.timebase().frame_domain_id()
            !=FrameDomainId::OUTPUT_RATE
        ||authoritativeMaster.absolute_start_frame()!=core::FrameIndex{0}
        ||total<=0)
        return reject(core::ErrorCode::MalformedAudioContainer,
            "M18_DECODED_READBACK_METADATA_OR_MASTER_MISMATCH");
    constexpr std::int64_t kChunk=4096;
    std::int64_t offset=0;
    while(offset<total){
        const auto n=std::min(kChunk,total-offset);
        const auto length=core::FrameCount::create(n);
        if (!length)return R::failure(*length.error());
        auto decoded=AudioBuffer::create(
            info.audio_format(),FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{offset},*length.value());
        if (!decoded)return R::failure(*decoded.error());
        auto got=reader.read_frames(core::FrameIndex{offset},
            decoded.value()->mutable_view());
        if (!got)return R::failure(*got.error());
        if (*got.value()!=*length.value())
            return reject(core::ErrorCode::TruncatedAudioData,
                "M18_DECODED_READBACK_TRUNCATED");
        const auto masterChunk=authoritativeMaster.subview(
            core::FrameIndex{offset},*length.value());
        if(!masterChunk)return R::failure(*masterChunk.error());
        const auto compare=m18_verify_decoded_delivery(
            *masterChunk.value(),decoded.value()->view(),expectedEncoding,seed);
        if(!compare.matched()){
            return R::failure(core::Error{
                compare.status==M18DecodedVerifyStatus::CODEC_REJECTED
                    ?core::ErrorCode::InvalidAudioSample
                    :core::ErrorCode::IoFailure,
                "M18_DECODED_READBACK_EXACT_SAMPLE_MISMATCH",
                {{"absolute_frame",std::to_string(compare.absolute_frame)},
                 {"channel",std::to_string(compare.channel)},
                 {"status",std::to_string(
                    static_cast<unsigned>(compare.status))}}});
        }
        offset+=n;
    }
    return R::success(M18DecodedWavCandidateEvidence{
        info.container_kind(),expectedEncoding,
        static_cast<std::uint64_t>(total),info.data_size_bytes()});
}
} // namespace rgsml::audio::internal
