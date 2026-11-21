#pragma once
// L3-M05/M06/M10 frozen support-aware objective arithmetic only.
// Callers must provide authoritative FULL configured feature population,
// component masses, actual pairSupport and M08 optimizationTarget.
// Never use the provisional nine-cell M21 scalar probe as a substitute.
// This function does NOT infer available DSP topologies, perform GR/TP
// constraints, modify protected features or verify a materialized master.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
enum class Domain :std::uint8_t {
    tonal,dynamics,stereo,loudness
};
struct AuthoritativeComponent final {
    // m_i × a_ik, including ALL canonical configured components,
    // even if evidence/eligibility is missing; never shrink denom.
    double configured_mass{};
    bool eligible_for_correction{false};
    double pair_support{}; // validated [0,1], paired M05/M06 support
    double comparison_scale{}; // frozen native units (or log2)
    std::optional<double> candidate_z;
    std::optional<double> optimization_target_z;
    // Candidate dropout of a required component is an invalid candidate,
    // even when another binding can still satisfy the coverage gate.
    bool required_candidate_evidence{false};
};
struct DomainPopulation final {
    Domain domain{};
    bool requested_corrective{false};
    std::vector<AuthoritativeComponent> configured;
};
enum class TargetCostStatus :std::uint8_t {
    measured,
    no_correction_required,
    invalid_configured_population,
    missing_required_candidate,
    insufficient_effective_coverage,
    nonfinite_component
};
struct TargetCost final {
    TargetCostStatus status{TargetCostStatus::invalid_configured_population};
    std::optional<double> j_target;
    std::array<std::optional<double>,4> domain_loss{};
    std::array<std::optional<double>,4> effective_coverage{};
    [[nodiscard]] constexpr bool valid_measured() const noexcept {
        return status==TargetCostStatus::measured&&j_target.has_value();
    }
};
[[nodiscard]] inline TargetCost compute_frozen_target_cost(
    const std::array<DomainPopulation,4>& populations) noexcept
{
    TargetCost result;
    std::array<bool,4> seen{};
    double total=0.0,domain_weight=0.0;
    for(const auto& pop:populations) {
        const auto d=static_cast<std::size_t>(pop.domain);
        if(d>=4||seen[d]||pop.configured.empty())
            return result;
        seen[d]=true;
        double configured=0.0,effective=0.0,numerator=0.0;
        for(const auto& feature:pop.configured) {
            if(!std::isfinite(feature.configured_mass)
               ||feature.configured_mass<=0.0
               ||!std::isfinite(feature.pair_support)
               ||feature.pair_support<0.0||feature.pair_support>1.0
               ||!std::isfinite(feature.comparison_scale)
               ||feature.comparison_scale<=0.0)
                return result;
            configured+=feature.configured_mass;
            if(!std::isfinite(configured))return result;
            if(!pop.requested_corrective
               ||!feature.eligible_for_correction)
                continue;
            if(feature.required_candidate_evidence
               &&(!feature.candidate_z
                  ||!std::isfinite(*feature.candidate_z))) {
                result.status=TargetCostStatus::missing_required_candidate;
                return result;
            }
            const double mass=feature.configured_mass*feature.pair_support;
            if(!std::isfinite(mass))return result;
            if(mass==0.0)continue;
            if(!feature.candidate_z
               ||!feature.optimization_target_z
               ||!std::isfinite(*feature.candidate_z)
               ||!std::isfinite(*feature.optimization_target_z)) {
                result.status=TargetCostStatus::missing_required_candidate;
                return result;
            }
            // Finite normalized residual before robust min(square,16).
            // Saturate by magnitude>4 without overflowing large squares.
            const double residual=(*feature.candidate_z-
                *feature.optimization_target_z)/
                feature.comparison_scale;
            if(!std::isfinite(residual)) {
                result.status=TargetCostStatus::nonfinite_component;
                return result;
            }
            const double loss=std::abs(residual)>=4.0
                ?16.0:residual*residual;
            effective+=mass;
            numerator+=mass*loss;
            if(!std::isfinite(effective)||!std::isfinite(numerator))
                return result;
        }
        if(configured<=0.0||!std::isfinite(configured))
            return result;
        if(!pop.requested_corrective)continue;
        const double coverage=effective/configured;
        result.effective_coverage[d]=coverage;
        // Compare ratio with exact inclusive 0.60 threshold;
        // other configured/unavailable feature mass remains in denom.
        if(!std::isfinite(coverage)||coverage<0.60||effective<=0.0) {
            result.status=TargetCostStatus::insufficient_effective_coverage;
            return result;
        }
        const double loss=numerator/effective;
        if(!std::isfinite(loss))return result;
        result.domain_loss[d]=loss;
        constexpr double W=0.25; // equal frozen domain weights
        total+=W*loss;
        domain_weight+=W;
    }
    if(domain_weight==0.0) {
        result.status=TargetCostStatus::no_correction_required;
        return result; // No invented zero Jtarget for empty correction.
    }
    const double j=total/domain_weight;
    if(!std::isfinite(j))return result;
    result.j_target=j;
    result.status=TargetCostStatus::measured;
    return result;
}
} // namespace rgsml::app::l3_automatch::m10_frozen
