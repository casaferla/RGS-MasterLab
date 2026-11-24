#pragma once
#include "l3_m09_native_selected_family_search.hpp"
#include "l3_m10_exact_native_graph_content.hpp"
#include <bit>

namespace rgsml::app::l3_automatch::m09_frozen {
[[nodiscard]] inline std::optional<ExactBytes> exact_family_current_content(
    const m10_frozen::PreservedNativeGraph& graph,const dsp::ModuleRegistry& registry,
    std::span<const m10_frozen::Coordinate> inventory,std::span<const double> values)
{
    if(inventory.size()!=values.size()||
       !m10_frozen::m10_valid_sorted_coordinate_inventory(inventory))return {};
    const auto graph_content=m10_frozen::exact_native_graph_content(graph.linked,graph.bindings,registry);
    if(!graph_content)return {};
    ExactBytes out;
    using namespace m10_frozen::p2_cache_detail;
    append_exact_field(out,"rgsml.l3.native-family-current/1");
    append_exact_byte_field(out,*graph_content);
    append_exact_field(out,std::to_string(inventory.size()));
    for(std::size_t i=0;i<inventory.size();++i) {
        const auto& c=inventory[i];
        if(!std::isfinite(values[i])||values[i]<0.0||values[i]>1.0)return {};
        append_exact_byte_field(out,c.semantic_node_id);
        append_exact_byte_field(out,c.stable_parameter_id);
        append_exact_field(out,std::to_string(std::bit_cast<std::uint64_t>(c.normalized_u)));
        append_exact_field(out,std::to_string(std::bit_cast<std::uint64_t>(values[i])));
    }
    return out;
}
[[nodiscard]] inline std::optional<ExactBytes> exact_family_seed_content(
    const m10_frozen::PreservedNativeGraph& graph,const dsp::ModuleRegistry& registry,
    std::span<const m10_frozen::Coordinate> inventory)
{
    std::vector<double> values;
    for(const auto& c:inventory)values.push_back(c.normalized_u);
    return exact_family_current_content(graph,registry,inventory,values);
}
[[nodiscard]] inline std::optional<m10_frozen::ExactExecutionCacheKey>
exact_family_trial_key(const CandidateCarrier& description,
    const m10_frozen::PreservedNativeGraph& graph,const dsp::ModuleRegistry& registry,
    std::span<const m10_frozen::Coordinate> inventory,std::span<const double> values)
{
    if(description.semantic_tuple.empty()||description.seed_exact_content.empty()||
       description.dependency_tuple.empty()||description.generation_snapshot.empty())return {};
    const auto current=exact_family_current_content(graph,registry,inventory,values);
    if(!current)return {};
    using namespace m10_frozen::p2_cache_detail;
    ExactBytes topology;
    append_exact_field(topology,"rgsml.l3.native-family-topology/1");
    append_exact_byte_field(topology,description.semantic_tuple);
    append_exact_byte_field(topology,description.seed_exact_content);
    append_exact_byte_field(topology,description.dependency_tuple);
    return m10_frozen::ExactExecutionCacheKey{description.generation_snapshot,*current,*current,std::move(topology)};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
