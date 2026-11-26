#pragma once
// L3-M13 recovered P02: compare ACTUAL fresh, frozen M08 raw proximity
// with the immutable accepted M10 prediction; do not recompute artistic
// distances with a new similarity model, clip to [0,1], or repair a plan.
// Both limits are in fraction units, not percentage points.
#include "l3_m08_frozen_proximity_reporting.hpp"
#include "l3_m13_qtp_27_projection.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rgsml::app::l3_automatch::m13_frozen {
enum class ProximityComparisonStatus:std::uint8_t {
    passed,failed,unavailable,not_applicable
};
struct ProximityComparison final {
    ProximityComparisonStatus status{ProximityComparisonStatus::unavailable};
    std::optional<double> absolute_raw_delta;
    std::optional<double> allowable_raw_delta;
    [[nodiscard]] bool passed() const noexcept {
        return status==ProximityComparisonStatus::passed&&
            absolute_raw_delta&&allowable_raw_delta&&
            std::isfinite(*absolute_raw_delta)&&
            std::isfinite(*allowable_raw_delta)&&
            *absolute_raw_delta>=0.0&&
            (*allowable_raw_delta==kSameRunProximityFractionBound||
             *allowable_raw_delta==kIndependentPlatformProximityFractionBound)&&
            *absolute_raw_delta<=*allowable_raw_delta;
    }
};
[[nodiscard]] inline ProximityComparison
compare_exact_m08_raw_proximity(
    const m08_frozen::DomainProximity& predicted,
    const m08_frozen::DomainProximity& verified,
    bool same_run_not_independent_platform)
{
    if(predicted.reason!=verified.reason)
        return {ProximityComparisonStatus::failed,{},{}};
    if(!predicted.comparable()||!verified.comparable()) {
        // An unavailable reason must not carry a fabricated raw value.
        // Missing comparable values remain unavailable rather than a
        // numerical mismatch, consistently with the frozen M08 report.
        if((predicted.raw&&!predicted.comparable())||
           (verified.raw&&!verified.comparable()))
            return {ProximityComparisonStatus::failed,{},{}};
        return {ProximityComparisonStatus::unavailable,{},{}};
    }
    const double a=*predicted.raw,b=*verified.raw;
    if(predicted.reason==m08_frozen::ProximityReason::proximity_already_matched&&
       (a!=1.0||b!=1.0))
        return {ProximityComparisonStatus::failed,{},{}};
    if(!std::isfinite(a)||!std::isfinite(b)||a>1.0||b>1.0)
        return {ProximityComparisonStatus::failed,{},{}};
    const double bound=same_run_not_independent_platform
        ?kSameRunProximityFractionBound
        :kIndependentPlatformProximityFractionBound;
    const double delta=std::abs(a-b);
    if(!std::isfinite(delta))
        return {ProximityComparisonStatus::failed,{},{}};
    return {delta<=bound?ProximityComparisonStatus::passed:
        ProximityComparisonStatus::failed,delta,bound};
}
struct FullM08Comparison final {
    std::array<ProximityComparison,4> domains;
    ProximityComparison global;
    bool all_requested_domains_comparable{};
    bool full_global_population{};
    [[nodiscard]] bool all_required_pass() const noexcept {
        if(!all_requested_domains_comparable||!full_global_population||
           !global.passed()||
           global.allowable_raw_delta!=kSameRunProximityFractionBound)return false;
        bool any_requested=false;
        for(const auto& d:domains) {
            if(d.status==ProximityComparisonStatus::not_applicable) {
                if(d.absolute_raw_delta||d.allowable_raw_delta)return false;
                continue;
            }
            if(!d.passed()||d.allowable_raw_delta!=kSameRunProximityFractionBound)
                return false;
            any_requested=true;
        }
        return any_requested;
    }
};
[[nodiscard]] inline FullM08Comparison
compare_frozen_m08_prediction_with_fresh_verified(
    const std::array<m08_frozen::RequestedDomainReport,4>& predicted,
    const std::array<m08_frozen::RequestedDomainReport,4>& verified)
{
    FullM08Comparison out;
    out.all_requested_domains_comparable=true;
    std::array<m08_frozen::RequestedDomainReport,4> exact_predicted=predicted;
    std::array<m08_frozen::RequestedDomainReport,4> exact_verified=verified;
    for(std::size_t i=0;i<4;++i) {
        const auto& a=predicted[i],&b=verified[i];
        if(a.requested!=b.requested) {
            out.domains[i]={ProximityComparisonStatus::failed,{},{}};
            out.all_requested_domains_comparable=false;
            continue;
        }
        if(!a.requested) {
            out.domains[i]={ProximityComparisonStatus::not_applicable,{},{}};
            continue;
        }
        // Same accepted M08 effective coverage is exact dependency,
        // not a gain to be silently rediscovered from surviving bands.
        if(!a.effective_coverage||!b.effective_coverage||
           !std::isfinite(*a.effective_coverage)||
           !std::isfinite(*b.effective_coverage)) {
            out.domains[i]={ProximityComparisonStatus::unavailable,{},{}};
            out.all_requested_domains_comparable=false;
            continue;
        }
        if(*a.effective_coverage<0.0||*a.effective_coverage>1.0||
           *b.effective_coverage<0.0||*b.effective_coverage>1.0||
           *a.effective_coverage!=*b.effective_coverage) {
            out.domains[i]={ProximityComparisonStatus::failed,{},{}};
            out.all_requested_domains_comparable=false;
            continue;
        }
        out.domains[i]=compare_exact_m08_raw_proximity(
            a.proximity,b.proximity,true);
        if(!out.domains[i].passed())
            out.all_requested_domains_comparable=false;
    }
    const auto p=m08_frozen::report_requested_domains(exact_predicted);
    const auto v=m08_frozen::report_requested_domains(exact_verified);
    out.full_global_population=
        p.completeness==m08_frozen::GlobalCompleteness::complete&&
        v.completeness==m08_frozen::GlobalCompleteness::complete;
    if(p.completeness==m08_frozen::GlobalCompleteness::invalid_evidence||
       v.completeness==m08_frozen::GlobalCompleteness::invalid_evidence||
       p.completeness!=v.completeness) {
        out.global={ProximityComparisonStatus::failed,{},{}};
        return out;
    }
    if(!out.full_global_population||
       !p.conditional_comparable_proximity||
       !v.conditional_comparable_proximity) {
        out.global={ProximityComparisonStatus::unavailable,{},{}};
        return out;
    }
    const auto a=*p.conditional_comparable_proximity;
    const auto b=*v.conditional_comparable_proximity;
    if(!std::isfinite(a)||!std::isfinite(b)) {
        out.global={ProximityComparisonStatus::failed,{},{}};
        return out;
    }
    const double delta=std::abs(b-a);
    out.global={delta<=kSameRunProximityFractionBound
        ?ProximityComparisonStatus::passed:
         ProximityComparisonStatus::failed,
        delta,kSameRunProximityFractionBound};
    return out;
}
} // namespace rgsml::app::l3_automatch::m13_frozen
