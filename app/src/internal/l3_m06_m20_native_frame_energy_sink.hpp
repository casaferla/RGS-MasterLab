#pragma once
#include "l3_native_seed_measurement_carriers.hpp"
// L3-M06 native D10-D12 E[t,b] producer on the ACTUAL M20 FFT pass.
// Uses M20's optional selected-frame PSD sink, no independent FFT or
// time-averaged PSD reconstruction. Owner/canonical grid/gate are
// immutable externally supplied Phase6 authorities, never inferred.
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
class AuthenticM20FrameEnergySink final:
    public analysis::TonalStereoSelectedFrameSink {
public:
    AuthenticM20FrameEnergySink(
        std::uint32_t sample_rate_hz,
        std::uint64_t exact_native_fft_size,
        std::vector<NativeFrameBand> exact_common_canonical_grid,
        bool current_phase6_band_grid_frame_policy_attested)
        :fs_(sample_rate_hz),nfft_(exact_native_fft_size),
         bands_(std::move(exact_common_canonical_grid)),
         current_(current_phase6_band_grid_frame_policy_attested)
    {
        if(!current_||fs_<44100U||fs_>192000U||nfft_<2U||
           (nfft_&(nfft_-1U))!=0U||bands_.empty())
            valid_=false;
        const double nyquist=static_cast<double>(fs_)/2.0;
        for(std::size_t i=0;i<bands_.size();++i) {
            const auto& band=bands_[i];
            if(!std::isfinite(band.low_hz)||
               !std::isfinite(band.high_hz)||
               band.low_hz<20.0||band.high_hz<=band.low_hz||
               band.high_hz>std::min(20000.0,0.45*fs_)||
               band.high_hz>nyquist||
               (i>0U&&(band.exact_canonical_index<=
                         bands_[i-1U].exact_canonical_index||
                         band.low_hz<bands_[i-1U].high_hz)))
                valid_=false;
        }
    }
    [[nodiscard]] bool on_selected_native_frame(
        std::uint64_t frame,
        std::uint32_t fs,
        std::uint64_t fft,
        std::span<const double> psd) override
    {
        if(!valid_||fs!=fs_||fft!=nfft_||
           psd.size()!=static_cast<std::size_t>(nfft_/2U+1U)||
           (prior_frame_&&frame<=*prior_frame_)) {
            valid_=false;
            return false;
        }
        std::vector<std::optional<double>> row;
        row.reserve(bands_.size());
        const double nyquist=static_cast<double>(fs_)/2.0;
        const double step=static_cast<double>(fs_)/static_cast<double>(nfft_);
        for(const auto& band:bands_) {
            double E=0.0;
            for(std::size_t k=0;k<psd.size();++k) {
                const double value=psd[k];
                if(!std::isfinite(value)||value<0.0) {
                    valid_=false;
                    return false;
                }
                const double f=static_cast<double>(k)*step;
                const double a=std::max({0.0,band.low_hz,f-step/2.0});
                const double b=std::min({nyquist,band.high_hz,f+step/2.0});
                if(b<=a)continue;
                E+=value*(b-a);
            }
            if(!std::isfinite(E)||E<0.0) {
                valid_=false;
                return false;
            }
            row.push_back(E);
        }
        original_stft_ordinals_.push_back(frame);
        native_frame_energies_.push_back(std::move(row));
        prior_frame_=frame;
        return true;
    }
    [[nodiscard]] bool valid() const noexcept{return valid_&&current_;}
    [[nodiscard]] const auto& canonical_original_ordinals() const noexcept {
        return original_stft_ordinals_;
    }
    [[nodiscard]] const auto& exact_e_tb() const noexcept {
        return native_frame_energies_;
    }
    [[nodiscard]] const auto& canonical_grid() const noexcept {return bands_;}
    [[nodiscard]] auto take_native_energies() && noexcept {return std::move(native_frame_energies_);}
    [[nodiscard]] std::size_t configured_bands() const noexcept{
        return bands_.size();
    }
    [[nodiscard]] constexpr bool authorizes_m10() const noexcept {
        return false;
    }
private:
    std::uint32_t fs_{};
    std::uint64_t nfft_{};
    std::vector<NativeFrameBand> bands_;
    bool current_{},valid_{true};
    std::optional<std::uint64_t> prior_frame_;
    std::vector<std::uint64_t> original_stft_ordinals_;
    std::vector<std::vector<std::optional<double>>> native_frame_energies_;
};
} // namespace rgsml::app::l3_automatch::m05m06_real
