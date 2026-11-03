#include <rgsml/dsp/stereo_ms_width.hpp>

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace rgsml::dsp {
namespace {

[[nodiscard]] rgsml::core::Error width_error(
    rgsml::core::ErrorCode code, std::string category,
    std::string message)
{
    return rgsml::core::Error{
        code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] double common_gain(const StereoMsParameters& p) noexcept
{
    return 0.5 * (p.mid_gain_db() + p.side_gain_db());
}

}  // namespace

StereoMsWidthCoordinates stereo_ms_width_coordinates(
    const StereoMsParameters& p) noexcept
{
    const double c = common_gain(p);
    // Direct intersection of Mid [-12,+12] and Side [-24,+12]:
    // Mid=C-d/2, Side=C+d/2, d=Side-Mid.
    const double min_d = std::max(2.0*c - 24.0, -48.0 - 2.0*c);
    const double max_d = std::min(2.0*c + 24.0, 24.0 - 2.0*c);
    const double current_width = p.side_muted() ? 0.0
        : 100.0 * std::pow(10.0,
            (p.side_gain_db() - p.mid_gain_db()) / 20.0);
    return StereoMsWidthCoordinates{
        c,
        current_width,
        100.0 * std::pow(10.0, min_d / 20.0),
        100.0 * std::pow(10.0, max_d / 20.0)};
}

rgsml::core::Result<StereoMsParameters>
stereo_ms_edit_width_preserving_common_gain(
    const StereoMsParameters& current,
    double requested_width_percent)
{
    using rgsml::core::ErrorCode;
    using rgsml::core::Result;

    if (!std::isfinite(requested_width_percent)
        || requested_width_percent < 0.0) {
        return Result<StereoMsParameters>::failure(width_error(
            ErrorCode::InvalidArgument,
            "INVALID_STEREO_MS_WIDTH",
            "Width request must be finite and nonnegative."));
    }

    // An unchanged displayed value is already backed by valid, canonical
    // stored gains. In particular, never reject a rounded display of a
    // valid endpoint or accidentally clear the exact Side mute endpoint.
    const auto coordinates = stereo_ms_width_coordinates(current);
    if (requested_width_percent == coordinates.current_width_percent) {
        return Result<StereoMsParameters>::success(current);
    }

    if (requested_width_percent == 0.0) {
        return StereoMsParameters::create(
            current.mid_gain_db(),
            current.side_gain_db(),
            true,
            current.mono_bass_mode(),
            current.mono_bass_cutoff_hz(),
            current.low_band_width_percent());
    }

    // Evaluate the logarithm without W/100: dividing subnormal W by 100
    // could underflow to zero and falsely turn a positive edit into mute.
    const double d = 20.0 * (
        std::log10(requested_width_percent) - 2.0);
    const double c = coordinates.common_gain_db;
    const double min_d = std::max(2.0*c - 24.0, -48.0 - 2.0*c);
    const double max_d = std::min(2.0*c + 24.0, 24.0 - 2.0*c);
    if (!std::isfinite(d) || d < min_d || d > max_d) {
        return Result<StereoMsParameters>::failure(width_error(
            ErrorCode::OutOfRange,
            "STEREO_MS_WIDTH_NOT_REPRESENTABLE",
            "Requested Width is outside the fixed-common-gain legal domain."));
    }

    const double mid = c - d * 0.5;
    const double side = c + d * 0.5;
    // Canonical parameter validation is the final authority. No epsilon,
    // clipping, hidden common gain adjustment or secondary Width field.
    const auto updated = StereoMsParameters::create(
        mid, side, false,
        current.mono_bass_mode(),
        current.mono_bass_cutoff_hz(),
        current.low_band_width_percent());
    if (!updated) {
        return Result<StereoMsParameters>::failure(width_error(
            ErrorCode::OutOfRange,
            "STEREO_MS_WIDTH_NOT_REPRESENTABLE",
            "Rounded Mid/Side gain mapping is not a legal parameter state."));
    }
    return updated;
}

}  // namespace rgsml::dsp
