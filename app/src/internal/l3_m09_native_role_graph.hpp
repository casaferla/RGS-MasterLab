#pragma once
#include "l3_m09_abstract_family_enumeration.hpp"
#include "l3_m09_native_automatic_coordinate.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeRoleSeed final {
    IntentRole role;
    dsp::ModuleInstanceId instance;
    core::Uuid semantic;
    dsp::ModuleParameterPayload payload;
};
[[nodiscard]] inline std::optional<std::string_view>
native_artistic_role_type(IntentRole role) noexcept {
    switch(role) {
    case IntentRole::tonal_eq: case IntentRole::stereo_side_eq:return "rgsml.dsp.parametric-eq";
    case IntentRole::spectral_dynamic_eq:return "rgsml.dsp.dynamic-eq";
    case IntentRole::macro_compressor:return "rgsml.dsp.compressor";
    case IntentRole::micro_transient:return "rgsml.dsp.transient-shaper";
    case IntentRole::stereo_ms:return "rgsml.dsp.stereo-ms";
    case IntentRole::loudness_gain:return "rgsml.dsp.gain";
    case IntentRole::terminal_limiter:return std::nullopt;
    }
    return std::nullopt;
}
// Only the artistic segment is assembled here. A terminal-limiter
// alternative requires its separate owner-authorized composition;
// never silently discard it or turn WAV Export into a chain DSP.
[[nodiscard]] inline std::optional<m10_frozen::PreservedNativeGraph>
assemble_native_artistic_role_graph(
    const dsp::ModuleRegistry& registry,const AbstractFamilyChoice& family,
    std::span<const NativeRoleSeed> ordered_seeds,std::uint64_t revision)
{
    const auto roles=ordered_abstract_roles(family);
    if(roles.size()!=ordered_seeds.size())return std::nullopt;
    std::vector<dsp::ModuleInstance> nodes;
    std::vector<dsp::ModuleExecutionBinding> bindings;
    nodes.reserve(roles.size());bindings.reserve(roles.size());
    for(std::size_t i=0;i<roles.size();++i) {
        const auto& seed=ordered_seeds[i];
        const auto type=native_artistic_role_type(roles[i]);
        if(!type||seed.role!=roles[i]||seed.semantic.is_nil())return std::nullopt;
        for(std::size_t j=0;j<i;++j)
            if(ordered_seeds[j].instance==seed.instance||
               ordered_seeds[j].semantic==seed.semantic)return std::nullopt;
        if(!registry.has_factory(*type)||!registry.create_module(*type,seed.payload))
            return std::nullopt;
        auto node=dsp::ModuleInstance::create({seed.instance,std::string(*type),
            true,false,false,false,dsp::ModuleProvenance::REFERENCE_MATCH,
            dsp::ModuleOwner::REFERENCE_MATCH,dsp::ModuleLinkState::REF_LINKED,
            seed.semantic,{}});
        if(!node)return std::nullopt;
        nodes.push_back(std::move(*node.value()));
        bindings.push_back({seed.instance,seed.payload});
    }
    auto chain=dsp::ProcessingChain::restore(registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},
        revision,std::move(nodes));
    if(!chain)return std::nullopt;
    return m10_frozen::PreservedNativeGraph{std::move(*chain.value()),std::move(bindings)};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
