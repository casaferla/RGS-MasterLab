#ifndef RGSML_ANALYSIS_SPECTRUM_CONFIG_HPP
#define RGSML_ANALYSIS_SPECTRUM_CONFIG_HPP

#include <cstddef>
#include <cstdint>

namespace rgsml::analysis {

struct SpectrumConfig final {
    std::uint32_t sample_rate_hz{44100};
    std::size_t channel_count{2};
    std::size_t window_size{7497};
    std::size_t fft_size{8192};
    std::size_t hop_size{1874};

    static SpectrumConfig compute(std::uint32_t sampleRateHz, std::size_t channelCount);
};

}  // namespace rgsml::analysis

#endif  // RGSML_ANALYSIS_SPECTRUM_CONFIG_HPP
