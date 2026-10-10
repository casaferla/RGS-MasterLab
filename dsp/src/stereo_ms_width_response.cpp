#include <rgsml/dsp/stereo_ms_width_response.hpp>

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/stereo_ms_crossover.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <string>
#include <vector>

namespace rgsml::dsp {
namespace {

using rgsml::core::Error;
using rgsml::core::ErrorCode;
using Response = std::vector<StereoMsWidthResponsePoint>;
using Result = rgsml::core::Result<Response>;

[[nodiscard]] Result invalid_response(std::string message)
{
    return Result::failure(Error{
        ErrorCode::InvalidArgument, std::move(message),
        {{"category", "INVALID_STEREO_MS_RESPONSE_REQUEST"}}});
}

[[nodiscard]] std::complex<double> section_transfer(
    const StereoMsFilterSection& c, std::complex<double> z)
{
    // Frequency response of the exact normalized TDF-II section.
    // DSP runtime implements the same b0,b1,b2,a1,a2 coefficients and sign.
    const auto z2 = z * z;
    return (c.b0 + c.b1 * z + c.b2 * z2)
        / (1.0 + c.a1 * z + c.a2 * z2);
}

} // namespace

Result stereo_ms_width_response_at(
    const StereoMsParameters& parameters,
    double sample_rate_hz,
    std::span<const double> frequencies_hz)
{
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0
        || frequencies_hz.empty() || frequencies_hz.size() > 4096) {
        return invalid_response("A finite positive sample rate and 1..4096 points are required.");
    }
    for (double frequency : frequencies_hz) {
        if (!std::isfinite(frequency) || frequency < 0.0
            || frequency >= sample_rate_hz * 0.5) {
            return invalid_response("Frequency must be finite and within [0, Nyquist).");
        }
    }

    std::optional<StereoMsCrossoverDesign> design;
    // Exact Side mute is a rank-one mono projection. Its frequency response
    // is zero irrespective of saved, non-effective crossover parameters.
    if (!parameters.side_muted() && parameters.mono_bass_mode() != MonoBassMode::OFF) {
        const auto built = design_stereo_ms_crossover(
            parameters.mono_bass_mode(),
            parameters.mono_bass_cutoff_hz(), sample_rate_hz);
        if (!built) return Result::failure(*built.error());
        design = *built.value();
    }

    const double broadband_width =
        100.0 * std::pow(10.0,
            (parameters.side_gain_db() - parameters.mid_gain_db()) / 20.0);
    const double beta = parameters.low_band_width_percent() / 100.0;
    Response points;
    points.reserve(frequencies_hz.size());
    for (const double frequency : frequencies_hz) {
        double width = parameters.side_muted() ? 0.0 : broadband_width;
        if (design) {
            const double omega = 2.0 * std::numbers::pi_v<double>
                * frequency / sample_rate_hz;
            const std::complex<double> z{std::cos(omega), -std::sin(omega)};
            // Runtime cascades exactly two identical sections per band.
            const auto lp = std::pow(section_transfer(design->low_section, z), 2);
            const auto hp = std::pow(section_transfer(design->high_section, z), 2)
                * (design->invert_high_branch ? -1.0 : 1.0);
            const auto mid = lp + hp;
            const auto side = beta * lp + hp;
            const double mid_abs = std::abs(mid);
            if (!std::isfinite(mid_abs) || mid_abs <= 0.0) {
                return invalid_response("Crossover Mid transfer is not numerically evaluable.");
            }
            width *= std::abs(side) / mid_abs;
        }
        if (!std::isfinite(width)) {
            return invalid_response("Width display calculation overflowed.");
        }
        points.push_back({frequency, width});
    }
    return Result::success(std::move(points));
}

Result stereo_ms_width_response_grid(
    const StereoMsParameters& parameters,
    double sample_rate_hz,
    std::size_t point_count)
{
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0
        || point_count < 2 || point_count > 4096) {
        return invalid_response("Grid requires finite sample rate and 2..4096 points.");
    }
    const double upper = std::min(20000.0, 0.49 * sample_rate_hz);
    if (!(upper > 20.0)) {
        return invalid_response("Sample rate is too low for a 20 Hz response grid.");
    }
    std::vector<double> frequencies;
    frequencies.reserve(point_count);
    const double ratio = upper / 20.0;
    for (std::size_t i = 0; i < point_count; ++i) {
        const double x = static_cast<double>(i) /
            static_cast<double>(point_count - 1);
        frequencies.push_back(20.0 * std::pow(ratio, x));
    }
    return stereo_ms_width_response_at(parameters, sample_rate_hz, frequencies);
}

} // namespace rgsml::dsp
