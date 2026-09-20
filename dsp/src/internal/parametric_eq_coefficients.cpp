#include <rgsml/dsp/internal/parametric_eq_coefficients.hpp>

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace rgsml::dsp::internal {
namespace {

using rgsml::core::Error;
using rgsml::core::ErrorCode;
using rgsml::core::Result;

constexpr double kPi = std::numbers::pi;

[[nodiscard]] Error coeff_error(ErrorCode code, std::string category, std::string message)
{
    return Error{code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] Result<SectionCoefficients> make_section(
    double b0, double b1, double b2, double a0, double a1, double a2)
{
    if (!std::isfinite(b0) || !std::isfinite(b1) || !std::isfinite(b2)
        || !std::isfinite(a0) || !std::isfinite(a1) || !std::isfinite(a2)
        || a0 == 0.0) {
        return Result<SectionCoefficients>::failure(coeff_error(
            ErrorCode::InvalidState,
            "NONFINITE_COEFFICIENT",
            "Biquad coefficient calculation yielded non-finite values or zero a0."));
    }

    const double norm_b0 = b0 / a0;
    const double norm_b1 = b1 / a0;
    const double norm_b2 = b2 / a0;
    const double norm_a1 = a1 / a0;
    const double norm_a2 = a2 / a0;

    if (!std::isfinite(norm_b0) || !std::isfinite(norm_b1) || !std::isfinite(norm_b2)
        || !std::isfinite(norm_a1) || !std::isfinite(norm_a2)) {
        return Result<SectionCoefficients>::failure(coeff_error(
            ErrorCode::InvalidState,
            "NONFINITE_COEFFICIENT",
            "Normalized Biquad coefficients are non-finite."));
    }

    // Pole calculation for z^2 + a1*z + a2 = 0
    const double disc = norm_a1 * norm_a1 - 4.0 * norm_a2;
    double rmax = 0.0;
    if (disc < 0.0) {
        rmax = std::sqrt(std::abs(norm_a2));
    } else {
        const double sqrt_disc = std::sqrt(disc);
        const double p1 = std::abs((-norm_a1 + sqrt_disc) / 2.0);
        const double p2 = std::abs((-norm_a1 - sqrt_disc) / 2.0);
        rmax = std::max(p1, p2);
    }

    if (!std::isfinite(rmax) || rmax >= 1.0) {
        return Result<SectionCoefficients>::failure(coeff_error(
            ErrorCode::InvalidState,
            "UNSTABLE_POLE",
            "Biquad filter section has unstable or non-finite pole radius."));
    }

    std::int64_t settling = 0;
    if (rmax > 0.0) {
        constexpr double kThreshold = 1e-6; // -120 dB
        const double frames = std::ceil(std::log(kThreshold) / std::log(rmax));
        settling = static_cast<std::int64_t>(std::max(0.0, frames));
    }

    return Result<SectionCoefficients>::success(SectionCoefficients{
        BiquadCoefficients{norm_b0, norm_b1, norm_b2, norm_a1, norm_a2},
        rmax,
        settling});
}

}  // namespace

Result<ParametricEqCoefficients> compute_parametric_eq_coefficients(
    const ParametricEqParameters& params,
    double sample_rate_hz)
{
    if (!std::isfinite(sample_rate_hz) || sample_rate_hz <= 0.0) {
        return Result<ParametricEqCoefficients>::failure(coeff_error(
            ErrorCode::InvalidArgument,
            "INVALID_SAMPLE_RATE",
            "Sample rate must be positive binary64."));
    }

    ParametricEqCoefficients result;
    result.bands.reserve(params.bands().size());
    std::int64_t total_settling = 0;

    for (const auto& band : params.bands()) {
        BandCoefficients band_coeffs;
        band_coeffs.band_id = band.band_id();
        band_coeffs.enabled = band.enabled();
        band_coeffs.filter_type = band.filter_type();
        band_coeffs.routing = band.routing();

        if (!band.enabled()) {
            result.bands.push_back(std::move(band_coeffs));
            continue;
        }

        std::int64_t band_settling = 0;

        switch (band.filter_type()) {
        case EqFilterType::BELL: {
            const auto& p = std::get<BellPayload>(band.payload());
            if (p.frequency_hz > 0.45 * sample_rate_hz) {
                return Result<ParametricEqCoefficients>::failure(coeff_error(
                    ErrorCode::OutOfRange,
                    "FREQUENCY_EXCEEDS_NYQUIST_GUARD",
                    "Frequency exceeds 0.45 * Fs limit."));
            }
            const double w0 = 2.0 * kPi * p.frequency_hz / sample_rate_hz;
            const double A = std::pow(10.0, p.gain_db / 40.0);
            const double alpha = std::sin(w0) / (2.0 * p.q);

            const double b0 = 1.0 + alpha * A;
            const double b1 = -2.0 * std::cos(w0);
            const double b2 = 1.0 - alpha * A;
            const double a0 = 1.0 + alpha / A;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha / A;

            auto sec = make_section(b0, b1, b2, a0, a1, a2);
            if (!sec) {
                return Result<ParametricEqCoefficients>::failure(*sec.error());
            }
            band_settling += sec.value()->settling_frames;
            band_coeffs.sections.push_back(std::move(*sec.value()));
            break;
        }
        case EqFilterType::NOTCH: {
            const auto& p = std::get<NotchPayload>(band.payload());
            if (p.frequency_hz > 0.45 * sample_rate_hz) {
                return Result<ParametricEqCoefficients>::failure(coeff_error(
                    ErrorCode::OutOfRange,
                    "FREQUENCY_EXCEEDS_NYQUIST_GUARD",
                    "Frequency exceeds 0.45 * Fs limit."));
            }
            const double w0 = 2.0 * kPi * p.frequency_hz / sample_rate_hz;
            const double alpha = std::sin(w0) / (2.0 * p.q);

            const double b0 = 1.0;
            const double b1 = -2.0 * std::cos(w0);
            const double b2 = 1.0;
            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha;

            auto sec = make_section(b0, b1, b2, a0, a1, a2);
            if (!sec) {
                return Result<ParametricEqCoefficients>::failure(*sec.error());
            }
            band_settling += sec.value()->settling_frames;
            band_coeffs.sections.push_back(std::move(*sec.value()));
            break;
        }
        case EqFilterType::LOW_SHELF: {
            const auto& p = std::get<ShelfPayload>(band.payload());
            if (p.frequency_hz > 0.45 * sample_rate_hz) {
                return Result<ParametricEqCoefficients>::failure(coeff_error(
                    ErrorCode::OutOfRange,
                    "FREQUENCY_EXCEEDS_NYQUIST_GUARD",
                    "Frequency exceeds 0.45 * Fs limit."));
            }
            const double w0 = 2.0 * kPi * p.frequency_hz / sample_rate_hz;
            const double A = std::pow(10.0, p.gain_db / 40.0);
            const double S = p.shelf_slope;
            const double alpha = (std::sin(w0) / 2.0) * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
            const double beta = 2.0 * std::sqrt(A) * alpha;
            const double c = std::cos(w0);

            const double b0 = A * ((A + 1.0) - (A - 1.0) * c + beta);
            const double b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * c);
            const double b2 = A * ((A + 1.0) - (A - 1.0) * c - beta);
            const double a0 = (A + 1.0) + (A - 1.0) * c + beta;
            const double a1 = -2.0 * ((A - 1.0) + (A + 1.0) * c);
            const double a2 = (A + 1.0) + (A - 1.0) * c - beta;

            auto sec = make_section(b0, b1, b2, a0, a1, a2);
            if (!sec) {
                return Result<ParametricEqCoefficients>::failure(*sec.error());
            }
            band_settling += sec.value()->settling_frames;
            band_coeffs.sections.push_back(std::move(*sec.value()));
            break;
        }
        case EqFilterType::HIGH_SHELF: {
            const auto& p = std::get<ShelfPayload>(band.payload());
            if (p.frequency_hz > 0.45 * sample_rate_hz) {
                return Result<ParametricEqCoefficients>::failure(coeff_error(
                    ErrorCode::OutOfRange,
                    "FREQUENCY_EXCEEDS_NYQUIST_GUARD",
                    "Frequency exceeds 0.45 * Fs limit."));
            }
            const double w0 = 2.0 * kPi * p.frequency_hz / sample_rate_hz;
            const double A = std::pow(10.0, p.gain_db / 40.0);
            const double S = p.shelf_slope;
            const double alpha = (std::sin(w0) / 2.0) * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
            const double beta = 2.0 * std::sqrt(A) * alpha;
            const double c = std::cos(w0);

            const double b0 = A * ((A + 1.0) + (A - 1.0) * c + beta);
            const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * c);
            const double b2 = A * ((A + 1.0) + (A - 1.0) * c - beta);
            const double a0 = (A + 1.0) - (A - 1.0) * c + beta;
            const double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * c);
            const double a2 = (A + 1.0) - (A - 1.0) * c - beta;

            auto sec = make_section(b0, b1, b2, a0, a1, a2);
            if (!sec) {
                return Result<ParametricEqCoefficients>::failure(*sec.error());
            }
            band_settling += sec.value()->settling_frames;
            band_coeffs.sections.push_back(std::move(*sec.value()));
            break;
        }
        case EqFilterType::HIGH_PASS:
        case EqFilterType::LOW_PASS: {
            const auto& p = std::get<PassPayload>(band.payload());
            if (p.frequency_hz > 0.45 * sample_rate_hz) {
                return Result<ParametricEqCoefficients>::failure(coeff_error(
                    ErrorCode::OutOfRange,
                    "FREQUENCY_EXCEEDS_NYQUIST_GUARD",
                    "Frequency exceeds 0.45 * Fs limit."));
            }
            const double K = std::tan(kPi * p.frequency_hz / sample_rate_hz);
            const auto slope = static_cast<std::uint16_t>(p.slope_db_per_octave);
            int order = 0;
            switch (slope) {
            case 6: order = 1; break;
            case 12: order = 2; break;
            case 18: order = 3; break;
            case 24: order = 4; break;
            case 36: order = 6; break;
            case 48: order = 8; break;
            default:
                return Result<ParametricEqCoefficients>::failure(coeff_error(
                    ErrorCode::InvalidArgument,
                    "INVALID_SLOPE",
                    "Invalid slope value."));
            }

            const bool is_hp = (band.filter_type() == EqFilterType::HIGH_PASS);

            if (order % 2 != 0) {
                // 1st order section
                double b0 = 0.0, b1 = 0.0, a0 = 1.0 + K, a1 = K - 1.0;
                if (is_hp) {
                    b0 = 1.0 / (1.0 + K);
                    b1 = -b0;
                } else {
                    b0 = K / (1.0 + K);
                    b1 = b0;
                }
                // make_section expects b0, b1, b2, a0, a1, a2 where normalized coefficients are divided by a0
                // For 1st order: a0 = 1, b0 = b0, b1 = b1, b2 = 0, a1 = (K-1)/(K+1), a2 = 0
                auto sec = make_section(b0, b1, 0.0, 1.0, (K - 1.0) / (K + 1.0), 0.0);
                if (!sec) {
                    return Result<ParametricEqCoefficients>::failure(*sec.error());
                }
                band_settling += sec.value()->settling_frames;
                band_coeffs.sections.push_back(std::move(*sec.value()));
            }

            const int num_2nd = order / 2;
            std::vector<double> q_factors;
            q_factors.reserve(num_2nd);
            for (int k = 1; k <= num_2nd; ++k) {
                const double q = 1.0 / (2.0 * std::sin((2 * k - 1) * kPi / (2.0 * order)));
                q_factors.push_back(q);
            }
            std::sort(q_factors.begin(), q_factors.end()); // ascending Q

            for (const double Q : q_factors) {
                const double norm = 1.0 / (1.0 + K / Q + K * K);
                double b0 = 0.0, b1 = 0.0, b2 = 0.0;
                if (is_hp) {
                    b0 = norm;
                    b1 = -2.0 * norm;
                    b2 = norm;
                } else {
                    b0 = K * K * norm;
                    b1 = 2.0 * b0;
                    b2 = b0;
                }
                const double a1 = 2.0 * (K * K - 1.0) * norm;
                const double a2 = (1.0 - K / Q + K * K) * norm;

                auto sec = make_section(b0, b1, b2, 1.0, a1, a2);
                if (!sec) {
                    return Result<ParametricEqCoefficients>::failure(*sec.error());
                }
                band_settling += sec.value()->settling_frames;
                band_coeffs.sections.push_back(std::move(*sec.value()));
            }
            break;
        }
        }

        total_settling += band_settling;
        result.bands.push_back(std::move(band_coeffs));
    }

    result.total_settling_frames = total_settling;
    return Result<ParametricEqCoefficients>::success(std::move(result));
}

}  // namespace rgsml::dsp::internal
