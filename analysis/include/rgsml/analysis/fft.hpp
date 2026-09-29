#ifndef RGSML_ANALYSIS_FFT_HPP
#define RGSML_ANALYSIS_FFT_HPP

#include <complex>
#include <cstddef>
#include <vector>

namespace rgsml::analysis {

class Radix2Fft final {
public:
    explicit Radix2Fft(std::size_t fftSize);

    void forward(const double* inputReal, std::size_t inputLen, std::complex<double>* output) const;

    [[nodiscard]] std::size_t fft_size() const noexcept { return fft_size_; }

private:
    std::size_t fft_size_;
    std::vector<std::size_t> bit_reversed_;
    std::vector<std::complex<double>> twiddles_;
};

}  // namespace rgsml::analysis

#endif  // RGSML_ANALYSIS_FFT_HPP
