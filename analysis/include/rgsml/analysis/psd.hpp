#ifndef RGSML_ANALYSIS_PSD_HPP
#define RGSML_ANALYSIS_PSD_HPP

#include <complex>
#include <cstddef>
#include <span>

namespace rgsml::analysis {

void compute_live_display_power(
    std::span<const std::complex<double>> channel0Fft,
    std::span<const std::complex<double>> channel1Fft,
    double sumW,
    std::span<double> outA2);

void compute_onesided_psd(
    std::span<const std::complex<double>> channelFft,
    double sampleRate,
    double sumW2,
    std::span<double> outPsd);

}  // namespace rgsml::analysis

#endif  // RGSML_ANALYSIS_PSD_HPP
