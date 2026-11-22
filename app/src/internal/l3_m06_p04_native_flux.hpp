#pragma once
// Phase6 P04 authentic transient RAW_POSITIVE_SPECTRAL_FLUX producer.
// Unlike M20's 4096@48k STFT, P04 owns its distinct frozen 20ms
// Hann/5ms DIRECT RATIONAL 200Hz native grid. One additional FFT
// detector pass is required for this distinct algorithm, never a
// resampled M20 average or Grid100 shortcut.
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/analysis/fft.hpp>
#include <rgsml/analysis/hann.hpp>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
inline constexpr double kP04PositiveFluxFloorPower=1e-12;
struct P04RawNativeFrame final {
    std::uint64_t canonical_grid_ordinal{};
    std::uint64_t absolute_start_frame{};
    // P04 current-frame-center timestamp numerator / (2*fs),
    // not guessed sample-accurate onset or prior-frame timestamp.
    std::uint64_t event_center_twice_frames{};
    std::optional<double> raw_positive_spectral_flux;
};
enum class P04NativeReason:std::uint8_t {
    ready,invalid_or_partial_audio_source,unsupported_rate,
    invalid_geometry_or_numeric,no_complete_native_frames
};
struct P04RawFluxPopulation final {
    std::vector<P04RawNativeFrame> canonical;
    std::uint64_t sample_rate_hz{};
    std::uint64_t native_window_frames{};
    std::uint64_t canonical_fft_size{};
    P04NativeReason reason{P04NativeReason::invalid_or_partial_audio_source};
    [[nodiscard]] constexpr bool authorizes_d07_d09() const noexcept {
        return false;
    }
};
[[nodiscard]] inline std::optional<std::uint64_t>
p04_round_ties_even(std::uint64_t numerator,std::uint64_t denominator) {
    if(denominator==0U)return std::nullopt;
    const auto q=numerator/denominator,r=numerator%denominator;
    const auto greater=r>denominator/2U;
    const auto tie=denominator%2U==0U&&r==denominator/2U;
    return q+static_cast<std::uint64_t>(
        greater||(tie&&(q&1U)!=0U));
}
[[nodiscard]] inline P04RawFluxPopulation
measure_p04_native_positive_flux(const audio::AudioBufferView& pcm)
{
    P04RawFluxPopulation out;
    if(pcm.absolute_start_frame().value()!=0||
       pcm.frame_count().value()<=0||
       pcm.timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return out;
    const auto fs64=pcm.format().sample_rate().value();
    const auto channels=pcm.format().channel_count();
    if((fs64!=44100&&fs64!=48000&&fs64!=88200&&fs64!=96000&&
        fs64!=176400&&fs64!=192000)||channels<1U||channels>2U) {
        out.reason=P04NativeReason::unsupported_rate;return out;
    }
    const auto fs=static_cast<std::uint64_t>(fs64);
    const auto count=static_cast<std::uint64_t>(
        pcm.frame_count().value());
    const auto window=p04_round_ties_even(fs,50U);
    if(!window||*window==0U||
       count<*window) {
        out.reason=P04NativeReason::no_complete_native_frames;
        return out;
    }
    std::uint64_t nfft=1U;
    while(nfft<*window&&nfft<=16384U)nfft*=2U;
    if(nfft<*window||nfft>32768U) {
        out.reason=P04NativeReason::invalid_geometry_or_numeric;return out;
    }
    out.sample_rate_hz=fs;
    out.native_window_frames=*window;
    out.canonical_fft_size=nfft;
    const auto left=pcm.channel(0);
    if(!left)return out;
    std::span<const double> l=*left.value();
    std::span<const double> r;
    if(channels==2U) {
        const auto right=pcm.channel(1);
        if(!right)return out;
        r=*right.value();
    }
    if(l.size()!=count||(channels==2U&&r.size()!=count)) {
        out.reason=P04NativeReason::invalid_or_partial_audio_source;
        return out;
    }
    const auto hann=analysis::HannWindow::create(
        static_cast<std::size_t>(*window));
    analysis::Radix2Fft fft(static_cast<std::size_t>(nfft));
    std::vector<double> weighted(static_cast<std::size_t>(*window));
    std::vector<std::complex<double>> spectrum(
        static_cast<std::size_t>(nfft));
    std::vector<std::size_t> selected_bins;
    for(std::uint64_t k=0U;k<=nfft/2U;++k)
        if(k*fs>=40U*nfft&&k*fs<=16000U*nfft&&
           20U*k<=9U*nfft)
            selected_bins.push_back(static_cast<std::size_t>(k));
    if(selected_bins.empty()) {
        out.reason=P04NativeReason::invalid_geometry_or_numeric;return out;
    }
    std::vector<double> bins(selected_bins.size());
    std::vector<double> prev(selected_bins.size());
    bool previous_exists=false;
    // No sample-rate conversion, per-channel mean FFT POWER before
    // frozen sqrt / L2 normalization / positive-difference flux.
    for(std::uint64_t j=0U;;++j) {
        if(j>std::numeric_limits<std::uint64_t>::max()/fs) {
            out.reason=P04NativeReason::invalid_geometry_or_numeric;
            return out;
        }
        const auto start=p04_round_ties_even(j*fs,200U);
        if(!start||*start>count||count-*start<*window)break;
        std::fill(bins.begin(),bins.end(),0.0);
        for(std::size_t c=0;c<channels;++c) {
            const auto source=c==0U?l:r;
            for(std::size_t n=0;n<weighted.size();++n) {
                const auto v=source[static_cast<std::size_t>(*start)+n];
                if(!std::isfinite(v)) {
                    out.reason=P04NativeReason::invalid_geometry_or_numeric;
                    return out;
                }
                weighted[n]=v*hann.window[n];
            }
            fft.forward(weighted.data(),weighted.size(),spectrum.data());
            for(std::size_t i=0;i<selected_bins.size();++i) {
                bins[i]+=std::norm(spectrum[selected_bins[i]])/
                         static_cast<double>(channels);
            }
        }
        double total_power=0.0;
        for(auto& b:bins) {
            if(!std::isfinite(b)||b<0.0) {
                out.reason=P04NativeReason::invalid_geometry_or_numeric;
                return out;
            }
            b=std::sqrt(std::max(b,kP04PositiveFluxFloorPower));
            total_power+=b*b;
        }
        if(!std::isfinite(total_power)||total_power<=0.0) {
            out.reason=P04NativeReason::invalid_geometry_or_numeric;
            return out;
        }
        const volatile double positive_norm=std::max(
            std::sqrt(total_power),1e-12);
        double flux_squared=0.0;
        for(std::size_t k=0;k<bins.size();++k) {
            const auto u=bins[k]/positive_norm;
            if(previous_exists) {
                const auto diff=std::max(0.0,u-prev[k]);
                flux_squared+=diff*diff;
            }
            prev[k]=u;
        }
        if(!std::isfinite(flux_squared)) {
            out.reason=P04NativeReason::invalid_geometry_or_numeric;
            return out;
        }
        out.canonical.push_back({
            j,*start,2U*(*start)+(*window),
            previous_exists
                ?std::optional<double>{std::sqrt(flux_squared)}
                :std::nullopt});
        previous_exists=true;
    }
    out.reason=out.canonical.empty()
        ?P04NativeReason::no_complete_native_frames
        :P04NativeReason::ready;
    return out;
}
} // namespace rgsml::app::l3_automatch::m05m06_real
