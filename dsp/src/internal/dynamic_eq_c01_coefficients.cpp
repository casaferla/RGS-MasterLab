#include "dynamic_eq_c01_coefficients.hpp"
#include <rgsml/core/error.hpp>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>

namespace rgsml::dsp::internal {
namespace {
[[nodiscard]] core::Error bad(std::string reason) {
    return core::Error{core::ErrorCode::InvalidArgument,
        "L3_M01_C01_COEFFICIENT_INVALID", {{"category",std::move(reason)}}};
}
[[nodiscard]] core::Result<BiquadCoefficients> normalized(
    double b0,double b1,double b2,double a0,double a1,double a2)
{
    const std::array<double,6> raw{b0,b1,b2,a0,a1,a2};
    for(const double x:raw)
        if(!std::isfinite(x))
            return core::Result<BiquadCoefficients>::failure(bad("NONFINITE_RAW"));
    if(a0==0.0)
        return core::Result<BiquadCoefficients>::failure(bad("ZERO_A0"));
    const BiquadCoefficients c{
        b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
    const std::array<double,5> v{c.b0,c.b1,c.b2,c.a1,c.a2};
    for(const double x:v)
        if(!std::isfinite(x))
            return core::Result<BiquadCoefficients>::failure(bad("NONFINITE_NORMALIZED"));
    // Second-order Jury stability after binary64 normalization. This is
    // instantaneous stability, not a substitute for the frozen B01
    // common-metric proof of the complete time-varying realization.
    if(!(1.0+c.a1+c.a2>0.0 &&
         1.0-c.a1+c.a2>0.0 &&
         1.0-c.a2>0.0))
        return core::Result<BiquadCoefficients>::failure(bad("UNSTABLE_SECTION"));
    return core::Result<BiquadCoefficients>::success(c);
}
} // namespace

C01CoefficientGenerator::C01CoefficientGenerator(
    DynamicEqFilterType type,double c,double s,double shape) noexcept
    :type_(type),c_(c),s_(s),shape_(shape) {}

core::Result<C01CoefficientGenerator> C01CoefficientGenerator::create(
    const DynamicEqBandParameters& band,
    std::uint32_t sample_rate_hz,std::uint32_t channels)
{
    using R=core::Result<C01CoefficientGenerator>;
    const auto checked=band.validate_for_audio(sample_rate_hz,channels);
    if(!checked)return R::failure(*checked.error());
    const auto& spec=band.spec();
    if(!spec.enabled)return R::failure(bad("DISABLED_BAND_NO_EXECUTION"));
    const double f=std::visit([](const auto& s){return s.frequency_hz;},
                              spec.shape);
    const double shape=spec.filter_type==DynamicEqFilterType::BELL
        ?std::get<DynamicEqBellShape>(spec.shape).q
        :std::get<DynamicEqShelfShape>(spec.shape).shelf_slope;
    const double w=2.0*std::numbers::pi*f/
        static_cast<double>(sample_rate_hz);
    const double c=std::cos(w),s=std::sin(w);
    if(!std::isfinite(c)||!std::isfinite(s)||!std::isfinite(shape)
       ||shape<=0.0||s<=0.0)
        return R::failure(bad("INVALID_PREPARED_SHAPE"));
    return R::success(C01CoefficientGenerator{spec.filter_type,c,s,shape});
}
core::Result<BiquadCoefficients>
C01CoefficientGenerator::audio_coefficients(double gain_db) const
{
    using R=core::Result<BiquadCoefficients>;
    if(!std::isfinite(gain_db)||gain_db < -18.0||gain_db >18.0)
        return R::failure(bad("EFFECTIVE_GAIN_OUT_OF_BOUNDS"));
    const double A=std::pow(10.0,gain_db/40.0);
    if(type_==DynamicEqFilterType::BELL) {
        const double alpha=s_/(2.0*shape_);
        return normalized(1.0+alpha*A,-2.0*c_,1.0-alpha*A,
                          1.0+alpha/A,-2.0*c_,1.0-alpha/A);
    }
    const double alpha=(s_/2.0)*
        std::sqrt((A+1.0/A)*(1.0/shape_-1.0)+2.0);
    const double beta=2.0*std::sqrt(A)*alpha;
    if(type_==DynamicEqFilterType::LOW_SHELF) {
        return normalized(
            A*((A+1.0)-(A-1.0)*c_+beta),
            2.0*A*((A-1.0)-(A+1.0)*c_),
            A*((A+1.0)-(A-1.0)*c_-beta),
            (A+1.0)+(A-1.0)*c_+beta,
            -2.0*((A-1.0)+(A+1.0)*c_),
            (A+1.0)+(A-1.0)*c_-beta);
    }
    if(type_==DynamicEqFilterType::HIGH_SHELF) {
        return normalized(
            A*((A+1.0)+(A-1.0)*c_+beta),
            -2.0*A*((A-1.0)+(A+1.0)*c_),
            A*((A+1.0)+(A-1.0)*c_-beta),
            (A+1.0)-(A-1.0)*c_+beta,
            2.0*((A-1.0)-(A+1.0)*c_),
            (A+1.0)-(A-1.0)*c_-beta);
    }
    return R::failure(bad("UNKNOWN_FILTER_TYPE"));
}
core::Result<BiquadCoefficients>
C01CoefficientGenerator::detector_coefficients() const
{
    const double alpha=type_==DynamicEqFilterType::BELL
        ? s_/(2.0*shape_)
        : s_/(2.0*(1.0/std::numbers::sqrt2));
    const double a0=1.0+alpha,a1=-2.0*c_,a2=1.0-alpha;
    if(type_==DynamicEqFilterType::BELL)
        return normalized(alpha,0.0,-alpha,a0,a1,a2);
    if(type_==DynamicEqFilterType::LOW_SHELF)
        return normalized((1.0-c_)/2.0,1.0-c_,(1.0-c_)/2.0,a0,a1,a2);
    if(type_==DynamicEqFilterType::HIGH_SHELF)
        return normalized((1.0+c_)/2.0,-(1.0+c_),(1.0+c_)/2.0,a0,a1,a2);
    return core::Result<BiquadCoefficients>::failure(bad("UNKNOWN_DETECTOR_TYPE"));
}
} // namespace rgsml::dsp::internal
