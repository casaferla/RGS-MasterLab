#include "internal/m18_candidate_delivery_validation.hpp"
#include "internal/sha256.hpp"
#include "m18_decoded_delivery_verifier.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::platform::windows::internal {
namespace {
template<class T>
[[nodiscard]] core::Result<T> reject(
    core::ErrorCode code,const char* message)
{
    return core::Result<T>::failure(core::Error{code,message});
}
} // namespace

core::Result<M18CandidateReadbackEvidence>
validate_m18_candidate_decoded_delivery(
    audio::WavReader& reader,
    audio::AudioBufferView verified_m17_master,
    audio::WavSampleFormat expected_encoding,
    std::optional<std::uint64_t> pcm24_dither_seed,
    double ceiling_db_tp)
{
    // Ceiling remains a validated binding to M17's accepted parameters.
    // It is NOT remeasured against the mathematically quantized M18 WAV.
    using R=core::Result<M18CandidateReadbackEvidence>;
    const bool pcm24=expected_encoding==audio::WavSampleFormat::PCM_S24;
    if ((!pcm24 &&expected_encoding!=audio::WavSampleFormat::IEEE_F32)
        ||pcm24!=pcm24_dither_seed.has_value()
        ||!std::isfinite(ceiling_db_tp)
        ||ceiling_db_tp< -12.0 ||ceiling_db_tp>0.0)
        return reject<M18CandidateReadbackEvidence>(
            core::ErrorCode::InvalidArgument,
            "M18_CANDIDATE_INVALID_ENCODING_SEED_OR_CEILING");

    const auto& info=reader.info();
    const auto full=verified_m17_master.frame_count().value();
    if (info.encoded_sample_format()!=expected_encoding
        ||info.audio_format()!=verified_m17_master.format()
        ||info.frame_count()!=verified_m17_master.frame_count()
        ||verified_m17_master.timebase().frame_domain_id()
            !=audio::FrameDomainId::OUTPUT_RATE
        ||verified_m17_master.absolute_start_frame()!=core::FrameIndex{0}
        ||full<=0)
        return reject<M18CandidateReadbackEvidence>(
            core::ErrorCode::MalformedAudioContainer,
            "M18_CANDIDATE_METADATA_OR_OWNER_INVALID");

    auto hasher=CanonicalDecodedAudioHasher::create(
        info.audio_format(),info.frame_count());
    if(!hasher)return R::failure(*hasher.error());
    constexpr std::int64_t kFrames=4096;
    std::int64_t offset=0;
    while(offset<full){
        const auto n=std::min(kFrames,full-offset);
        const auto count=core::FrameCount::create(n);
        if(!count)return R::failure(*count.error());
        auto decoded=audio::AudioBuffer::create(
            info.audio_format(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{offset},*count.value());
        if(!decoded)return R::failure(*decoded.error());

        const auto read=reader.read_frames(
            core::FrameIndex{offset},decoded.value()->mutable_view());
        if(!read)return R::failure(*read.error());
        if(*read.value()!=*count.value())
            return reject<M18CandidateReadbackEvidence>(
                core::ErrorCode::TruncatedAudioData,
                "M18_CANDIDATE_DECODED_WAV_TRUNCATED");

        const auto original=verified_m17_master.subview(
            core::FrameIndex{offset},*count.value());
        if(!original)return R::failure(*original.error());
        const auto exact=audio::internal::m18_verify_decoded_delivery(
            *original.value(),decoded.value()->view(),
            expected_encoding,pcm24_dither_seed);
        if(!exact.matched()){
            return R::failure(core::Error{
                exact.status==audio::internal::M18DecodedVerifyStatus::CODEC_REJECTED
                    ?core::ErrorCode::InvalidAudioSample
                    :core::ErrorCode::IoFailure,
                "M18_CANDIDATE_POSTQUANT_SAMPLE_MISMATCH",
                {{"absolute_frame",std::to_string(exact.absolute_frame)},
                 {"channel",std::to_string(exact.channel)},
                 {"status",std::to_string(
                    static_cast<unsigned>(exact.status))}}});
        }
        const auto hash=hasher.value()->update(decoded.value()->view());
        if(!hash)return R::failure(*hash.error());
        offset+=n;
    }

    const auto decodedSha=hasher.value()->finalize();
    if(!decodedSha)return R::failure(*decodedSha.error());
    return R::success(M18CandidateReadbackEvidence{
        info.container_kind(),expected_encoding,
        static_cast<std::uint64_t>(full),
        info.data_size_bytes(),*decodedSha.value()});
}
} // namespace rgsml::platform::windows::internal
