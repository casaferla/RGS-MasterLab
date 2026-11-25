#pragma once
#include "l3_m09_native_selected_family_search.hpp"
#include "l3_m10_exact_native_graph_content.hpp"
#include <bit>
#include <set>
#include <tuple>

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
// M09 description only: the combined consumer owns the 48-coordinate
// exclusion. This never creates a valid M10 trial ticket.
[[nodiscard]] inline std::optional<std::vector<m10_frozen::Coordinate>>
exact_over_budget_description_inventory(const NativeTypedSeed& seed)
{
    if(!seed.registry||seed.coordinates.size()<=m10_frozen::kMaxActiveCoordinates)return {};
    std::vector<m10_frozen::Coordinate> out;
    std::set<std::tuple<dsp::ModuleInstanceId,NativeField,std::optional<core::Uuid>>> fields;
    for(const auto& c:seed.coordinates) {
        if(c.canonical.semantic_node_id.empty()||c.canonical.stable_parameter_id.empty()||
           !std::isfinite(c.canonical.normalized_u)||c.canonical.normalized_u<0.0||
           c.canonical.normalized_u>1.0||
           !fields.emplace(c.instance_id,c.field,c.band_id).second)return {};
        out.push_back(c.canonical);
    }
    std::sort(out.begin(),out.end(),m10_frozen::m10_canonical_coordinate_less);
    for(std::size_t i=1;i<out.size();++i)
        if(!m10_frozen::m10_canonical_coordinate_less(out[i-1U],out[i]))return {};
    return out;
}
[[nodiscard]] inline std::optional<ExactBytes> exact_over_budget_seed_content(
    const NativeTypedSeed& seed)
{
    const auto inventory=exact_over_budget_description_inventory(seed);
    if(!inventory)return {};
    const auto graph=m10_frozen::exact_native_graph_content(seed.seed.linked,seed.seed.bindings,*seed.registry);
    if(!graph)return {};
    ExactBytes out;
    using namespace m10_frozen::p2_cache_detail;
    append_exact_field(out,"rgsml.l3.native-family-over-budget/1");
    append_exact_byte_field(out,*graph);
    append_exact_field(out,std::to_string(inventory->size()));
    for(const auto& c:*inventory) {
        append_exact_byte_field(out,c.semantic_node_id);
        append_exact_byte_field(out,c.stable_parameter_id);
        append_exact_field(out,std::to_string(std::bit_cast<std::uint64_t>(c.normalized_u)));
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
// The Product path uses the actual typed codec for both seed and
// refinement keys. No caller-provided arbitrary current tuple.
[[nodiscard]] inline NativeProductSearchResult run_exact_selected_native_family_search(
    const m10_frozen::BoundLegalCombinedResult& selected,
    std::span<const SelectedNativeFamilyRecipe> recipes,
    const NativeFamilySeedBank& bank,std::uint64_t revision,std::int64_t rate,
    const m10_frozen::PreservedNativeSearchEnvironment& env,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    bool independent_fixed_safety_scope_current,
    const std::atomic<bool>* cancellation=nullptr)
{
    const auto registry=env.owners.frozen_registry;
    if(!registry)return {};
    return run_selected_native_family_search(selected,recipes,registry,bank,revision,rate,
        env,amount,requested,independent_fixed_safety_scope_current,
        [&](const auto& graph,const auto& inventory){
            return exact_family_seed_content(graph,*registry,inventory);
        },
        [&](const auto& description,const auto& graph,const auto& inventory,const auto& values){
            return exact_family_trial_key(description,graph,*registry,inventory,values);
        },cancellation);
}
} // namespace rgsml::app::l3_automatch::m09_frozen
