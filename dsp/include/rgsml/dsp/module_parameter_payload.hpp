#pragma once

#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <variant>

namespace rgsml::dsp {

using ModuleParameterPayload = std::variant<
    GainParameters,
    ParametricEqParameters>;

}  // namespace rgsml::dsp
