#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <rgsml/core/error.hpp>
#include <cmath>

namespace rgsml::dsp {
core::Result<TransientShaperParameters> TransientShaperParameters::create(
    double attack,double sustain)
{
    using R=core::Result<TransientShaperParameters>;
    if(!std::isfinite(attack)||!std::isfinite(sustain))
        return R::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "L3_M03_TRANSIENT_SHAPER_NONFINITE_PARAMETER"});
    if(attack< -12.0||attack>12.0||sustain< -12.0||sustain>12.0)
        return R::failure(core::Error{
            core::ErrorCode::OutOfRange,
            "L3_M03_TRANSIENT_SHAPER_AMOUNT_OUT_OF_RANGE"});
    return R::success(TransientShaperParameters{attack,sustain});
}
} // namespace rgsml::dsp
