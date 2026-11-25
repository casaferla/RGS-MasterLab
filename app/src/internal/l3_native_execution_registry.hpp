#pragma once
#include <rgsml/dsp/module_registry.hpp>
#include <algorithm>
#include <memory>
#include <optional>

namespace rgsml::app::l3_automatch {
// Extend the caller-owned snapshot, never the live MANUAL registry.
// Keep existing descriptor/codec authorities; no duplicate factory.
// P2 becomes available for final REF_LINKED candidates only in this
// immutable offline snapshot; no implicit activation in the live chain.
[[nodiscard]] inline std::optional<std::shared_ptr<const dsp::ModuleRegistry>>
freeze_native_automatch_execution_registry(std::shared_ptr<const dsp::ModuleRegistry> live)
{
    if(!live)return {};
    const auto linked_p2=[](const dsp::ModuleRegistry& registry){
        const auto descriptor=registry.find_descriptor("rgsml.dsp.streaming-true-peak-limiter");
        if(!descriptor||!registry.has_factory("rgsml.dsp.streaming-true-peak-limiter"))return false;
        const auto segments=descriptor.value()->get().allowed_segments();
        return std::find(segments.begin(),segments.end(),dsp::ChainSegment::REF_LINKED)!=segments.end();
    };
    if(live->has_factory("rgsml.dsp.dynamic-eq")&&
       live->has_factory("rgsml.dsp.transient-shaper")&&linked_p2(*live))return live;
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
    if(!linked_p2(registry)) {
        auto next=dsp::ModuleRegistry::with_streaming_true_peak_automatch(std::move(registry));
        if(!next)return {};
        registry=std::move(*next.value());
    }
    return std::make_shared<const dsp::ModuleRegistry>(std::move(registry));
}
} // namespace rgsml::app::l3_automatch
