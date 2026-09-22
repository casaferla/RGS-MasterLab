#include <rgsml/dsp/parametric_eq_response.hpp>

#include "internal/parametric_eq_coefficients.hpp"

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace rgsml::dsp {
namespace {

using rgsml::core::Error;
using rgsml::core::ErrorCode;
using rgsml::core::Result;

constexpr double kPi = std::numbers::pi;

[[nodiscard]] Error response_error(ErrorCode code, std::string message)
{
    return Error{code, std::move(message)};
}

[[nodiscard]] std::complex<double> evaluate_biquad_transfer(
    const internal::BiquadCoefficients& c,
    double frequency_hz,
    double sample_rate_hz) noexcept
{
    const double w = 2.0 * kPi * frequency_hz / sample_rate_hz;
    const std::complex<double> j(0.0, 1.0);
    const std::complex<double> z1 = std::exp(-j * w);
    const std::complex<double> z2 = std::exp(-j * (2.0 * w));

    const std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;
    return num / den;
}

[[nodiscard]] EqResponsePoint make_response_point(
    double frequency_hz,
    const std::complex<double>& H) noexcept
{
    const double mag_lin = std::abs(H);
    const double mag_db = mag_lin > 1e-15
        ? 20.0 * std::log10(mag_lin)
        : -300.0;
    const double phase_rad = std::arg(H);
    return EqResponsePoint{
        .frequency_hz = frequency_hz,
        .magnitude_db = mag_db,
        .phase_rad = phase_rad,
        .transfer_function = H,
    };
}

[[nodiscard]] Result<std::complex<double>> evaluate_internal_transfer(
    const ParametricEqParameters& params,
    double frequency_hz,
    core::SampleRate sample_rate)
{
    const double Fs = static_cast<double>(sample_rate.value());
    if (!std::isfinite(Fs) || Fs <= 0.0) {
        return Result<std::complex<double>>::failure(response_error(
            ErrorCode::InvalidArgument,
            "Sample rate must be positive binary64."));
    }
    if (!std::isfinite(frequency_hz) || frequency_hz < 0.0) {
        return Result<std::complex<double>>::failure(response_error(
            ErrorCode::InvalidArgument,
            "Frequency must be finite and non-negative."));
    }
    if (frequency_hz > 0.45 * Fs) {
        return Result<std::complex<double>>::failure(response_error(
            ErrorCode::OutOfRange,
            "Frequency exceeds 0.45 * Fs limit."));
    }

    auto coeffs_res = internal::compute_parametric_eq_coefficients(params, Fs);
    if (!coeffs_res) {
        return Result<std::complex<double>>::failure(*coeffs_res.error());
    }

    std::complex<double> H_total(1.0, 0.0);
    for (const auto& band : coeffs_res.value()->bands) {
        if (!band.enabled) {
            continue;
        }
        for (const auto& sec : band.sections) {
            H_total *= evaluate_biquad_transfer(sec.coeffs, frequency_hz, Fs);
        }
    }

    return Result<std::complex<double>>::success(H_total);
}

}  // namespace

Result<EqResponsePoint> evaluate_band_point(
    const EqBandParameters& band,
    double frequency_hz,
    core::SampleRate sample_rate)
{
    auto single_param = ParametricEqParameters::create({band});
    if (!single_param) {
        return Result<EqResponsePoint>::failure(*single_param.error());
    }
    auto transfer = evaluate_internal_transfer(*single_param.value(), frequency_hz, sample_rate);
    if (!transfer) {
        return Result<EqResponsePoint>::failure(*transfer.error());
    }
    return Result<EqResponsePoint>::success(
        make_response_point(frequency_hz, *transfer.value()));
}

Result<std::vector<EqResponsePoint>> evaluate_band_response(
    const EqBandParameters& band,
    const std::vector<double>& frequencies_hz,
    core::SampleRate sample_rate)
{
    std::vector<EqResponsePoint> points;
    points.reserve(frequencies_hz.size());
    for (const double f : frequencies_hz) {
        auto pt = evaluate_band_point(band, f, sample_rate);
        if (!pt) {
            return Result<std::vector<EqResponsePoint>>::failure(*pt.error());
        }
        points.push_back(std::move(*pt.value()));
    }
    return Result<std::vector<EqResponsePoint>>::success(std::move(points));
}

}  // namespace rgsml::dsp
