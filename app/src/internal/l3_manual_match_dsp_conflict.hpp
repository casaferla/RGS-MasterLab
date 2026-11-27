#pragma once

#include <rgsml/dsp/module_instance.hpp>
#include <algorithm>
#include <span>
#include <string_view>

namespace rgsml::app::l3_automatch {
// Bypassed MANUAL nodes are permanent neutral editor slots and must not
// prevent AutoMatch from owning a real correction. An active user-owned
// C01/C00 is protected, not implicitly bypassed or overwritten by a plan.
[[nodiscard]] inline bool protected_manual_match_type(
    std::span<const dsp::ModuleInstance> manual,std::string_view type)
{
    if(type!="rgsml.dsp.dynamic-eq" && type!="rgsml.dsp.transient-shaper")
        return false;
    return std::any_of(manual.begin(),manual.end(),[&](const auto& node){
        return node.active() && node.module_type_id()==type;
    });
}
[[nodiscard]] inline bool conflicting_active_match_dsps(
    std::span<const dsp::ModuleInstance> manual,
    std::span<const dsp::ModuleInstance> linked)
{
    return std::any_of(linked.begin(),linked.end(),[&](const auto& node){
        return node.active() && protected_manual_match_type(manual,node.module_type_id());
    });
}
} // namespace rgsml::app::l3_automatch
