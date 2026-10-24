#include <rgsml/audio/full_resource_output_rate_candidate.hpp>

#include "internal/playback_src_input.hpp"

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

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

// EXPERIMENTAL: six-rate offline-only rational FIR candidate. Frozen
// 44.1<->48 playback kernels/PCM oracles are dispatched separately below.
// Spectral, resource, cancellation and physical Gates remain outstanding.
[[nodiscard]] bool one_of_six(std::int64_t hz) noexcept {
    for(const auto allowed:{44'100,48'000,88'200,96'000,176'400,192'000})
        if(allowed==hz)return true;
    return false;
}
[[nodiscard]] std::int64_t reflect(std::int64_t index,
                                    std::int64_t frames) noexcept {
    if(frames==1)return 0;
    const auto period=2*(frames-1);
    index%=period;
    if(index<0)index+=period;
    return index<frames?index:period-index;
}
[[nodiscard]] core::Result<FullResourceOutputRateCandidate>
experimental_rational_output_rate(AudioBufferView input,
    core::SampleRate outputRate,core::FrameCount block) {
    using Candidate=FullResourceOutputRateCandidate;
    constexpr double pi=3.141592653589793238462643383279502884;
    const auto inHz=input.format().sample_rate().value();
    const auto outHz=outputRate.value();
    const auto inputFrames=input.frame_count().value();
    if(!one_of_six(inHz)||!one_of_six(outHz))
        return core::Result<Candidate>::failure(candidate_error(
            core::ErrorCode::UnsupportedOperation,"M17_SRC_RATE_PAIR_UNSUPPORTED",
            "Only six approved input/output sample rates are accepted."));
    if(inputFrames>(std::numeric_limits<std::int64_t>::max()/2)-1024)
        return core::Result<Candidate>::failure(candidate_error(
            core::ErrorCode::IntegerOverflow,"M17_SRC_FRAME_RANGE",
            "Rational SRC cannot safely reflect the full PCM resource."));
    // Ceil exact rational duration without multiplying whole-frame counts.
    const auto q=inputFrames/inHz;
    const auto r=inputFrames%inHz;
    const auto tail=(r*outHz+inHz-1)/inHz;
    if(q>(std::numeric_limits<std::int64_t>::max()-tail)/outHz)
        return core::Result<Candidate>::failure(candidate_error(
            core::ErrorCode::IntegerOverflow,"M17_SRC_OUTPUT_LENGTH",
            "Rational output-frame count overflows int64."));
    const auto total=q*outHz+tail;
    auto count=core::FrameCount::create(total);
    if(!count)return core::Result<Candidate>::failure(*count.error());
    auto format=AudioFormat::create(outputRate,input.format().channel_layout());
    if(!format)return core::Result<Candidate>::failure(*format.error());
    auto dest=AudioBuffer::create(*format.value(),FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*count.value());
    if(!dest)return core::Result<Candidate>::failure(*dest.error());

    // Taps scale with the decimation ratio: fixed 128 taps would NOT
    // adequately attenuate the alias band of 192 -> 48 kHz.
    const double ratio=std::max(1.0,static_cast<double>(inHz)/outHz);
    auto taps=static_cast<std::size_t>(std::ceil(128.0*ratio));
    taps+=taps%2;
    const auto left=static_cast<std::int64_t>(taps/2-1);
    const auto gcd=std::gcd(inHz,outHz);
    const auto phases=static_cast<std::size_t>(outHz/gcd);
    const auto stride=inHz/gcd;
    std::vector<double> coefficients(phases*taps);
    std::vector<double> window(taps);
    const double cutoff=0.94*std::min(1.0,
        static_cast<double>(outHz)/inHz);
    for(std::size_t k=0;k<taps;++k) {
        const double angle=2.0*pi*static_cast<double>(k)/(taps-1);
        window[k]=0.35875-0.48829*std::cos(angle)
            +0.14128*std::cos(2*angle)-0.01168*std::cos(3*angle);
    }
    for(std::size_t phase=0;phase<phases;++phase) {
        const double frac=static_cast<double>(phase)/phases;
        double sum=0.0;
        for(std::size_t k=0;k<taps;++k) {
            const double distance=static_cast<double>(
                static_cast<std::int64_t>(k)-left)-frac;
            const double x=pi*cutoff*distance;
            const double sinc=std::abs(x)<1e-12?1.0:std::sin(x)/x;
            const double value=cutoff*sinc*window[k];
            coefficients[phase*taps+k]=value;
            sum+=value;
        }
        if(!std::isfinite(sum)||std::abs(sum)<1e-12)
            return core::Result<Candidate>::failure(candidate_error(
                core::ErrorCode::InvalidState,"M17_SRC_KERNEL_UNNORMALIZED",
                "Rational FIR phase normalization failed."));
        for(std::size_t k=0;k<taps;++k)
            coefficients[phase*taps+k]/=sum;
    }
    for(std::size_t ch=0;ch<input.format().channel_count();++ch) {
        auto from=input.channel(ch);
        auto to=dest.value()->mutable_view().channel(ch);
        if(!from||!to)
            return core::Result<Candidate>::failure(candidate_error(
                core::ErrorCode::InvalidState,"M17_SRC_CHANNEL_UNAVAILABLE",
                "Rational offline SRC requires available mono/stereo PCM."));
        for(std::int64_t begin=0;begin<total;) {
            const auto length=std::min(block.value(),total-begin);
            for(std::int64_t index=begin;index<begin+length;++index) {
                const auto seconds=index/outHz;
                const auto remainder=index%outHz;
                const auto numerator=remainder*inHz;
                const auto base=seconds*inHz+numerator/outHz;
                const auto phase=static_cast<std::size_t>(
                    (index%static_cast<std::int64_t>(phases))
                    *stride%static_cast<std::int64_t>(phases));
                const auto* kernel=coefficients.data()+phase*taps;
                double value=0.0;
                for(std::size_t k=0;k<taps;++k) {
                    const auto at=reflect(base+
                        static_cast<std::int64_t>(k)-left,inputFrames);
                    value+=(*from.value())[static_cast<std::size_t>(at)]
                        *kernel[k];
                }
                if(!std::isfinite(value))
                    return core::Result<Candidate>::failure(candidate_error(
                        core::ErrorCode::InvalidAudioSample,
                        "M17_SRC_NONFINITE_OUTPUT",
                        "Rational SRC refuses nonfinite output PCM."));
                (*to.value())[static_cast<std::size_t>(index)]=value;
            }
            begin+=length;
        }
    }
    // No synthetic SHA-256 for a generated FIR; provenance is NOT certified.
    return core::Result<Candidate>::success({
        std::move(*dest.value()),input.format().sample_rate(),outputRate,
        input.frame_count(),true,{}});
}
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
        return experimental_rational_output_rate(input,output_rate,block);
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
