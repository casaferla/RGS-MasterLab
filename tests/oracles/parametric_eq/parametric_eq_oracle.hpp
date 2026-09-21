#pragma once

#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <complex>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

namespace rgsml::tests::oracles {

struct IndependentBiquadCoeffs final {
    double b0{1.0};
    double b1{0.0};
    double b2{0.0};
    double a1{0.0};
    double a2{0.0};
};

// O1: Independent coefficient oracle using double precision binary64 reference math
inline IndependentBiquadCoeffs compute_bell_coeffs(double f, double gain_db, double q, double fs)
{
    const double w0 = 2.0 * std::numbers::pi * f / fs;
    const double A = std::pow(10.0, gain_db / 40.0);
    const double alpha = std::sin(w0) / (2.0 * q);

    const double b0 = 1.0 + alpha * A;
    const double b1 = -2.0 * std::cos(w0);
    const double b2 = 1.0 - alpha * A;
    const double a0 = 1.0 + alpha / A;
    const double a1 = -2.0 * std::cos(w0);
    const double a2 = 1.0 - alpha / A;

    return IndependentBiquadCoeffs{b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

inline IndependentBiquadCoeffs compute_notch_coeffs(double f, double q, double fs)
{
    const double w0 = 2.0 * std::numbers::pi * f / fs;
    const double alpha = std::sin(w0) / (2.0 * q);

    const double b0 = 1.0;
    const double b1 = -2.0 * std::cos(w0);
    const double b2 = 1.0;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * std::cos(w0);
    const double a2 = 1.0 - alpha;

    return IndependentBiquadCoeffs{b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

inline IndependentBiquadCoeffs compute_low_shelf_coeffs(double f, double gain_db, double slope, double fs)
{
    const double w0 = 2.0 * std::numbers::pi * f / fs;
    const double A = std::pow(10.0, gain_db / 40.0);
    const double alpha = (std::sin(w0) / 2.0) * std::sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
    const double beta = 2.0 * std::sqrt(A) * alpha;
    const double c = std::cos(w0);

    const double b0 = A * ((A + 1.0) - (A - 1.0) * c + beta);
    const double b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * c);
    const double b2 = A * ((A + 1.0) - (A - 1.0) * c - beta);
    const double a0 = (A + 1.0) + (A - 1.0) * c + beta;
    const double a1 = -2.0 * ((A - 1.0) + (A + 1.0) * c);
    const double a2 = (A + 1.0) + (A - 1.0) * c - beta;

    return IndependentBiquadCoeffs{b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

inline IndependentBiquadCoeffs compute_high_shelf_coeffs(double f, double gain_db, double slope, double fs)
{
    const double w0 = 2.0 * std::numbers::pi * f / fs;
    const double A = std::pow(10.0, gain_db / 40.0);
    const double alpha = (std::sin(w0) / 2.0) * std::sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
    const double beta = 2.0 * std::sqrt(A) * alpha;
    const double c = std::cos(w0);

    const double b0 = A * ((A + 1.0) + (A - 1.0) * c + beta);
    const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * c);
    const double b2 = A * ((A + 1.0) + (A - 1.0) * c - beta);
    const double a0 = (A + 1.0) - (A - 1.0) * c + beta;
    const double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * c);
    const double a2 = (A + 1.0) - (A - 1.0) * c - beta;

    return IndependentBiquadCoeffs{b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

// O2: Independent scalar TDF-II sample oracle
struct IndependentTdf2State final {
    double s1{0.0};
    double s2{0.0};

    double process_sample(double x, const IndependentBiquadCoeffs& c)
    {
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

// O3: Independent analytic transfer oracle
// H(e^jw)=(b0+b1e^-jw+b2e^-j2w)/(1+a1e^-jw+a2e^-j2w)
inline std::complex<double> biquad_transfer_function(
    const IndependentBiquadCoeffs& c,
    double frequency_hz,
    double sample_rate_hz)
{
    const double w = 2.0 * std::numbers::pi * frequency_hz / sample_rate_hz;
    const std::complex<double> j(0.0, 1.0);
    const std::complex<double> z1 = std::exp(-j * w);
    const std::complex<double> z2 = std::exp(-j * 2.0 * w);

    const std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;
    return num / den;
}

// O4: Rendered signal verification helper (RMS difference)
inline double compute_rms_diff(std::span<const double> a, std::span<const double> b)
{
    if (a.size() != b.size() || a.empty()) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double diff = a[i] - b[i];
        sum += diff * diff;
    }
    return std::sqrt(sum / static_cast<double>(a.size()));
}

}  // namespace rgsml::tests::oracles
