#include <rgsml/dsp/stereo_ms_crossover.hpp>

#include <rgsml/core/error.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>
#include <utility>

namespace rgsml::dsp {
namespace {

[[nodiscard]] rgsml::core::Error design_error(
    rgsml::core::ErrorCode code, std::string category, std::string message)
{
    return rgsml::core::Error{
        code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] rgsml::core::Result<std::int64_t> module_settling(
    double maximum_pole_magnitude)
{
    if (!std::isfinite(maximum_pole_magnitude)
        || maximum_pole_magnitude < 0.0
        || maximum_pole_magnitude >= 1.0) {
        return rgsml::core::Result<std::int64_t>::failure(design_error(
            rgsml::core::ErrorCode::InvalidState,
            "STEREO_MS_UNSTABLE_CROSSOVER",
            "Rounded crossover poles are not strictly inside the unit circle."));
    }
    if (maximum_pole_magnitude == 0.0) {
        // The frozen pole policy explicitly excludes feedforward memory.
        return rgsml::core::Result<std::int64_t>::success(0);
    }

    const double per_section =
        std::ceil(std::log(1e-6) / std::log(maximum_pole_magnitude));
    if (!std::isfinite(per_section) || per_section < 0.0
        || per_section >= static_cast<double>(
            std::numeric_limits<std::int64_t>::max() / 2)) {
        return rgsml::core::Result<std::int64_t>::failure(design_error(
            rgsml::core::ErrorCode::OutOfRange,
            "STEREO_MS_SETTLING_OVERFLOW",
            "Pole-derived two-section settling does not fit frame limits."));
    }
    const auto section = static_cast<std::int64_t>(per_section);
    return rgsml::core::Result<std::int64_t>::success(section * 2);
}

[[nodiscard]] bool finite_section(StereoMsFilterSection s) noexcept
{
    return std::isfinite(s.b0) && std::isfinite(s.b1)
        && std::isfinite(s.b2) && std::isfinite(s.a1)
        && std::isfinite(s.a2);
}

}  // namespace

rgsml::core::Result<StereoMsCrossoverDesign> design_stereo_ms_crossover(
    MonoBassMode mode, double cutoff_hz, double sample_rate_hz)
{
    using rgsml::core::ErrorCode;
    using rgsml::core::Result;

    if (mode != MonoBassMode::LR12 && mode != MonoBassMode::LR24) {
        return Result<StereoMsCrossoverDesign>::failure(design_error(
            ErrorCode::InvalidArgument, "INVALID_STEREO_MS_MODE",
            "Only active LR12/LR24 have crossover coefficients; OFF has none."));
    }
    if (!std::isfinite(cutoff_hz) || !std::isfinite(sample_rate_hz)
        || sample_rate_hz <= 0.0 || cutoff_hz < 40.0
        || cutoff_hz > 300.0
        || !(cutoff_hz < 0.45 * sample_rate_hz)) {
        return Result<StereoMsCrossoverDesign>::failure(design_error(
            ErrorCode::InvalidArgument, "INVALID_STEREO_MS_CUTOFF",
            "Cutoff must be within [40,300] Hz and strictly below 0.45 Fs."));
    }

    const double k = std::tan(std::numbers::pi_v<double>
                              * cutoff_hz / sample_rate_hz);
    if (!std::isfinite(k) || k <= 0.0) {
        return Result<StereoMsCrossoverDesign>::failure(design_error(
            ErrorCode::InvalidState, "INVALID_STEREO_MS_COEFFICIENT",
            "The prewarped Butterworth K must be finite and positive."));
    }

    StereoMsCrossoverDesign d{};
    d.mode = mode;
    d.sections_per_branch = 2;
    d.invert_high_branch = (mode == MonoBassMode::LR12);
    if (mode == MonoBassMode::LR12) {
        // Recovered one-pole Phase-5 law: denominator 1 + a1*z^-1.
        const double denom = 1.0 + k;
        const double r = (1.0 - k) / denom;
        const double lp = k / denom;
        const double hp = 1.0 / denom;
        d.low_section = {lp, lp, 0.0, -r, 0.0};
        d.high_section = {hp, -hp, 0.0, -r, 0.0};
        d.maximum_pole_magnitude = std::abs(r);
    } else {
        // LR24 is TWO identical second-order Butterworth sections per branch
        // with Q=1/sqrt(2), not a fourth-order Butterworth prototype.
        const double root2 = std::numbers::sqrt2_v<double>;
        const double kk = k * k;
        const double denom = 1.0 + root2 * k + kk;
        const double lp = kk / denom;
        const double hp = 1.0 / denom;
        const double a1 = (2.0 * (kk - 1.0)) / denom;
        const double a2 = (1.0 - root2 * k + kk) / denom;
        d.low_section = {lp, 2.0 * lp, lp, a1, a2};
        d.high_section = {hp, -2.0 * hp, hp, a1, a2};
        // Schur stability on the rounded coefficients, not just an ideal
        // pole. Checked with the same a1/a2 used by the runtime sections.
        if (!(1.0 + a1 + a2 > 0.0)
            || !(1.0 - a1 + a2 > 0.0)
            || !(1.0 - a2 > 0.0)
            || !(a2 >= 0.0)) {
            return Result<StereoMsCrossoverDesign>::failure(design_error(
                ErrorCode::InvalidState, "STEREO_MS_UNSTABLE_CROSSOVER",
                "Rounded LR24 denominator fails strict Schur stability."));
        }
        d.maximum_pole_magnitude = std::sqrt(a2);
    }

    if (!finite_section(d.low_section) || !finite_section(d.high_section)) {
        return Result<StereoMsCrossoverDesign>::failure(design_error(
            ErrorCode::InvalidState, "INVALID_STEREO_MS_COEFFICIENT",
            "A normalized LR section contains non-finite coefficients."));
    }

    const auto settlement = module_settling(d.maximum_pole_magnitude);
    if (!settlement) {
        return Result<StereoMsCrossoverDesign>::failure(*settlement.error());
    }
    d.settling_frames = *settlement.value();
    return Result<StereoMsCrossoverDesign>::success(d);
}

}  // namespace rgsml::dsp
