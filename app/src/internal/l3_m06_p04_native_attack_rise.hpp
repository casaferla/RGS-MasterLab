#pragma once
// L3 M06 P04 D09 authentic positive attack rise from native PCM.
// 1-ms causal RECTANGULAR mean-channel POWER envelope, sqrt,
// baseline median [-50,-5) ms, peak [0,30) ms, ascending
// 10%/90% threshold crossings on [-10,30) with linear interpolation.
// No new events, no fabricated history, no attack DSP knob identity.
#include "l3_m06_p04_real_event_descriptors.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
struct P04D09NativeResult final {
    std::vector<CanonicalTransientEvent> same_detector_event_population;
    bool exact_source_and_canonical_selection_current{};
    [[nodiscard]] constexpr bool authorizes_solver() const noexcept {
        return false;
    }
};
[[nodiscard]] inline P04D09NativeResult
derive_real_p04_d09_attack_rise(
    const audio::AudioBufferView& actual_pcm,
    const P04DetectedNativePopulation& detector,
    const P04RealDescriptorExtraction& d07_d08,
    bool exact_current_phase6_pcm_and_detector_epoch_attested)
{
    P04D09NativeResult out;
    if(!exact_current_phase6_pcm_and_detector_epoch_attested||
       detector.reason!=P04SelectionReason::ready||
       !d07_d08.source_valid||
       detector.canonical_events.size()!=
         d07_d08.complete_detector_population.size())
        return out;
    const auto fs=actual_pcm.format().sample_rate().value();
    const auto frames=actual_pcm.frame_count().value();
    const auto channels=actual_pcm.format().channel_count();
    if(fs<=0||frames<=0||(channels!=1U&&channels!=2U)||
       actual_pcm.absolute_start_frame().value()!=0||
       actual_pcm.timebase().frame_domain_id()!=
            audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return out;
    const auto l=actual_pcm.channel(0);
    if(!l)return out;
    std::span<const double> r;
    if(channels==2U) {
        const auto channel=actual_pcm.channel(1);
        if(!channel)return out;
        r=*channel.value();
    }
    const auto left=*l.value();
    if(left.size()!=static_cast<std::size_t>(frames)||
       (channels==2U&&r.size()!=left.size()))return out;
    const auto one_ms=p04_round_ties_even(
        static_cast<std::uint64_t>(fs),1000U);
    if(!one_ms||*one_ms==0U)return out;
    const auto power=[&](std::int64_t n)->double {
        const auto index=static_cast<std::size_t>(n);
        const double a=left[index];
        const double b=channels==2U?r[index]:0.0;
        return channels==2U?(a*a+b*b)/2.0:a*a;
    };
    out.same_detector_event_population=
        d07_d08.complete_detector_population;
    for(std::size_t i=0;i<out.same_detector_event_population.size();++i) {
        const auto& e=detector.canonical_events[i];
        auto& dest=out.same_detector_event_population[i];
        if(e.canonical_source_frame_ordinal!=
           dest.canonical_event_ordinal)
            return {};
        const auto at=[&](std::int64_t ms) {
            return p04_ms_frame_index_round_even(
                e.exact_center_twice_frames,ms,
                static_cast<std::uint64_t>(fs));
        };
        const auto p50=at(-50),p5=at(-5),m10=at(-10);
        const auto at0=at(0),p30=at(30);
        if(!p50||!p5||!m10||!at0||!p30)continue;
        const auto first=*p50;
        const auto last=*p30;
        const auto n1=static_cast<std::int64_t>(*one_ms);
        if(first-n1+1<0||last>frames||last<=*at0||
           *p5<=first||*m10<first||*at0<=*m10)
            continue;
        // O(window) bounded per event, O(1) mean update per sample.
        // The 1ms causal predecessor is genuine PCM, not zero padded.
        double sum=0.0;
        bool valid=true;
        for(auto n=first-n1+1;n<=first;++n) {
            const double p=power(n);
            if(!std::isfinite(p)) {valid=false;break;}
            sum+=p;
        }
        if(!valid)continue;
        std::vector<double> envelope;
        envelope.reserve(static_cast<std::size_t>(last-first));
        for(auto n=first;n<last;++n) {
            if(n>first) {
                const auto add=power(n);
                const auto drop=power(n-n1);
                if(!std::isfinite(add)||!std::isfinite(drop)) {
                    valid=false;break;
                }
                sum+=add-drop;
            }
            if(!std::isfinite(sum)||sum< -1e-12) {
                valid=false;break;
            }
            const double env=std::sqrt(std::max(0.0,sum)/
                                      static_cast<double>(n1));
            if(!std::isfinite(env)){valid=false;break;}
            envelope.push_back(env);
        }
        if(!valid||envelope.size()!=static_cast<std::size_t>(last-first))
            continue;
        std::vector<double> baseline;
        baseline.reserve(static_cast<std::size_t>(*p5-first));
        for(auto n=first;n<*p5;++n)
            baseline.push_back(
                envelope[static_cast<std::size_t>(n-first)]);
        const auto median=p04_type7_median(std::move(baseline));
        if(!median)continue;
        double peak=0.0;
        for(auto n=*at0;n<last;++n)
            peak=std::max(peak,
                envelope[static_cast<std::size_t>(n-first)]);
        if(!std::isfinite(peak)||peak<=*median)continue;
        const double t10=*median+0.1*(peak-*median);
        const double t90=*median+0.9*(peak-*median);
        const auto crossing=[&](double threshold)
            ->std::optional<double> {
            for(auto n=*m10+1;n<last;++n) {
                const auto previous=
                    envelope[static_cast<std::size_t>(n-1-first)];
                const auto current=
                    envelope[static_cast<std::size_t>(n-first)];
                if(previous<threshold&&current>=threshold&&
                   current>previous) {
                    const double proportion=
                        (threshold-previous)/(current-previous);
                    if(!std::isfinite(proportion)||proportion<0.0||
                       proportion>1.0)return std::nullopt;
                    return static_cast<double>(n-1)+proportion;
                }
            }
            return std::nullopt;
        };
        const auto a=crossing(t10),b=crossing(t90);
        if(!a||!b||*b<=*a)continue;
        const double rise_ms=1000.0*(*b-*a)/
            static_cast<double>(fs);
        if(std::isfinite(rise_ms)&&rise_ms>0.0)
            dest.attack_rise_time_ms=rise_ms;
    }
    out.exact_source_and_canonical_selection_current=true;
    return out;
}
} // namespace rgsml::app::l3_automatch::m05m06_real
