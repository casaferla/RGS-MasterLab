#include "transient_shaper_effective_memory.hpp"
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace rgsml::dsp::internal {
core::Result<core::FrameCount>
transient_shaper_conservative_precontext_frames(std::uint32_t fs)
{
    using R=core::Result<core::FrameCount>;
    if(fs!=44100U&&fs!=48000U&&fs!=96000U&&fs!=192000U)
        return R::failure(core::Error{
            core::ErrorCode::UnsupportedOperation,
            "L3_M03_EFFECTIVE_CONTEXT_UNSUPPORTED_RATE"});
    constexpr double epsilon=1e-9;
    constexpr double eta=1e-6;
    constexpr double delta=epsilon*eta/192.0;
    const double B=std::numeric_limits<double>::max();
    const auto N=[fs](double tau,double amplitude,double tolerance) {
        const double value=static_cast<double>(fs)*tau*
            (std::log(amplitude)-std::log(tolerance));
        return std::max(0.0,std::ceil(value));
    };
    const double nf=N(0.030,B,delta/2.0);
    const double nm=N(0.010,B,delta/2.0);
    const double nl=N(0.150,B,delta);
    const double ng=N(0.00025,48.0,eta/2.0);
    const double H=std::max(nf+nm,nl)+ng;
    if(!std::isfinite(H)||H<0.0
       ||H>=static_cast<double>(
           std::numeric_limits<std::int64_t>::max()))
        return R::failure(core::Error{
            core::ErrorCode::IntegerOverflow,
            "L3_M03_EFFECTIVE_CONTEXT_OVERFLOW"});
    return core::FrameCount::create(static_cast<std::int64_t>(H));
}
} // namespace rgsml::dsp::internal
