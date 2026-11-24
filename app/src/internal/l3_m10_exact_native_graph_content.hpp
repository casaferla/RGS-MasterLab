#pragma once
// Exact typed graph serialization for a measured native candidate.
// The existing DSP parameter codec is the sole source of parameter
// bytes. Graph identity is not a substitute for full M07-M11 scope.
#include "l3_m10_p2_exact_cache_evaluator.hpp"
#include "l3_automatch_typed_gain_trial.hpp"
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <optional>
#include <span>

namespace rgsml::app::l3_automatch::m10_frozen {
[[nodiscard]] inline std::optional<ExactCacheBytes>
exact_native_graph_content(
    const dsp::ProcessingChain& chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleRegistry& registry)
{
    const auto nodes=chain.instances();
    if(nodes.size()!=bindings.size())return std::nullopt;
    ExactCacheBytes out;
    using p2_cache_detail::append_exact_field;
    append_exact_field(out,"rgsml.l3.native-typed-graph/1");
    append_exact_field(out,std::to_string(static_cast<unsigned>(chain.context().stage)));
    append_exact_field(out,std::to_string(static_cast<unsigned>(chain.context().segment)));
    append_exact_field(out,std::to_string(chain.revision()));
    append_exact_field(out,std::to_string(nodes.size()));
    for(std::size_t i=0;i<nodes.size();++i) {
        const auto& n=nodes[i];
        if(!binding_matches_instance(n,bindings[i]))return std::nullopt;
        const auto descriptor=registry.find_descriptor(n.module_type_id());
        const auto params=dsp::encode_module_parameters_json(bindings[i].parameters);
        if(!descriptor||!params||!registry.has_factory(n.module_type_id()))
            return std::nullopt;
        append_exact_field(out,n.instance_id().uuid().to_string());
        append_exact_field(out,n.module_type_id());
        append_exact_field(out,descriptor.value()->get().algorithm_version());
        append_exact_field(out,descriptor.value()->get().parameter_schema_id());
        append_exact_field(out,n.semantic_node_id()?n.semantic_node_id()->to_string():"");
        out.push_back(n.enabled()?1U:0U);
        out.push_back(n.user_bypass()?1U:0U);
        out.push_back(n.controller_suspended()?1U:0U);
        out.push_back(n.domain_suspended()?1U:0U);
        out.push_back(static_cast<std::uint8_t>(n.provenance()));
        out.push_back(static_cast<std::uint8_t>(n.owner()));
        out.push_back(static_cast<std::uint8_t>(n.link_state()));
        append_exact_field(out,*params.value());
    }
    return out;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
