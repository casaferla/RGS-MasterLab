#pragma once
#include "l3_m11_native_fixed_assessment.hpp"
#include <rgsml/dsp/gain_parameters.hpp>
#include <algorithm>
#include <cmath>
#include <optional>

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeLoudnessGainSeed final {
    dsp::GainParameters payload;
    double raw_gain_db{};
    double estimated_tp_db{};
    bool seed_parameter_limited{};
    bool terminal_branch_requested{};
};
// Recovered M09 section 7.6: use L01 alone, not L02 or scoped L03.
// M16 estimate is not final safety and does not instantiate a limiter.
// Current generation and graph are checked by the Product owner.
[[nodiscard]] inline std::optional<NativeLoudnessGainSeed>
derive_native_loudness_gain_seed(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,
    const std::array<bool,4>& requested,
    double project_ceiling_db_tp)
{
    if(!std::isfinite(project_ceiling_db_tp)||project_ceiling_db_tp>0.0||
       !fixed.measured_m16_linear_peak||
       !std::isfinite(*fixed.measured_m16_linear_peak)||
       *fixed.measured_m16_linear_peak<=0.0)return std::nullopt;
    const auto targets=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    constexpr auto domain=static_cast<std::size_t>(m07_frozen::Domain::loudness);
    if(!targets.valid_current||!targets.corrective[domain])return std::nullopt;
    const auto protection=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!protection)return std::nullopt;
    std::size_t component=0;
    for(std::size_t i=0;i<fixed.measured_fixed.size();++i) {
        const auto& slot=fixed.measured_fixed[i];
        if(slot.binding_id!="rgsml.reference-match.binding.loudness.l01") {
            component+=slot.configured.size();continue;
        }
        if(slot.configured.size()!=1U||component>=protection->size())return std::nullopt;
        const auto& part=slot.configured.front();
        const auto& guard=(*protection)[component];
        if(!part.current_valid||!guard.required_in_frozen_k||
           guard.protected_by_m11_mask||!part.current_candidate_z)
            return std::nullopt;
        const auto desired=m08_frozen::desired_for_component(amount,domain,part.prepared,part.reference);
        if(!desired.value)return std::nullopt;
        const double raw=desired.value->constraint_adjusted_z.value_or(desired.value->desired_z)-
            *part.current_candidate_z;
        if(!std::isfinite(raw))return std::nullopt;
        const double bounded=std::clamp(raw,-6.0,6.0);
        const double estimate=20.0*std::log10(*fixed.measured_m16_linear_peak)+bounded;
        const auto payload=dsp::GainParameters::create(bounded);
        if(!payload||!std::isfinite(estimate))return std::nullopt;
        return NativeLoudnessGainSeed{*payload.value(),raw,estimate,raw!=bounded,
                                     estimate>project_ceiling_db_tp};
    }
    return std::nullopt;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
