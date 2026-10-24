#include <rgsml/audio/full_resource_output_rate_candidate.hpp>

#include "internal/playback_src_input.hpp"

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace rgsml::audio {
namespace {
[[nodiscard]] core::Error candidate_error(
    core::ErrorCode code, const char* category, const char* reason)
{
    return core::Error{code,reason,{{"category",category}}};
}

class ImmutablePcmInput final : public internal::PlaybackSrcInput {
public:
    explicit ImmutablePcmInput(AudioBufferView source) noexcept
      : source_(source) {}
    [[nodiscard]] const AudioFormat& format() const noexcept override
    { return source_.format(); }
    [[nodiscard]] core::FrameCount frame_count() const noexcept override
    { return source_.frame_count(); }
    [[nodiscard]] core::Result<core::FrameCount> read_frames(
        core::FrameIndex begin, MutableAudioBufferView destination) override
    {
        if(destination.timebase().frame_domain_id()
              !=FrameDomainId::SOURCE_PROCESSING_RATE
           || destination.absolute_start_frame()!=begin
           || destination.format()!=source_.format()
           || begin.value()<0) {
            return core::Result<core::FrameCount>::failure(candidate_error(
                core::ErrorCode::InvalidArgument,
                "M17_SRC_SOURCE_WINDOW_INVALID",
                "SRC source window metadata must match the immutable full PCM."));
        }
        auto in=source_.subview(begin,destination.frame_count());
        if(!in) return core::Result<core::FrameCount>::failure(*in.error());
        for(std::size_t ch=0;ch<source_.format().channel_count();++ch) {
            const auto from=in.value()->channel(ch);
            auto to=destination.channel(ch);
            if(!from||!to) return core::Result<core::FrameCount>::failure(
                candidate_error(core::ErrorCode::InvalidState,
                    "M17_SRC_CHANNEL_UNAVAILABLE",
                    "SRC source or destination lacks an expected channel."));
            std::copy(from.value()->begin(),from.value()->end(),to.value()->begin());
        }
        return core::Result<core::FrameCount>::success(destination.frame_count());
    }
private:
    AudioBufferView source_;
};
} // namespace

core::Result<FullResourceOutputRateCandidate>
materialize_full_resource_output_rate_candidate(
    AudioBufferView input, core::SampleRate output_rate, core::FrameCount block)
{
    using Candidate=FullResourceOutputRateCandidate;
    if(input.timebase().frame_domain_id()!=FrameDomainId::SOURCE_PROCESSING_RATE
       || input.absolute_start_frame().value()!=0
       || input.frame_count().value()<=0
       || block.value()<=0
       || block.value()>PlaybackSampleRateAdapter::kMaximumOutputBlockFrames) {
        return core::Result<Candidate>::failure(candidate_error(
            core::ErrorCode::InvalidArgument,
            "M17_SRC_FULL_RESOURCE_REQUIRED",
            "SRC requires complete zero-origin source PCM and bounded block size."));
    }
    for(std::size_t ch=0;ch<input.format().channel_count();++ch) {
        auto samples=input.channel(ch);
        if(!samples) return core::Result<Candidate>::failure(*samples.error());
        for(const auto v:*samples.value())
            if(!std::isfinite(v))
                return core::Result<Candidate>::failure(candidate_error(
                    core::ErrorCode::InvalidAudioSample,
                    "M17_SRC_NONFINITE_INPUT",
                    "Full-resource SRC refuses nonfinite PCM in any channel."));
    }
    const auto same=input.format().sample_rate()==output_rate;
    if(!same && !(
        (input.format().sample_rate().value()==44100
           && output_rate.value()==48000)
        || (input.format().sample_rate().value()==48000
           && output_rate.value()==44100))) {
        return core::Result<Candidate>::failure(candidate_error(
            core::ErrorCode::UnsupportedOperation,
            "M17_SRC_RATE_PAIR_UNSUPPORTED",
            "Only the frozen 44.1/48 kHz conversion pair is implemented."));
    }
    auto fmt=AudioFormat::create(output_rate,input.format().channel_layout());
    if(!fmt)return core::Result<Candidate>::failure(*fmt.error());

    if(same) {
        auto dest=AudioBuffer::create(*fmt.value(),FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},input.frame_count());
        if(!dest)return core::Result<Candidate>::failure(*dest.error());
        for(std::size_t ch=0;ch<input.format().channel_count();++ch) {
            auto src=input.channel(ch);
            auto dst=dest.value()->mutable_view().channel(ch);
            if(!src||!dst)return core::Result<Candidate>::failure(
                candidate_error(core::ErrorCode::InvalidState,
                    "M17_SRC_CHANNEL_UNAVAILABLE","A channel is unavailable."));
            std::copy(src.value()->begin(),src.value()->end(),dst.value()->begin());
        }
        return core::Result<Candidate>::success({
            std::move(*dest.value()),input.format().sample_rate(),output_rate,
            input.frame_count(),false,{}});
    }
    auto adapter=PlaybackSampleRateAdapter::create(PlaybackRateSpec{
        input.format().sample_rate(),output_rate,input.format().channel_layout(),
        input.frame_count()});
    if(!adapter)return core::Result<Candidate>::failure(*adapter.error());
    auto dest=AudioBuffer::create(*fmt.value(),FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},adapter.value()->output_frame_count());
    if(!dest)return core::Result<Candidate>::failure(*dest.error());
    ImmutablePcmInput ownedInput{input};
    const auto total=adapter.value()->output_frame_count().value();
    for(std::int64_t begin=0;begin<total;) {
        const auto length=std::min(block.value(),total-begin);
        auto count=core::FrameCount::create(length);
        if(!count)return core::Result<Candidate>::failure(*count.error());
        auto outputSlice=dest.value()->mutable_view().subview(
            core::FrameIndex{begin},*count.value());
        if(!outputSlice)return core::Result<Candidate>::failure(*outputSlice.error());
        auto wrote=internal::read_playback_src_frames(*adapter.value(),
            ownedInput,core::FrameIndex{begin},*outputSlice.value());
        if(!wrote)return core::Result<Candidate>::failure(*wrote.error());
        if(*wrote.value()!=*count.value())
            return core::Result<Candidate>::failure(candidate_error(
                core::ErrorCode::TruncatedAudioData,
                "M17_SRC_SHORT_OUTPUT",
                "Full-resource SRC returned an incomplete output block."));
        begin+=length;
    }
    return core::Result<Candidate>::success({
        std::move(*dest.value()),input.format().sample_rate(),output_rate,
        input.frame_count(),true,adapter.value()->kernel_sha256()});
}
} // namespace rgsml::audio
