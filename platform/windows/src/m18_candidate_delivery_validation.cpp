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
    double ceiling_db_tp,double delivery_gain)
{
    using R=core::Result<M18CandidateReadbackEvidence>;
    const bool pcm24=expected_encoding==audio::WavSampleFormat::PCM_S24;
    if ((!pcm24 &&expected_encoding!=audio::WavSampleFormat::IEEE_F32)
        ||pcm24!=pcm24_dither_seed.has_value()
        ||!std::isfinite(ceiling_db_tp)
        ||ceiling_db_tp< -12.0 ||ceiling_db_tp>0.0
        ||!std::isfinite(delivery_gain)
        ||delivery_gain<=0.0 ||delivery_gain>1.0)
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
    auto tp=analysis::M18DeliveredTruePeakCandidate::create(
        info.audio_format(),info.frame_count());
    if(!tp)return R::failure(*tp.error());

    constexpr std::int64_t kFrames=4096;
    std::int64_t offset=0;
    double maxAbsSampleError=0.0;
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
        // On a user-approved trim retry, verify the exact *trimmed*
        // postcodec signal; the retained original M17 PCM remains const.
        std::optional<audio::AudioBuffer> trimmed;
        audio::AudioBufferView expected=*original.value();
        if(delivery_gain!=1.0) {
            auto owned=audio::AudioBuffer::create(
                info.audio_format(),audio::FrameDomainId::OUTPUT_RATE,
                core::FrameIndex{offset},*count.value());
            if(!owned)return R::failure(*owned.error());
            auto target=owned.value()->mutable_view();
            for(std::size_t ch=0;ch<info.audio_format().channel_count();++ch) {
                const auto from=original.value()->channel(ch);
                const auto to=target.channel(ch);
                if(!from)return R::failure(*from.error());
                if(!to)return R::failure(*to.error());
                for(std::size_t i=0;i<static_cast<std::size_t>(n);++i)
                    (*to.value())[i]=(*from.value())[i]*delivery_gain;
            }
            trimmed.emplace(std::move(*owned.value()));
            expected=trimmed->view();
        }
        const auto exact=audio::internal::m18_verify_decoded_delivery(
            expected,decoded.value()->view(),
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
        for(std::size_t ch=0;ch<info.audio_format().channel_count();++ch) {
            const auto from=expected.channel(ch);
            const auto actual=decoded.value()->view().channel(ch);
            if(!from)return R::failure(*from.error());
            if(!actual)return R::failure(*actual.error());
            for(std::size_t i=0;i<static_cast<std::size_t>(n);++i)
                maxAbsSampleError=std::max(maxAbsSampleError,
                    std::abs((*from.value())[i]-(*actual.value())[i]));
        }
        const auto hash=hasher.value()->update(decoded.value()->view());
        if(!hash)return R::failure(*hash.error());
        const auto updateTp=tp.value()->get()->push_decoded(
            decoded.value()->view());
        if(!updateTp)return R::failure(*updateTp.error());
        offset+=n;
    }

    const auto decodedSha=hasher.value()->finalize();
    if(!decodedSha)return R::failure(*decodedSha.error());
    const auto measuredTp=tp.value()->get()->finalize();
    if(!measuredTp)return R::failure(*measuredTp.error());
    const auto& tpEvidence=*measuredTp.value();
    // Evidence only; no automatic trim, 'Export Anyway', or publication.
    // A strict nonconforming value must be handled by an explicit caller
    // policy and current-session consent, NOT silently clipped/rounded.
    const bool conforming=tpEvidence.is_exact_silence
        ||(tpEvidence.global_db_tp.has_value()
           &&*tpEvidence.global_db_tp<=ceiling_db_tp);
    return R::success(M18CandidateReadbackEvidence{
        info.container_kind(),expected_encoding,
        static_cast<std::uint64_t>(full),
        info.data_size_bytes(),*decodedSha.value(),tpEvidence,
        ceiling_db_tp,conforming,maxAbsSampleError});
}
} // namespace rgsml::platform::windows::internal
