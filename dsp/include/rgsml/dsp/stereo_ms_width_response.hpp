#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace rgsml::dsp {

// Observational editor value only; NOT an audio processing parameter.
// The ratio is |output Side response / output Mid response| relative to
// equal-amplitude input Mid/Side, including their stored broadband gains.
// It does not convey phase identity or predict a source's actual energy.
struct StereoMsWidthResponsePoint final {
    double frequency_hz;
    double effective_width_percent;

    friend bool operator==(const StereoMsWidthResponsePoint&,
                           const StereoMsWidthResponsePoint&) = default;
};

// Frequency-domain evaluation using the EXACT frozen StereoMsCrossoverDesign
// biquad coefficients, two serial sections, and LR12 high-branch inversion.
// No filter law, side gain or crossover model may be re-derived in QML.
// For canonical mono input / bypass, the caller must disable this stereo graph.
// This function performs no audio processing and never mutates DSP state.
[[nodiscard]] rgsml::core::Result<std::vector<StereoMsWidthResponsePoint>>
stereo_ms_width_response_at(
    const StereoMsParameters& parameters,
    double sample_rate_hz,
    std::span<const double> frequencies_hz);

// Log-spaced display grid from 20 Hz to min(20 kHz, 0.49 Fs). Its purpose
// is screen geometry, not new DSP mathematics or sample-rate substitution.
[[nodiscard]] rgsml::core::Result<std::vector<StereoMsWidthResponsePoint>>
stereo_ms_width_response_grid(
    const StereoMsParameters& parameters,
    double sample_rate_hz,
    std::size_t point_count = 129);

} // namespace rgsml::dsp
