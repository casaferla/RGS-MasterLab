#include <rgsml/analysis/spectrum_config.hpp>

#include <cmath>

namespace rgsml::analysis {

SpectrumConfig SpectrumConfig::compute(std::uint32_t sampleRateHz, std::size_t channelCount)
{
    SpectrumConfig cfg;
    cfg.sample_rate_hz = sampleRateHz;
    cfg.channel_count = channelCount;

    constexpr double tw = 0.170;
    const double winVal = static_cast<double>(sampleRateHz) * tw;

    auto roundTiesToEven = [](double x) -> std::size_t {
        double intPart;
        double frac = std::modf(x, &intPart);
        if (std::abs(frac - 0.5) < 1e-9) {
            long long i = static_cast<long long>(intPart);
            if (std::abs(i % 2) == 1) {
                return static_cast<std::size_t>(i + (x > 0 ? 1 : -1));
            } else {
                return static_cast<std::size_t>(i);
            }
        }
        return static_cast<std::size_t>(std::round(x));
    };

    cfg.window_size = roundTiesToEven(winVal);

    std::size_t nfft = 1;
    while (nfft < cfg.window_size) {
        nfft <<= 1;
    }
    cfg.fft_size = nfft;

    cfg.hop_size = roundTiesToEven(static_cast<double>(cfg.window_size) / 4.0);

    return cfg;
}

}  // namespace rgsml::analysis
