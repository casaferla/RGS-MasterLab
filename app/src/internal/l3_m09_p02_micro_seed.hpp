#pragma once
// L3-M09 P02 frozen D07/D08/D09 Micro analytical seed (nonrelease).
// Caller must supply exact Fixed-output and M08 optimized coordinates,
// M11 protection and authoritative event/context evidence. D09 governs
// compatibility/ambiguity, NEVER a transient time-knob seed or coverage.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rgsml::app::l3_automatch::m09_frozen {
struct MicroComponent final {
    bool requested{};
    bool protected_from_action{};
    bool exact_current{};
    double fixed_z{};
    double optimization_z{};
    double frozen_deadband{};
    double frozen_component_mass{};
    double paired_support{};
};
using MicroTriplet=std::array<MicroComponent,3>; // P10/P50/P90
enum class MicroReason:std::uint8_t {
    ready,limited_to_seed_range,missing_exact_fixed_or_event_evidence,
    invalid_component,missing_non_deadband_movement,
    incoherent_vector_direction,conflicting_d09_rise_direction
};
struct MicroSeed final {
    std::optional<double> raw_db;
    std::optional<double> limited_db;
    MicroReason reason{MicroReason::missing_exact_fixed_or_event_evidence};
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept {return false;}
};
namespace micro_detail {
struct Group final {
    double delta{};
    int sign{};
    MicroReason reason{MicroReason::missing_non_deadband_movement};
};
[[nodiscard]] inline Group reduce_group(const MicroTriplet& components) {
    int sign=0;
    double sum=0.0;
    double mass=0.0;
    for(const auto& c:components) {
        if(!c.requested||c.protected_from_action)continue;
        if(!c.exact_current)
            return {0,0,MicroReason::missing_exact_fixed_or_event_evidence};
        if(!std::isfinite(c.fixed_z)||
           !std::isfinite(c.optimization_z)||
           !std::isfinite(c.frozen_deadband)||c.frozen_deadband<0.0||
           !std::isfinite(c.frozen_component_mass)||
           c.frozen_component_mass<=0.0||
           !std::isfinite(c.paired_support)||
           c.paired_support<0.0||c.paired_support>1.0)
            return {0,0,MicroReason::invalid_component};
        if(c.paired_support==0.0)continue;
        const double movement=c.optimization_z-c.fixed_z;
        if(!std::isfinite(movement))
            return {0,0,MicroReason::invalid_component};
        if(std::abs(movement)<=c.frozen_deadband)continue;
        const int direction=movement>0.0?1:-1;
        if(sign!=0&&sign!=direction)
            return {0,0,MicroReason::incoherent_vector_direction};
        sign=direction;
        const double w=c.frozen_component_mass*c.paired_support;
        sum+=w*movement;
        mass+=w;
    }
    if(sign==0||!std::isfinite(sum)||!std::isfinite(mass)||
       mass<=0.0)
        return {0,0,sign==0?
            MicroReason::missing_non_deadband_movement:
            MicroReason::invalid_component};
    const double delta=sum/mass;
    if(!std::isfinite(delta))
        return {0,0,MicroReason::invalid_component};
    return {delta,sign,MicroReason::ready};
}
[[nodiscard]] inline MicroReason check_d09_compatibility(
    const MicroTriplet& d09, bool exact_event_context) {
    if(!exact_event_context)
        return MicroReason::missing_exact_fixed_or_event_evidence;
    int rise_sign=0;
    for(const auto& c:d09) {
        if(!c.requested||c.protected_from_action)continue;
        if(!c.exact_current)
            return MicroReason::missing_exact_fixed_or_event_evidence;
        if(!std::isfinite(c.fixed_z)||!std::isfinite(c.optimization_z)||
           !std::isfinite(c.frozen_deadband)||c.frozen_deadband<0.0||
           !std::isfinite(c.paired_support)||
           c.paired_support<0.0||c.paired_support>1.0)
            return MicroReason::invalid_component;
        if(c.paired_support==0.0)continue;
        const double direction=c.optimization_z-c.fixed_z;
        if(!std::isfinite(direction))return MicroReason::invalid_component;
        if(std::abs(direction)<=c.frozen_deadband)continue;
        const int next=direction>0.0?1:-1;
        if(rise_sign!=0&&rise_sign!=next)
            return MicroReason::conflicting_d09_rise_direction;
        rise_sign=next;
    }
    return MicroReason::ready;
}
} // namespace micro_detail

[[nodiscard]] inline MicroSeed derive_micro_p02(
    const MicroTriplet& d07_transient_contrast,
    const MicroTriplet& d08_local_crest,
    const MicroTriplet& d09_rise_time_compatibility,
    bool exact_fixed_protected_and_m08_target,
    bool exact_event_context)
{
    if(!exact_fixed_protected_and_m08_target)
        return {{},{},
            MicroReason::missing_exact_fixed_or_event_evidence};
    const auto d09=micro_detail::check_d09_compatibility(
        d09_rise_time_compatibility,exact_event_context);
    if(d09!=MicroReason::ready)return {{},{},d09};
    const auto contrast=micro_detail::reduce_group(d07_transient_contrast);
    if(contrast.reason!=MicroReason::ready)
        return {{},{},contrast.reason};
    const auto crest=micro_detail::reduce_group(d08_local_crest);
    if(crest.reason!=MicroReason::ready)return {{},{},crest.reason};
    if(contrast.sign!=crest.sign)
        return {{},{},MicroReason::incoherent_vector_direction};
    // Frozen P02 seed mixture: these coefficients are NOT objective
    // weights or an Amount-to-knob mapping.
    const double raw=0.70*contrast.delta+0.30*crest.delta;
    if(!std::isfinite(raw))return {{},{},MicroReason::invalid_component};
    const double bounded=raw>3.0?3.0:raw< -3.0?-3.0:raw;
    return {raw,bounded,raw!=bounded?
        MicroReason::limited_to_seed_range:MicroReason::ready};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
