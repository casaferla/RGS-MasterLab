#pragma once

#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>

#include <variant>

namespace rgsml::dsp {

using ModuleParameterPayload = std::variant<
    GainParameters,
    ParametricEqParameters,
    CompressorParameters,
    StereoMsParameters,
    TruePeakLimiterParameters>;

}  // namespace rgsml::dsp
