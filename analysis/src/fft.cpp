#include <rgsml/analysis/fft.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rgsml::analysis {

Radix2Fft::Radix2Fft(std::size_t fftSize)
    : fft_size_(fftSize)
{
    bit_reversed_.resize(fft_size_);
    std::size_t bits = 0;
    while ((std::size_t(1) << bits) < fft_size_) {
        ++bits;
    }

    for (std::size_t i = 0; i < fft_size_; ++i) {
        std::size_t rev = 0;
        for (std::size_t b = 0; b < bits; ++b) {
            if ((i >> b) & 1) {
                rev |= (std::size_t(1) << (bits - 1 - b));
            }
        }
        bit_reversed_[i] = rev;
    }

    twiddles_.resize(fft_size_ / 2);
    for (std::size_t k = 0; k < fft_size_ / 2; ++k) {
        const double angle = -2.0 * std::numbers::pi * static_cast<double>(k) / static_cast<double>(fft_size_);
        twiddles_[k] = std::complex<double>(std::cos(angle), std::sin(angle));
    }
}

void Radix2Fft::forward(const double* inputReal, std::size_t inputLen, std::complex<double>* output) const
{
    const std::size_t copyLen = std::min(inputLen, fft_size_);
    for (std::size_t i = 0; i < copyLen; ++i) {
        output[bit_reversed_[i]] = std::complex<double>(inputReal[i], 0.0);
    }
    for (std::size_t i = copyLen; i < fft_size_; ++i) {
        output[bit_reversed_[i]] = std::complex<double>(0.0, 0.0);
    }

    for (std::size_t len = 2; len <= fft_size_; len <<= 1) {
        const std::size_t halfLen = len / 2;
        const std::size_t step = fft_size_ / len;
        for (std::size_t i = 0; i < fft_size_; i += len) {
            for (std::size_t j = 0; j < halfLen; ++j) {
                const auto u = output[i + j];
                const auto v = output[i + j + halfLen] * twiddles_[j * step];
                output[i + j] = u + v;
                output[i + j + halfLen] = u - v;
            }
        }
    }
}

}  // namespace rgsml::analysis
