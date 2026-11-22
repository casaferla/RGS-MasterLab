#pragma once
// L3-M10 frozen numerical coordinate inventory -> authentic Product P2
// Ceiling/Drive typed binding. No invented normalization or step policy.
// M09 canonical inverse transformations must be supplied by the owner;
// M10's frozen 0..1 coordinates are NOT assumed to be linear decibels.
// This is a pre-render binding port, not a 27-feature scorer, M12 Project
// transaction, M13 verifier or M17 WAV export controller.
#include "l3_m10_frozen_coordinate_refiner.hpp"
#include "l3_automatch_typed_p2_trial.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {

struct ExactP2CoordinateNames final {
    // The same immutable semantic node is used for both scalar fields.
    std::vector<std::uint8_t> semantic_node_id;
    std::vector<std::uint8_t> ceiling_parameter_id;
    std::vector<std::uint8_t> drive_parameter_id;
};

struct BoundM10P2Candidate final {
    BoundP2JointTrial typed;
    double normalized_ceiling_u{};
    double normalized_drive_u{};
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept {return false;}
};

// `decode` takes one exact canonical Coordinate and the proposed M10
// normalized u, returns an optional PHYSICAL dB value according to the
// already-authorized M09 inverse transform and legal bounds. M10 supplies
// the entire trial coordinate vector; OTHER DSP values remain with the
// full candidate Product worker. This function NEVER renders an excerpt
// or scores M21's nine display metrics.
template<typename DecodePhysical>
[[nodiscard]] inline std::optional<BoundM10P2Candidate>
bind_m10_p2_joint_coordinates(
    std::span<const Coordinate> canonical_inventory,
    std::span<const double> proposed_normalized_values,
    const ExactP2CoordinateNames& exact,
    const dsp::ProcessingChain& current_protected_manual,
    std::span<const dsp::ModuleExecutionBinding> current_bindings,
    const dsp::ModuleInstanceId& exact_p2_instance,
    std::string exact_ticket_identity,
    DecodePhysical&& decode)
{
    if(canonical_inventory.empty()||
       canonical_inventory.size()!=proposed_normalized_values.size()||
       canonical_inventory.size()>kMaxActiveCoordinates||
       exact.semantic_node_id.empty()||
       exact.ceiling_parameter_id.empty()||
       exact.drive_parameter_id.empty()||
       exact.ceiling_parameter_id==exact.drive_parameter_id)
        return std::nullopt;
    std::optional<std::size_t> ceiling_index;
    std::optional<std::size_t> drive_index;
    for(std::size_t i=0;i<canonical_inventory.size();++i) {
        const auto& coord=canonical_inventory[i];
        const double u=proposed_normalized_values[i];
        if(coord.semantic_node_id.empty()||
           coord.stable_parameter_id.empty()||
           !std::isfinite(u)||u<0.0||u>1.0)
            return std::nullopt;
        // Every named P2 coordinate is mandatory; an unknown third
        // (e.g. Margin) or repeated P2 coordinate fails closed.
        if(coord.semantic_node_id!=exact.semantic_node_id)continue;
        if(coord.stable_parameter_id==exact.ceiling_parameter_id) {
            if(ceiling_index)return std::nullopt;
            ceiling_index=i;
        } else if(coord.stable_parameter_id==exact.drive_parameter_id) {
            if(drive_index)return std::nullopt;
            drive_index=i;
        } else return std::nullopt;
    }
    if(!ceiling_index||!drive_index)return std::nullopt;
    const auto physical_ceiling=std::invoke(
        decode,canonical_inventory[*ceiling_index],
        proposed_normalized_values[*ceiling_index]);
    const auto physical_drive=std::invoke(
        decode,canonical_inventory[*drive_index],
        proposed_normalized_values[*drive_index]);
    if(!physical_ceiling||!physical_drive||
       !std::isfinite(*physical_ceiling)||
       !std::isfinite(*physical_drive))
        return std::nullopt;
    auto typed=make_p2_joint_trial(
        current_protected_manual,current_bindings,exact_p2_instance,
        *physical_ceiling,*physical_drive,std::move(exact_ticket_identity));
    if(!typed)return std::nullopt;
    return BoundM10P2Candidate{
        std::move(*typed),
        proposed_normalized_values[*ceiling_index],
        proposed_normalized_values[*drive_index]};
}

} // namespace rgsml::app::l3_automatch::m10_frozen
