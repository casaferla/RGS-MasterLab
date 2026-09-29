#include <rgsml/analysis/psd.hpp>

#include <algorithm>
#include <cmath>

namespace rgsml::analysis {

void compute_live_display_power(
    std::span<const std::complex<double>> channel0Fft,
    std::span<const std::complex<double>> channel1Fft,
    double sumW,
    std::span<double> outA2)
{
    const std::size_t fftSize = channel0Fft.size();
    const std::size_t halfBins = fftSize / 2 + 1;
    const double invSumW2 = 1.0 / (sumW * sumW);
    const bool isStereo = !channel1Fft.empty() && channel1Fft.size() == fftSize;

    for (std::size_t k = 0; k < halfBins; ++k) {
        const double mag2_c0 = std::norm(channel0Fft[k]);
        double a2_c0 = 0.0;
        if (k == 0 || k == fftSize / 2) {
            a2_c0 = mag2_c0 * invSumW2;
        } else {
            a2_c0 = 4.0 * mag2_c0 * invSumW2;
        }

        if (isStereo) {
            const double mag2_c1 = std::norm(channel1Fft[k]);
            double a2_c1 = 0.0;
            if (k == 0 || k == fftSize / 2) {
                a2_c1 = mag2_c1 * invSumW2;
            } else {
                a2_c1 = 4.0 * mag2_c1 * invSumW2;
            }
            outA2[k] = (a2_c0 + a2_c1) * 0.5;
        } else {
            outA2[k] = a2_c0;
        }
    }
}

void compute_onesided_psd(
    std::span<const std::complex<double>> channelFft,
    double sampleRate,
    double sumW2,
    std::span<double> outPsd)
{
    const std::size_t fftSize = channelFft.size();
    const std::size_t halfBins = fftSize / 2 + 1;
    const double norm = 1.0 / (sampleRate * sumW2);

    for (std::size_t k = 0; k < halfBins; ++k) {
        const double mag2 = std::norm(channelFft[k]);
        if (k == 0 || k == fftSize / 2) {
            outPsd[k] = mag2 * norm;
        } else {
            outPsd[k] = 2.0 * mag2 * norm;
        }
    }
}

}  // namespace rgsml::analysis
