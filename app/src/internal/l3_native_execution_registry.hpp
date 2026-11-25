#pragma once
#include <rgsml/dsp/module_registry.hpp>
#include <memory>
#include <optional>

namespace rgsml::app::l3_automatch {
// Extend the caller-owned snapshot, never the live MANUAL registry.
// Keep existing descriptor/codec authorities; no duplicate factory
// registry and no implicit activation of Streaming TP Limiter.
[[nodiscard]] inline std::optional<std::shared_ptr<const dsp::ModuleRegistry>>
freeze_native_automatch_execution_registry(std::shared_ptr<const dsp::ModuleRegistry> live)
{
    if(!live)return {};
    if(live->has_factory("rgsml.dsp.dynamic-eq")&&
       live->has_factory("rgsml.dsp.transient-shaper"))return live;
    auto registry=*live;
    if(!registry.has_factory("rgsml.dsp.dynamic-eq")) {
        auto next=dsp::ModuleRegistry::with_dynamic_eq_offline(std::move(registry));
        if(!next)return {};
        registry=std::move(*next.value());
    }
    if(!registry.has_factory("rgsml.dsp.transient-shaper")) {
        auto next=dsp::ModuleRegistry::with_transient_shaper_offline(std::move(registry));
        if(!next)return {};
        registry=std::move(*next.value());
    }
    return std::make_shared<const dsp::ModuleRegistry>(std::move(registry));
}
} // namespace rgsml::app::l3_automatch
