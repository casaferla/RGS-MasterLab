#pragma once

#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/module_parameter_payload.hpp>

namespace rgsml::dsp {

struct ModuleExecutionBinding final {
    ModuleInstanceId instance_id;
    ModuleParameterPayload parameters;

    friend bool operator==(const ModuleExecutionBinding&, const ModuleExecutionBinding&) = default;
};

}  // namespace rgsml::dsp
