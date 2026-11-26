#pragma once

#include <rgsml/dsp/linked_dsp_activity_observation.hpp>
#include <rgsml/dsp/module_instance.hpp>

namespace rgsml::render {

// Borrowed sinks belong to the synchronous render caller, never to QML.
// Callers must discard every observation if the complete render fails.
// No sink is requested for bypassed/inactive modules. Identity is per node,
// not per algorithm, so two instances cannot share a history accidentally.
class LinkedDspActivitySinkProvider {
public:
    virtual ~LinkedDspActivitySinkProvider()=default;
    [[nodiscard]] virtual dsp::internal::IC01ActivitySink*
    dynamic_eq_sink(const dsp::ModuleInstanceId& instance) noexcept=0;
    [[nodiscard]] virtual dsp::internal::ITransientC00ActivitySink*
    transient_shaper_sink(const dsp::ModuleInstanceId& instance) noexcept=0;
};

} // namespace rgsml::render
