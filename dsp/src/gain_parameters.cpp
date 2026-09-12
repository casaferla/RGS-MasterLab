#include <rgsml/dsp/gain_parameters.hpp>

#include <rgsml/core/error.hpp>

#include <cmath>

namespace rgsml::dsp {

rgsml::core::Result<GainParameters> GainParameters::create(double gain_db)
{
    if (!std::isfinite(gain_db)) {
        return rgsml::core::Result<GainParameters>::failure(rgsml::core::Error{
            rgsml::core::ErrorCode::InvalidArgument,
            "Gain must be a finite binary64 value.",
            {{"category", "INVALID_GAIN_PARAMETER"}}});
    }
    if (gain_db < -24.0 || gain_db > 24.0) {
        return rgsml::core::Result<GainParameters>::failure(rgsml::core::Error{
            rgsml::core::ErrorCode::OutOfRange,
            "Gain is outside the inclusive [-24, +24] dB range.",
            {{"category", "GAIN_PARAMETER_OUT_OF_RANGE"}}});
    }
    return rgsml::core::Result<GainParameters>::success(
        GainParameters{gain_db == 0.0 ? 0.0 : gain_db});
}

GainParameters::GainParameters(double canonical_gain_db) noexcept
    : gain_db_(canonical_gain_db)
{
}

double GainParameters::gain_db() const noexcept
{
    return gain_db_;
}

}  // namespace rgsml::dsp
