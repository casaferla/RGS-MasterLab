#pragma once
// Immutable native M09 seed graph + exact canonical coordinate map.
// All transforms, reference anchors and stable parameter IDs come from
// M09's accepted inventory, not the current incumbent or GUI defaults.
#include "l3_m09_native_coordinate_payload.hpp"
#include "l3_m10_preserved_native_refinement.hpp"
#include "l3_m10_frozen_coordinate_transform.hpp"
#include <set>
#include <algorithm>

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeSeedCoordinate final {
    m10_frozen::Coordinate canonical;
    m10_frozen::CoordinateTransform transform;
    dsp::ModuleInstanceId instance_id;
    NativeField field;
    std::optional<core::Uuid> band_id;
    double immutable_reference_u{};
    double reference_weight{};
    core::Uuid exact_allocated_semantic_node_id;
};
struct NativeTypedSeed final {
    std::shared_ptr<const dsp::ModuleRegistry> registry;
    m10_frozen::PreservedNativeGraph seed;
    std::vector<NativeSeedCoordinate> coordinates;
};
struct DecodedNativeSeedTrial final {
    m10_frozen::PreservedNativeGraph graph;
    std::vector<m10_frozen::ActiveReferenceCoordinate> references;
};
[[nodiscard]] inline std::optional<DecodedNativeSeedTrial>
decode_native_typed_seed(
    const NativeTypedSeed& seed,
    std::span<const m10_frozen::Coordinate> canonical_inventory,
    std::span<const double> normalized)
{
    if(!seed.registry||
       seed.seed.linked.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED}||
       seed.seed.linked.instances().size()!=seed.seed.bindings.size()||
       seed.coordinates.size()!=canonical_inventory.size()||
       canonical_inventory.size()!=normalized.size()||
       !m10_frozen::m10_valid_sorted_coordinate_inventory(canonical_inventory))
        return std::nullopt;
    const auto nodes=seed.seed.linked.instances();
    auto bindings=seed.seed.bindings;
    std::vector<std::vector<NativePhysicalField>> fields(nodes.size());
    std::vector<m10_frozen::ActiveReferenceCoordinate> reference;
    reference.reserve(normalized.size());
    std::set<std::pair<std::vector<std::uint8_t>,std::vector<std::uint8_t>>> used;
    for(std::size_t i=0;i<canonical_inventory.size();++i) {
        const auto& c=canonical_inventory[i];
        const auto same=[&](const NativeSeedCoordinate& x){
            return x.canonical.semantic_node_id==c.semantic_node_id&&
                   x.canonical.stable_parameter_id==c.stable_parameter_id;
        };
        const auto found=std::find_if(seed.coordinates.begin(),seed.coordinates.end(),same);
        if(found==seed.coordinates.end()||
           std::count_if(seed.coordinates.begin(),seed.coordinates.end(),same)!=1||
           !used.emplace(c.semantic_node_id,c.stable_parameter_id).second||
           !std::isfinite(found->reference_weight)||found->reference_weight<=0.0||
           !std::isfinite(found->immutable_reference_u)||
           found->immutable_reference_u<0.0||found->immutable_reference_u>1.0)
            return std::nullopt;
        const auto node=std::find_if(nodes.begin(),nodes.end(),[&](const auto& n){
            return n.instance_id()==found->instance_id;
        });
        if(node==nodes.end()||!node->active()||!node->semantic_node_id()||
           found->exact_allocated_semantic_node_id.is_nil()||
           *node->semantic_node_id()!=found->exact_allocated_semantic_node_id)
            return std::nullopt;
        const auto index=static_cast<std::size_t>(node-nodes.begin());
        const auto physical_seed=read_native_physical_field(
            bindings[index].parameters,found->field,found->band_id);
        if(!physical_seed||
           found->transform.to_normalized(*physical_seed)!=
               std::optional<double>{found->canonical.normalized_u})
            return std::nullopt;
        const auto physical=normalized[i]==found->canonical.normalized_u
            ?physical_seed:found->transform.from_normalized(normalized[i]);
        if(!physical)return std::nullopt;
        fields[index].push_back({found->field,found->band_id,*physical});
        reference.push_back({normalized[i],found->immutable_reference_u,found->reference_weight});
    }
    for(std::size_t i=0;i<nodes.size();++i) {
        if(!binding_matches_instance(nodes[i],bindings[i])||
           nodes[i].owner()!=dsp::ModuleOwner::REFERENCE_MATCH||
           nodes[i].provenance()!=dsp::ModuleProvenance::REFERENCE_MATCH||
           nodes[i].link_state()!=dsp::ModuleLinkState::REF_LINKED)
            return std::nullopt;
        auto payload=apply_native_physical_fields(bindings[i].parameters,fields[i]);
        if(!payload||!seed.registry->has_factory(nodes[i].module_type_id())||
           !seed.registry->create_module(nodes[i].module_type_id(),*payload))
            return std::nullopt;
        bindings[i].parameters=std::move(*payload);
    }
    return DecodedNativeSeedTrial{
        {seed.seed.linked,std::move(bindings)},std::move(reference)};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
