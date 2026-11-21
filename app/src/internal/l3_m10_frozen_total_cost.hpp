#pragma once
// L3-M10 frozen numerical R/C/J aggregator. Active coordinate identity,
// reference positions and m/b counts come from M09/M11 authoritative
// ownership snapshots; never estimate them from default DSP schemas.
// Jtarget comes solely from frozen M05/M06/M08 objective, NOT M21 proxy.
#include "l3_m10_frozen_coordinate_refiner.hpp"
#include "l3_m10_frozen_target_objective.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
struct ActiveReferenceCoordinate final {
    double current_u{};
    double immutable_reference_u{}; // never rebase to incumbent
    double reference_weight_r{}; // positive, frozen P01 role
};
[[nodiscard]] inline std::optional<double> normalized_reference_distance(
    std::span<const ActiveReferenceCoordinate> active) noexcept
{
    if(active.size()>kMaxActiveCoordinates)return std::nullopt;
    if(active.empty())return 0.0; // frozen P03 R=0 convention
    double numerator=0.0,denominator=0.0;
    for(const auto& c:active) {
        if(!std::isfinite(c.current_u)
           ||!std::isfinite(c.immutable_reference_u)
           ||!std::isfinite(c.reference_weight_r)
           ||c.current_u<0.0||c.current_u>1.0
           ||c.immutable_reference_u<0.0
           ||c.immutable_reference_u>1.0
           ||c.reference_weight_r<=0.0)return std::nullopt;
        const double d=c.current_u-c.immutable_reference_u;
        const double term=c.reference_weight_r*d*d;
        numerator+=term;denominator+=c.reference_weight_r;
        if(!std::isfinite(numerator)||!std::isfinite(denominator)
           ||denominator<=0.0)return std::nullopt;
    }
    const double normalized=numerator/denominator;
    if(!std::isfinite(normalized)||normalized<0.0
       ||normalized>1.0)return std::nullopt;
    return normalized;
}
[[nodiscard]] inline std::optional<double> complexity_cost(
    std::uint32_t active_owned_modules,
    std::uint32_t active_artistic_bands) noexcept
{
    if(active_owned_modules>8||active_artistic_bands>12)
        return std::nullopt;
    return 0.5*static_cast<double>(active_owned_modules)/8.0+
           0.5*static_cast<double>(active_artistic_bands)/12.0;
}
[[nodiscard]] inline std::optional<MeasuredOrderKey> total_measured_cost(
    const TargetCost& target,
    std::span<const ActiveReferenceCoordinate> active,
    std::uint32_t active_owned_modules,
    std::uint32_t active_artistic_bands,
    std::vector<std::uint8_t> canonical_current_tuple,
    std::vector<std::uint8_t> topology_evidence_identity)
{
    if(!target.valid_measured())return std::nullopt;
    const auto R=normalized_reference_distance(active);
    const auto C=complexity_cost(
        active_owned_modules,active_artistic_bands);
    if(!R||!C)return std::nullopt;
    const double j=*target.j_target+0.02*(*R)+0.01*(*C);
    MeasuredOrderKey result{j,*target.j_target,*R,
                           active_owned_modules,
                           std::move(canonical_current_tuple),
                           std::move(topology_evidence_identity)};
    if(!valid_key(result))return std::nullopt;
    return result;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
