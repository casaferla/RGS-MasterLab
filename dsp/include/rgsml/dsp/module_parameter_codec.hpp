#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_parameter_payload.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <string>
#include <string_view>

namespace rgsml::dsp {

[[nodiscard]] rgsml::core::Result<std::string> encode_module_parameters_json(
    const ModuleParameterPayload& payload);

[[nodiscard]] rgsml::core::Result<std::string> encode_gain_parameters_json(
    const GainParameters& params);

[[nodiscard]] rgsml::core::Result<std::string> encode_parametric_eq_parameters_json(
    const ParametricEqParameters& params);

[[nodiscard]] rgsml::core::Result<std::string> encode_compressor_parameters_json(
    const CompressorParameters& params);

[[nodiscard]] rgsml::core::Result<ModuleParameterPayload> decode_module_parameters_json(
    std::string_view schema_id,
    std::string_view json_text);

[[nodiscard]] rgsml::core::Result<GainParameters> decode_gain_parameters_json(
    std::string_view json_text);

[[nodiscard]] rgsml::core::Result<ParametricEqParameters> decode_parametric_eq_parameters_json(
    std::string_view json_text);

[[nodiscard]] rgsml::core::Result<CompressorParameters> decode_compressor_parameters_json(
    std::string_view json_text);

}  // namespace rgsml::dsp
