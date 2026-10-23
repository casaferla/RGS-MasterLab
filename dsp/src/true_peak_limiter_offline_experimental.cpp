#include <rgsml/dsp/true_peak_limiter_offline_experimental.hpp>
#include "internal/experimental/true_peak_limiter_bounded_detector_feed.hpp"
#include "internal/experimental/true_peak_limiter_rolling_scheduler.hpp"
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::dsp {
namespace {
using core::Result;
using core::Error;
using core::ErrorCode;
using internal::TruePeakLimiterControllerState;
using internal::experimental::M17BoundedDetectorFeed;
using internal::experimental::M17RollingBatchSelector;
[[nodiscard]] Result<M17ExperimentalOfflineResult>
reject(ErrorCode code,const char* why) {
    return Result<M17ExperimentalOfflineResult>::failure(Error{code,why});
}
}
Result<M17ExperimentalOfflineResult>
render_m17_experimental_offline(audio::AudioBufferView input,
    TruePeakLimiterParameters params,
    const M17ExperimentalOfflineCancel& cancelled,
    const M17ExperimentalOfflineProgress& progress)
{
    if(cancelled && cancelled())return reject(ErrorCode::InvalidState,
        "M17 offline job cancelled");
    if(input.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
        ||input.frame_count().value()<=0
        ||input.absolute_start_frame().value()<0
        ||input.format().channel_count()<1
        ||input.format().channel_count()>2)
        return reject(ErrorCode::InvalidArgument,
            "M17 offline renderer requires complete OUTPUT_RATE PCM.");
    const auto n=input.frame_count().value();
    const auto origin=input.absolute_start_frame().value();
    const auto hz=input.format().sample_rate().value();
    const auto channels=input.format().channel_count();
    auto ctr=TruePeakLimiterControllerState::create(hz,params.ceiling_db_tp());
    auto feed=M17BoundedDetectorFeed::create(
        hz,params.ceiling_db_tp(),static_cast<std::uint32_t>(channels));
    auto selector=M17RollingBatchSelector::create(hz,params.ceiling_db_tp());
    if(!ctr || !feed || !selector)
        return reject(ErrorCode::InvalidArgument,
            "M17 offline accelerator cannot bind frozen contracts.");
    auto controller=std::move(*ctr.value());
    const auto h=controller.lookahead_ticks();
    const auto delay=controller.delay_frames();
    if(n>(std::numeric_limits<std::int64_t>::max()-h-128)/8
        ||origin>std::numeric_limits<std::int64_t>::max()-n-delay)
        return reject(ErrorCode::OutOfRange,
            "M17 offline source coordinate overflow.");
    for(std::size_t channel=0;channel<channels;++channel) {
        const auto lane=input.channel(channel);
        if(!lane)return reject(ErrorCode::InvalidArgument,
            "M17 offline source channel unavailable.");
        for(const double sample:*lane.value())
            if(!std::isfinite(sample))
                return reject(ErrorCode::InvalidAudioSample,
                    "M17 offline source contains nonfinite PCM.");
    }
    auto output=audio::AudioBuffer::create(input.format(),
        audio::FrameDomainId::OUTPUT_RATE,
        input.absolute_start_frame(),input.frame_count());
    if(!output)return Result<M17ExperimentalOfflineResult>::failure(
        *output.error());
    std::vector<TruePeakLimiterAppliedGainFrame> witness;
    witness.reserve(static_cast<std::size_t>(n));
    constexpr std::array<std::int64_t,4> partitions{64,31,113,257};
    std::size_t partIndex=0;
    std::int64_t sourcePos=0;
    for(std::int64_t begin=0;begin<n;) {
        if(cancelled && cancelled())return reject(ErrorCode::InvalidState,
            "M17 offline job retired without publishing PCM.");
        const auto stop=std::min(n,begin+partitions[partIndex++%partitions.size()]);
        const auto first=begin==0?-62:8*(begin-1)+1;
        const auto last=8*(stop-1);
        const auto deadline=last+h;
        feed->discard_before(first);
        while(!feed->drained() && feed->coverage_tick()<deadline) {
            if(sourcePos<n) {
                const auto a=input.channel(0);
                const auto b=channels==2?input.channel(1):a;
                if(!a||!b)return reject(ErrorCode::InvalidState,
                    "M17 offline input lane unavailable.");
                const double left=(*a.value())[static_cast<std::size_t>(sourcePos)];
                const double right=channels==2
                    ?(*b.value())[static_cast<std::size_t>(sourcePos)]
                    :+0.0;
                if(!feed->push(left,right))
                    return reject(ErrorCode::InvalidState,
                        "M17 offline detector could not accept frame.");
                ++sourcePos;
            } else if(!feed->drain())
                return reject(ErrorCode::InvalidState,
                    "M17 offline signed detector could not finalize.");
        }
        const auto events=feed->batch(first,last,h);
        if(!selector->load(events,first,last,
            feed->coverage_tick(),feed->drained()))
            return reject(ErrorCode::InvalidState,
                "M17 offline incomplete certified lookahead.");
        for(std::int64_t nFrame=begin;nFrame<stop;++nFrame) {
            double gain=1.0, reduction=+0.0;
            for(auto tick=nFrame==0?-62:8*(nFrame-1)+1;
                tick<=8*nFrame;++tick) {
                const auto current=feed->peak_at(tick);
                const auto choice=selector->choose(tick,current);
                if(!choice)return reject(ErrorCode::InvalidState,
                    "M17 offline selector rejected controller tick.");
                const auto& winning=choice->future_event;
                const auto future=winning
                    ?std::span<const internal::LimiterDetectorEvent>{
                        &*winning,1}
                    :std::span<const internal::LimiterDetectorEvent>{};
                const auto step=controller.advance(current,future);
                if(!step)return Result<M17ExperimentalOfflineResult>::failure(
                    *step.error());
                if(tick==8*nFrame) {
                    gain=step.value()->gain;
                    reduction=step.value()->reduction_db;
                }
            }
            for(std::size_t ch=0;ch<channels;++ch) {
                const auto src=input.channel(ch);
                const auto dst=output.value()->mutable_view().channel(ch);
                if(!src||!dst)return reject(ErrorCode::InvalidState,
                    "M17 offline output lane unavailable.");
                const double raw=(*src.value())[static_cast<std::size_t>(nFrame)];
                (*dst.value())[static_cast<std::size_t>(nFrame)]=
                    gain==1.0?raw:raw*gain;
            }
            witness.push_back({origin+nFrame,origin+delay+nFrame,
                reduction,gain});
        }
        begin=stop;
        if(progress)progress(begin,n);
    }
    if(sourcePos!=n || !feed->drained()
        ||selector->next_tick()!=8*(n-1)+1
        ||controller.next_tick()!=selector->next_tick()
        ||feed->high_water_events()>M17BoundedDetectorFeed::kMaxRetained
        ||selector->high_water_events()>M17RollingBatchSelector::kMaxBatchEvents
        ||witness.size()!=static_cast<std::size_t>(n))
        return reject(ErrorCode::InvalidState,
            "M17 offline source, EOS or bounded witness invariant failed.");
    if(cancelled && cancelled())return reject(ErrorCode::InvalidState,
        "M17 offline source retired before M16 verification.");
    return Result<M17ExperimentalOfflineResult>::success({
        std::move(*output.value()),std::move(witness),delay});
}
} // namespace rgsml::dsp
