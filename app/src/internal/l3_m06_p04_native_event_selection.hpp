#pragma once
// Phase6 P04 raw flux -> independent canonical robust-MAD detector.
// ±1s baseline excludes current, >=20 valid neighbors; first raw
// flux remains absent. Local max ±15ms earlier tie; NMS by robustZ
// DESC then timestamp ASC, minimum 30ms, final chronological events.
#include "l3_m06_p04_native_flux.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
inline constexpr double kP04MadFloor=1e-6;
struct P04DetectedNativeEvent final {
    std::uint64_t canonical_source_frame_ordinal{};
    std::uint64_t frame_start{};
    std::uint64_t exact_center_twice_frames{};
    double onset_strength_raw_flux{};
    double robust_z{};
};
enum class P04SelectionReason:std::uint8_t {
    ready,unqualified_or_invalid_flux_population,invalid_numeric,
    cancelled
};
struct P04DetectedNativePopulation final {
    std::vector<P04DetectedNativeEvent> canonical_events;
    std::uint64_t canonical_flux_valid_frames{};
    std::uint64_t robust_z_available_frames{};
    P04SelectionReason reason{
        P04SelectionReason::unqualified_or_invalid_flux_population};
    [[nodiscard]] constexpr bool authorizes_m07() const noexcept {
        return false;
    }
};
[[nodiscard]] inline std::optional<double>
p04_type7_median(std::vector<double> observations)
{
    if(observations.empty())return std::nullopt;
    for(const auto v:observations)
        if(!std::isfinite(v))return std::nullopt;
    const auto mid=observations.size()/2U;
    std::nth_element(observations.begin(),
        observations.begin()+static_cast<std::ptrdiff_t>(mid),
        observations.end());
    const auto high=observations[mid];
    if(observations.size()%2U)return high;
    const auto low=*std::max_element(
        observations.begin(),
        observations.begin()+static_cast<std::ptrdiff_t>(mid));
    const double median=std::midpoint(low,high);
    return std::isfinite(median)?
        std::optional<double>{median}:std::nullopt;
}
[[nodiscard]] inline P04DetectedNativePopulation
select_p04_canonical_native_events(
    const P04RawFluxPopulation& native,
    const std::atomic<bool>* cancellation=nullptr)
{
    P04DetectedNativePopulation out;
    if(native.reason!=P04NativeReason::ready||
       native.sample_rate_hz==0U||
       native.canonical.size()<2U)
        return out;
    const auto fs=native.sample_rate_hz;
    const auto expected_window=p04_round_ties_even(fs,50U);
    if(!expected_window||native.native_window_frames!=*expected_window)
        return out;
    std::uint64_t expected_fft=1U;
    while(expected_fft<*expected_window&&expected_fft<=16384U)
        expected_fft*=2U;
    if(native.canonical_fft_size!=expected_fft)return out;
    const auto& frames=native.canonical;
    std::vector<std::optional<double>> z(frames.size());
    for(std::size_t i=0;i<frames.size();++i) {
        const auto& f=frames[i];
        if(i>std::numeric_limits<std::uint64_t>::max()/fs)
            return out;
        const auto exact_start=p04_round_ties_even(i*fs,200U);
        if(!exact_start||f.absolute_start_frame!=*exact_start)
            return out;
        if(f.canonical_grid_ordinal!=i||
           (i>0U&&!f.raw_positive_spectral_flux)||
           (i>0U&&
            f.absolute_start_frame<=frames[i-1U].absolute_start_frame)||
           f.event_center_twice_frames!=
                2U*f.absolute_start_frame+native.native_window_frames||
           (i==0U&&f.raw_positive_spectral_flux))
            return out;
        if(f.raw_positive_spectral_flux) {
            if(!std::isfinite(*f.raw_positive_spectral_flux)||
               *f.raw_positive_spectral_flux<0.0) {
                out.reason=P04SelectionReason::invalid_numeric;
                return out;
            }
            ++out.canonical_flux_valid_frames;
        }
    }
    // Direct 1s frame-center neighborhood on the canonical frame
    // starts; same Nw means center-time differences = start differences.
    for(std::size_t i=0;i<frames.size();++i) {
        if(cancellation&&cancellation->load(
             std::memory_order_acquire)) {
            out.canonical_events.clear();
            out.reason=P04SelectionReason::cancelled;
            return out;
        }
        if(!frames[i].raw_positive_spectral_flux)continue;
        std::vector<double> baseline;
        const auto origin=frames[i].absolute_start_frame;
        for(std::size_t j=i;j>0U;) {
            --j;
            if(origin-frames[j].absolute_start_frame>fs)break;
            if(frames[j].raw_positive_spectral_flux)
                baseline.push_back(*frames[j].raw_positive_spectral_flux);
        }
        for(std::size_t j=i+1U;j<frames.size();++j) {
            if(frames[j].absolute_start_frame-origin>fs)break;
            if(frames[j].raw_positive_spectral_flux)
                baseline.push_back(*frames[j].raw_positive_spectral_flux);
        }
        if(baseline.size()<20U)continue;
        const auto center=p04_type7_median(baseline);
        if(!center) {
            out.reason=P04SelectionReason::invalid_numeric;return out;
        }
        for(auto& value:baseline)value=std::abs(value-*center);
        const auto mad=p04_type7_median(std::move(baseline));
        if(!mad) {
            out.reason=P04SelectionReason::invalid_numeric;return out;
        }
        const double difference=*frames[i].raw_positive_spectral_flux-
                                *center;
        const double denom=*mad<kP04MadFloor
            ?kP04MadFloor:1.4826*(*mad);
        const volatile double checked_denominator=denom;
        const double observed=*mad<kP04MadFloor&&
            std::abs(difference)<=kP04MadFloor
            ?0.0:difference/checked_denominator;
        if(!std::isfinite(observed)) {
            out.reason=P04SelectionReason::invalid_numeric;return out;
        }
        z[i]=observed;
        ++out.robust_z_available_frames;
    }
    std::vector<std::size_t> local;
    for(std::size_t i=0;i<frames.size();++i) {
        if(cancellation&&cancellation->load(
             std::memory_order_acquire)) {
            out.canonical_events.clear();
            out.reason=P04SelectionReason::cancelled;
            return out;
        }
        if(!z[i]||*z[i]<6.0)continue;
        const auto origin=frames[i].absolute_start_frame;
        bool winner=true;
        const auto within=[](std::uint64_t a,std::uint64_t b,
                             std::uint64_t fs,std::uint64_t ms) {
            return 1000U*(a>b?a-b:b-a)<=ms*fs;
        };
        for(std::size_t j=i;j>0U;) {
            --j;
            if(!within(origin,frames[j].absolute_start_frame,fs,15U))
                break;
            if(z[j]&&*z[j]>=*z[i]) {winner=false;break;}
        }
        if(!winner)continue;
        for(std::size_t j=i+1U;j<frames.size();++j) {
            if(!within(origin,frames[j].absolute_start_frame,fs,15U))
                break;
            if(z[j]&&*z[j]>*z[i]) {winner=false;break;}
        }
        if(winner)local.push_back(i);
    }
    std::sort(local.begin(),local.end(),
        [&z](std::size_t a,std::size_t b){
            return *z[a]>*z[b]||
                (*z[a]==*z[b]&&a<b);
        });
    std::vector<std::size_t> accepted;
    for(auto idx:local) {
        bool too_close=false;
        for(auto earlier:accepted) {
            const auto a=frames[idx].absolute_start_frame;
            const auto b=frames[earlier].absolute_start_frame;
            if(1000U*(a>b?a-b:b-a)<30U*fs) {
                too_close=true;break;
            }
        }
        if(!too_close)accepted.push_back(idx);
    }
    std::sort(accepted.begin(),accepted.end());
    out.canonical_events.reserve(accepted.size());
    for(const auto i:accepted)
        out.canonical_events.push_back({
            frames[i].canonical_grid_ordinal,
            frames[i].absolute_start_frame,
            frames[i].event_center_twice_frames,
            *frames[i].raw_positive_spectral_flux,*z[i]});
    out.reason=P04SelectionReason::ready;
    return out;
}
} // namespace rgsml::app::l3_automatch::m05m06_real
