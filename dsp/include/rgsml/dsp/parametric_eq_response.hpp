#pragma once

#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <complex>
#include <vector>

namespace rgsml::dsp {

struct EqResponsePoint final {
    double frequency_hz{0.0};
    double magnitude_db{0.0};
    double phase_rad{0.0};
    std::complex<double> transfer_function{1.0, 0.0};

    friend bool operator==(const EqResponsePoint&, const EqResponsePoint&) = default;
};

[[nodiscard]] core::Result<std::complex<double>> evaluate_parametric_eq_transfer(
    const ParametricEqParameters& params,
    double frequency_hz,
    core::SampleRate sample_rate);

[[nodiscard]] core::Result<EqResponsePoint> evaluate_parametric_eq_point(
    const ParametricEqParameters& params,
    double frequency_hz,
    core::SampleRate sample_rate);

[[nodiscard]] core::Result<std::vector<EqResponsePoint>> evaluate_parametric_eq_response(
    const ParametricEqParameters& params,
    const std::vector<double>& frequencies_hz,
    core::SampleRate sample_rate);

[[nodiscard]] core::Result<EqResponsePoint> evaluate_band_point(
    const EqBandParameters& band,
    double frequency_hz,
    core::SampleRate sample_rate);

[[nodiscard]] core::Result<std::vector<EqResponsePoint>> evaluate_band_response(
    const EqBandParameters& band,
    const std::vector<double>& frequencies_hz,
    core::SampleRate sample_rate);

}  // namespace rgsml::dsp
