#include <rgsml/dsp/true_peak_limiter_parameters.hpp>

#include <rgsml/core/error.hpp>

#include <cmath>

namespace rgsml::dsp {

rgsml::core::Result<TruePeakLimiterParameters>
TruePeakLimiterParameters::create(double ceiling_db_tp)
{
    if (!std::isfinite(ceiling_db_tp)) {
        return rgsml::core::Result<TruePeakLimiterParameters>::failure(
            rgsml::core::Error{
                rgsml::core::ErrorCode::InvalidArgument,
                "Ceiling must be a finite binary64 value.",
                {{"category", "INVALID_TRUE_PEAK_LIMITER_PARAMETER"}}});
    }
    if (ceiling_db_tp < -12.0 || ceiling_db_tp > 0.0) {
        return rgsml::core::Result<TruePeakLimiterParameters>::failure(
            rgsml::core::Error{
                rgsml::core::ErrorCode::OutOfRange,
                "Ceiling must lie within inclusive [-12, 0] dBTP.",
                {{"category", "TRUE_PEAK_LIMITER_PARAMETER_OUT_OF_RANGE"}}});
    }
    return rgsml::core::Result<TruePeakLimiterParameters>::success(
        TruePeakLimiterParameters{ceiling_db_tp == 0.0 ? 0.0 : ceiling_db_tp});
}

rgsml::core::Result<TruePeakLimiterParameters>
TruePeakLimiterParameters::create_default() noexcept
{
    return create(-1.0);
}

TruePeakLimiterParameters::TruePeakLimiterParameters(double ceiling_db_tp) noexcept
    : ceiling_db_tp_(ceiling_db_tp)
{
}

double TruePeakLimiterParameters::ceiling_db_tp() const noexcept
{
    return ceiling_db_tp_;
}

}  // namespace rgsml::dsp
