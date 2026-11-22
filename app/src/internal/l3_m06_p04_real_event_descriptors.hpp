#pragma once
// L3 M06 P04 native real PCM transient descriptor extraction.
// Original fixed event population survives edge-context holes.
// D07 early[0,30)ms vs sustain[80,200)ms RMS; D08 peak[0,30)
// minus local RMS[-50,200), no extra event or synthetic padding.
#include "l3_m06_p04_native_event_selection.hpp"
#include "l3_m06_native_transient_descriptors.hpp"
#include <rgsml/audio/audio_buffer_view.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
[[nodiscard]] inline std::optional<std::int64_t>
p04_ms_frame_index_round_even(
    std::uint64_t center_twice_frames,std::int64_t offset_ms,
    std::uint64_t sample_rate_hz) {
    if(center_twice_frames>
       static_cast<std::uint64_t>(
           std::numeric_limits<std::int64_t>::max()/1000)||
       sample_rate_hz>static_cast<std::uint64_t>(
           std::numeric_limits<std::int64_t>::max()/1000))
        return std::nullopt;
    const auto base=static_cast<std::int64_t>(
        center_twice_frames*1000U);
    const auto delta=2*offset_ms*
                     static_cast<std::int64_t>(sample_rate_hz);
    const auto n=base+delta;
    const auto positive=n>=0;
    const auto magnitude=static_cast<std::uint64_t>(
        positive?n:-n);
    const auto rounded=p04_round_ties_even(magnitude,2000U);
    if(!rounded)return std::nullopt;
    return positive?static_cast<std::int64_t>(*rounded):
        -static_cast<std::int64_t>(*rounded);
}
struct P04RealDescriptorProblem final {
    bool input_canonical_detector_and_source_attested{};
    std::uint64_t sample_rate_hz{};
};
struct P04RealDescriptorExtraction final {
    std::vector<CanonicalTransientEvent> complete_detector_population;
    bool source_valid{};
    [[nodiscard]] constexpr bool authorizes_reference_grade()
        const noexcept{return false;}
};
[[nodiscard]] inline P04RealDescriptorExtraction
derive_real_p04_d07_d08_contexts(
    const audio::AudioBufferView& actual_pcm,
    const P04DetectedNativePopulation& native_events,
    const P04RealDescriptorProblem& evidence)
{
    P04RealDescriptorExtraction result;
    const auto rate=actual_pcm.format().sample_rate().value();
    if(!evidence.input_canonical_detector_and_source_attested||
       native_events.reason!=P04SelectionReason::ready||
       rate<=0||
       static_cast<std::uint64_t>(rate)!=evidence.sample_rate_hz||
       actual_pcm.absolute_start_frame().value()!=0||
       actual_pcm.frame_count().value()<=0||
       actual_pcm.timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return result;
    const auto len=actual_pcm.frame_count().value();
    const auto channels=actual_pcm.format().channel_count();
    if(channels<1U||channels>2U)return result;
    const auto l=actual_pcm.channel(0);
    if(!l)return result;
    std::span<const double> right;
    if(channels==2U) {
        const auto r=actual_pcm.channel(1);
        if(!r)return result;
        right=*r.value();
    }
    const auto left=*l.value();
    if(left.size()!=static_cast<std::size_t>(len)||
       (channels==2U&&right.size()!=left.size()))
        return result;
    const auto power=[&](std::size_t index) {
        const double a=left[index];
        const double b=channels==2U?right[index]:0.0;
        return channels==2U?(a*a+b*b)/2.0:a*a;
    };
    const auto rms=[&](std::int64_t a,std::int64_t b)
        ->std::optional<double> {
        if(a<0||b<=a||b>len)return std::nullopt;
        double sum=0.0;
        for(auto i=a;i<b;++i) {
            const auto p=power(static_cast<std::size_t>(i));
            if(!std::isfinite(p))return std::nullopt;
            sum+=p;
        }
        const volatile double count=static_cast<double>(b-a);
        const auto average=sum/count;
        return std::isfinite(average)&&average>0.0?
            std::optional<double>{average}:std::nullopt;
    };
    result.complete_detector_population.reserve(
        native_events.canonical_events.size());
    std::optional<std::uint64_t> prior_ordinal;
    for(const auto& event:native_events.canonical_events) {
        if((prior_ordinal&&event.canonical_source_frame_ordinal<=
                          *prior_ordinal)||
           !std::isfinite(event.onset_strength_raw_flux)||
           !std::isfinite(event.robust_z)||
           event.onset_strength_raw_flux<0.0||
           event.robust_z<6.0)
            return {};
        prior_ordinal=event.canonical_source_frame_ordinal;
        const auto at=[&](std::int64_t offset) {
            return p04_ms_frame_index_round_even(
                event.exact_center_twice_frames,offset,
                evidence.sample_rate_hz);
        };
        const auto pre50=at(-50),early0=at(0),early30=at(30);
        const auto sustain80=at(80),sustain200=at(200);
        const auto before5=at(-5);
        CanonicalTransientEvent observed;
        observed.canonical_event_ordinal=
            event.canonical_source_frame_ordinal;
        observed.raw_positive_spectral_flux=
            event.onset_strength_raw_flux;
        observed.robust_z=event.robust_z;
        if(pre50&&early0&&early30&&sustain80&&sustain200&&before5) {
            const auto early=rms(*early0,*early30);
            const auto sustain=rms(*sustain80,*sustain200);
            if(early&&sustain) {
                const auto v=10.0*std::log10(*early/(*sustain));
                if(std::isfinite(v))
                    observed.early_minus_sustain_rms_db=v;
            }
            const auto local=rms(*pre50,*sustain200);
            if(local&&*early0>=0&&*early30>*early0&&
               *early30<=len) {
                double peak=0.0;
                bool finite=true;
                for(auto i=*early0;i<*early30;++i) {
                    const auto index=static_cast<std::size_t>(i);
                    const double a=std::abs(left[index]);
                    const double b=channels==2U?
                        std::abs(right[index]):0.0;
                    if(!std::isfinite(a)||!std::isfinite(b)) {
                        finite=false;break;
                    }
                    peak=std::max({peak,a,b});
                }
                if(finite&&peak>0.0) {
                    const auto crest=20.0*std::log10(peak)-
                                     10.0*std::log10(*local);
                    if(std::isfinite(crest))
                        observed.peak_minus_local_rms_db=crest;
                }
            }
        }
        result.complete_detector_population.push_back(observed);
    }
    result.source_valid=true;
    return result;
}
} // namespace rgsml::app::l3_automatch::m05m06_real
