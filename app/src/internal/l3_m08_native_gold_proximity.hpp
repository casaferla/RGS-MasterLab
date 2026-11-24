#pragma once
// M08 recovered §11 distance, with frozen P02-A/B/C boundaries.
// The accepted P/G comparison population supplies weights and coverage;
// never substitute the M10 optimization target or candidate support.
#include "l3_m08_frozen_targeted_population.hpp"
#include "l3_m08_frozen_proximity_reporting.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace rgsml::app::l3_automatch::m08_frozen {
struct NativeProximityPopulationResult final {
    bool canonical_population_current{};
    std::array<RequestedDomainReport,4> domains;
    std::array<DomainDistanceEvidence,4> distances;
};
[[nodiscard]] inline NativeProximityPopulationResult
measure_native_gold_proximity(
    const std::vector<AuthenticatedPairSlot>& paired,
    const std::array<bool,4>& requested,
    bool exact_native_comparison_population_current)
{
    NativeProximityPopulationResult out;
    for(std::size_t d=0;d<4;++d)out.domains[d].requested=requested[d];
    if(!exact_native_comparison_population_current||
       paired.size()!=m07_frozen::kBindings.size())return out;
    struct Acc final {
        double configured{},effective{},p_loss{},c_loss{};
        bool p_close{true},c_close{true},candidate_present{true},
             source_valid{true},numeric_valid{true};
        std::size_t comparable{};
    };
    std::array<Acc,4> totals;
    for(std::size_t i=0;i<paired.size();++i) {
        const auto& slot=paired[i];
        const auto& binding=m07_frozen::kBindings[i];
        const auto n=slot.configured.size();
        if(slot.binding_id!=binding.binding_id||
           (slot.source_feature_id!=binding.source_feature_id&&
            (binding.exclusive_fallback_feature_id.empty()||
             slot.source_feature_id!=binding.exclusive_fallback_feature_id))||
           !slot.feature_authority_attested||
           slot.exact_feature_dependency.empty()||n==0U||
           (binding.component_share==m07_frozen::ComponentShare::one&&n!=1U)||
           (binding.component_share==m07_frozen::ComponentShare::thirds&&n!=3U))
            return out;
        auto& a=totals[static_cast<std::size_t>(binding.domain)];
        const volatile double component_count=static_cast<double>(n);
        const double mass=static_cast<double>(binding.domain_mass.numerator)/
            static_cast<double>(binding.domain_mass.denominator)/
            component_count;
        for(const auto& c:slot.configured) {
            a.configured+=mass;
            if(!std::isfinite(c.paired_native_support)||
               c.paired_native_support<0.0||c.paired_native_support>1.0)
                return out;
            if(!c.current_valid)continue; // retains configured denominator
            if(!c.prepared.z||!c.reference.z||
               !std::isfinite(*c.prepared.z)||!std::isfinite(*c.reference.z)) {
                a.source_valid=false;continue;
            }
            const auto weight=mass*c.paired_native_support;
            if(weight==0.0)continue;
            ++a.comparable;
            a.effective+=weight;
            const double p_delta=*c.prepared.z-*c.reference.z;
            const double p_residual=p_delta/binding.scale;
            if(!std::isfinite(p_residual)) {a.numeric_valid=false;continue;}
            const double p_loss=std::abs(p_residual)>=4.0?
                16.0:p_residual*p_residual;
            a.p_loss+=weight*p_loss;
            a.p_close=a.p_close&&std::abs(p_delta)<=binding.deadband;
            // Any eligible positive-mass component belongs to the
            // comparison population even if a consumer did not need it.
            if(!c.current_candidate_z||!std::isfinite(*c.current_candidate_z)) {
                a.candidate_present=false;continue;
            }
            const double c_delta=*c.current_candidate_z-*c.reference.z;
            const double c_residual=c_delta/binding.scale;
            if(!std::isfinite(c_residual)) {a.numeric_valid=false;continue;}
            const double c_loss=std::abs(c_residual)>=4.0?
                16.0:c_residual*c_residual;
            a.c_loss+=weight*c_loss;
            a.c_close=a.c_close&&std::abs(c_delta)<=binding.deadband;
        }
    }
    out.canonical_population_current=true;
    for(std::size_t d=0;d<4;++d) {
        if(!requested[d])continue;
        const auto& a=totals[d];
        auto& report=out.domains[d];
        auto& evidence=out.distances[d];
        if(!a.source_valid||!std::isfinite(a.configured)||a.configured<=0.0)
            continue;
        const volatile double configured=a.configured;
        const double coverage=a.effective/configured;
        if(!std::isfinite(coverage)||coverage<0.0||coverage>1.0)continue;
        report.effective_coverage=coverage;
        if(coverage<0.60||a.comparable==0U||a.effective<=0.0)continue;
        evidence.producer_current=true;
        evidence.required_candidate_components_present=a.candidate_present;
        evidence.prepared_already_within_frozen_close=a.p_close;
        evidence.candidate_within_frozen_close=a.c_close;
        if(a.numeric_valid) {
            const volatile double effective=a.effective;
            evidence.baseline_to_gold_distance=std::sqrt(a.p_loss/effective);
            if(a.candidate_present)
                evidence.candidate_to_gold_distance=std::sqrt(a.c_loss/effective);
        }
        report.proximity=compute_domain_proximity(evidence);
    }
    return out;
}
} // namespace rgsml::app::l3_automatch::m08_frozen
