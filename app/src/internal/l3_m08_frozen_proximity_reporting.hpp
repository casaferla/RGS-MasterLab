#pragma once
// L3-M08 P02-A/B/C frozen proximity reporting only. The caller's
// Gold-relative domain distances and close-population evidence MUST
// already be authoritative under M05/M06/M07 support/selector semantics;
// this layer never substitutes a 9-cell M21 proxy, reconstructs missing
// candidate components, authorizes M10 or M13, or labels PARTIAL as FULL.
#include "l3_m07_frozen_binding_projection.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rgsml::app::l3_automatch::m08_frozen {
enum class ProximityReason:std::uint8_t {
    comparable,proximity_already_matched,
    proximity_undefined_initial_already_matched_candidate_departure,
    candidate_required_component_missing,
    source_evidence_unavailable,
    invalid_distance
};
struct DomainProximity final {
    std::optional<double> raw;
    ProximityReason reason{ProximityReason::source_evidence_unavailable};
    [[nodiscard]] constexpr bool comparable() const noexcept {
        return raw.has_value()&&
            (reason==ProximityReason::comparable
             ||reason==ProximityReason::proximity_already_matched);
    }
};
struct DomainDistanceEvidence final {
    // Both booleans are attested on the SAME required full population
    // and M05/M06 frozen deadbands, not two independent per-component
    // "best available" sets. Recompute from M07/M08 exact identities.
    bool producer_current{};
    bool required_candidate_components_present{};
    bool prepared_already_within_frozen_close{};
    bool candidate_within_frozen_close{};
    std::optional<double> baseline_to_gold_distance;
    std::optional<double> candidate_to_gold_distance;
};
[[nodiscard]] inline DomainProximity compute_domain_proximity(
    const DomainDistanceEvidence& evidence)
{
    if(!evidence.producer_current)
        return {{},ProximityReason::source_evidence_unavailable};
    if(!evidence.required_candidate_components_present)
        return {{},ProximityReason::candidate_required_component_missing};
    if(!evidence.baseline_to_gold_distance||
       !evidence.candidate_to_gold_distance ||
       !std::isfinite(*evidence.baseline_to_gold_distance) ||
       !std::isfinite(*evidence.candidate_to_gold_distance) ||
       *evidence.baseline_to_gold_distance<0.0 ||
       *evidence.candidate_to_gold_distance<0.0)
        return {{},ProximityReason::invalid_distance};
    if(evidence.prepared_already_within_frozen_close) {
        if(evidence.candidate_within_frozen_close)
            return {1.0,ProximityReason::proximity_already_matched};
        return {{},
            ProximityReason::
            proximity_undefined_initial_already_matched_candidate_departure};
    }
    const double denominator=*evidence.baseline_to_gold_distance;
    if(denominator<=0.0)
        return {{},ProximityReason::invalid_distance};
    // MSVC /O2 may still diagnose a speculative constant-zero division
    // (C4723) after optional-value propagation. The positive/finite
    // preflight above is mandatory; this volatile read prevents only
    // invalid compile-time speculation, without changing raw arithmetic.
    const volatile double checked_positive_denominator=denominator;
    const double raw=1.0-
        *evidence.candidate_to_gold_distance/checked_positive_denominator;
    if(!std::isfinite(raw))
        return {{},ProximityReason::invalid_distance};
    return {raw,ProximityReason::comparable}; // RAW; no UI clip
}
struct RequestedDomainReport final {
    bool requested{};
    DomainProximity proximity;
    // Frozen M07/M10 effective structural coverage, authoritative
    // current evidence only; not recomputed from surviving candidate.
    std::optional<double> effective_coverage;
};
enum class GlobalCompleteness:std::uint8_t {
    complete,partial,no_comparable_domain,no_requested_domains,
    invalid_evidence
};
struct ConditionalGlobalReport final {
    std::array<std::optional<double>,4> fixed_contributions;
    std::optional<double> conditional_comparable_proximity;
    double requested_domain_mass{};
    double comparable_domain_mass{};
    double structural_coverage{};
    std::optional<double> effective_coverage;
    double known_effective_mass{};
    double unknown_effective_mass{};
    GlobalCompleteness completeness{GlobalCompleteness::invalid_evidence};
    [[nodiscard]] constexpr bool authorizes_full_match_publication()
        const noexcept{return false;} // later M12/M13 only
};
[[nodiscard]] inline ConditionalGlobalReport report_requested_domains(
    const std::array<RequestedDomainReport,4>& domain)
{
    ConditionalGlobalReport report;
    double raw_sum=0.0;
    double cover_sum=0.0;
    bool has_unknown=false;
    bool missing_required=false;
    for(std::size_t d=0;d<domain.size();++d) {
        const auto& item=domain[d];
        if(!item.requested)continue; // Amount=0 never implies false
        constexpr double weight=0.25;
        report.requested_domain_mass+=weight;
        if(item.effective_coverage) {
            if(!std::isfinite(*item.effective_coverage)
               ||*item.effective_coverage<0.0
               ||*item.effective_coverage>1.0) {
                report.completeness=GlobalCompleteness::invalid_evidence;
                return report;
            }
            report.known_effective_mass+=
                weight*(*item.effective_coverage);
            cover_sum+=weight*(*item.effective_coverage);
        }else{
            has_unknown=true;
            report.unknown_effective_mass+=weight;
        }
        if(!item.proximity.comparable()) {
            missing_required=true;
            if(item.proximity.raw) {
                report.completeness=GlobalCompleteness::invalid_evidence;
                return report;
            }
            continue;
        }
        if(!item.effective_coverage
           ||*item.effective_coverage<0.60
           ||!std::isfinite(*item.proximity.raw)) {
            report.completeness=GlobalCompleteness::invalid_evidence;
            return report; // no fake current comparable below gate
        }
        report.comparable_domain_mass+=weight;
        const double contribution=weight*(*item.proximity.raw);
        report.fixed_contributions[d]=contribution;
        raw_sum+=contribution;
    }
    if(report.requested_domain_mass==0.0) {
        report.completeness=GlobalCompleteness::no_requested_domains;
        return report;
    }
    report.structural_coverage=
        report.comparable_domain_mass/report.requested_domain_mass;
    if(!has_unknown)
        report.effective_coverage=
            cover_sum/report.requested_domain_mass;
    if(report.comparable_domain_mass==0.0) {
        report.completeness=GlobalCompleteness::no_comparable_domain;
        return report;
    }
    report.conditional_comparable_proximity=
        raw_sum/report.comparable_domain_mass;
    if(!std::isfinite(*report.conditional_comparable_proximity)) {
        report.conditional_comparable_proximity.reset();
        report.completeness=GlobalCompleteness::invalid_evidence;
        return report;
    }
    report.completeness=missing_required?
        GlobalCompleteness::partial:GlobalCompleteness::complete;
    return report;
}
} // namespace rgsml::app::l3_automatch::m08_frozen
