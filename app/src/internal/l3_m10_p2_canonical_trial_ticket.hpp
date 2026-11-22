#pragma once
// L3-M10: exact per-topology/per-vector ticket for real native P2
// trials. The original frozen refiner SORTS its working Coordinate
// objects before sending u[], so both this identity and the M09
// inverse must consume that SAME canonical sorted inventory.
// This ticket is an identity only, NOT a cache key, objective or
// permission to publish a Project/M13-verified master.
#include "l3_m10_frozen_coordinate_refiner.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
[[nodiscard]] inline bool m10_canonical_coordinate_less(
    const Coordinate& a,const Coordinate& b)
{
    if(a.semantic_node_id!=b.semantic_node_id)
        return std::lexicographical_compare(
            a.semantic_node_id.begin(),a.semantic_node_id.end(),
            b.semantic_node_id.begin(),b.semantic_node_id.end());
    return std::lexicographical_compare(
        a.stable_parameter_id.begin(),a.stable_parameter_id.end(),
        b.stable_parameter_id.begin(),b.stable_parameter_id.end());
}
[[nodiscard]] inline bool m10_valid_sorted_coordinate_inventory(
    std::span<const Coordinate> coords) noexcept
{
    if(coords.empty()||coords.size()>kMaxActiveCoordinates)
        return false;
    for(std::size_t i=0;i<coords.size();++i) {
        if(coords[i].semantic_node_id.empty()||
           coords[i].stable_parameter_id.empty()||
           !std::isfinite(coords[i].normalized_u)||
           coords[i].normalized_u<0.0||coords[i].normalized_u>1.0)
            return false;
        if(i>0U&&!m10_canonical_coordinate_less(coords[i-1U],coords[i]))
            return false;
    }
    return true;
}
[[nodiscard]] inline std::optional<std::vector<Coordinate>>
m10_sort_exact_inventory(std::span<const Coordinate> coords)
{
    if(coords.empty()||coords.size()>kMaxActiveCoordinates)
        return std::nullopt;
    std::vector<Coordinate> sorted(coords.begin(),coords.end());
    std::sort(sorted.begin(),sorted.end(),m10_canonical_coordinate_less);
    if(!m10_valid_sorted_coordinate_inventory(sorted))
        return std::nullopt;
    return sorted;
}
namespace trial_ticket_detail {
inline void hex_byte(std::string& out,std::uint8_t byte)
{
    constexpr char kHex[]="0123456789abcdef";
    out.push_back(kHex[(byte>>4U)&0xFU]);
    out.push_back(kHex[byte&0xFU]);
}
inline void append_identity(std::string& out,
                            const std::vector<std::uint8_t>& bytes)
{
    out+=std::to_string(bytes.size());
    out+=':';
    for(const auto b:bytes)hex_byte(out,b);
    out+=';';
}
inline void append_binary64(std::string& out,double v)
{
    const auto bits=std::bit_cast<std::uint64_t>(v);
    for(unsigned byte=0;byte<8U;++byte)
        hex_byte(out,static_cast<std::uint8_t>(
            (bits>>(8U*byte))&0xFFU));
}
} // namespace trial_ticket_detail

[[nodiscard]] inline std::optional<std::string>
m10_exact_native_trial_ticket(
    std::size_t exact_combined_top32_ordinal,
    std::span<const Coordinate> canonically_sorted_inventory,
    std::span<const double> proposed_normalized_u)
{
    if(!m10_valid_sorted_coordinate_inventory(
           canonically_sorted_inventory)||
       proposed_normalized_u.size()!=canonically_sorted_inventory.size())
        return std::nullopt;
    std::string ticket{"rgsml.l3.m10.p2.trial/1;"};
    ticket+=std::to_string(exact_combined_top32_ordinal);
    ticket+=';';
    ticket+=std::to_string(canonically_sorted_inventory.size());
    ticket+=';';
    for(std::size_t i=0;i<canonically_sorted_inventory.size();++i) {
        const auto& c=canonically_sorted_inventory[i];
        const auto u=proposed_normalized_u[i];
        if(!std::isfinite(u)||u<0.0||u>1.0)
            return std::nullopt;
        trial_ticket_detail::append_identity(ticket,c.semantic_node_id);
        trial_ticket_detail::append_identity(ticket,c.stable_parameter_id);
        trial_ticket_detail::append_binary64(ticket,u);
        ticket+=';';
    }
    return ticket;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
