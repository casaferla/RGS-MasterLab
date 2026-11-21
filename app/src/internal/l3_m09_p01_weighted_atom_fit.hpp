#pragma once
// L3-M09 P01 ACCEPTED_FROZEN: pure atom projection, exact weighted
// full-population residual fit and deterministic tie ordering.
// Caller supplies the EXISTING canonical DSP static response at the
// bounded gain; +1dB response alone is NOT a final nonlinear fit.
// This helper never invents missing FFT/response/feature evidence.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
enum class AtomFilter:std::uint8_t {low_shelf,bell,high_shelf};
enum class AtomSeedReason:std::uint8_t {
    ready,seed_parameter_limited,missing_exact_static_response,
    invalid_active_population,unusable_zero_denominator,
    error_not_improved,below_minimum_relative_improvement
};
struct AtomProfileCell final {
    double residual_db{};
    double atom_plus_one_db_response{};
    double frozen_normalized_weight{};
};
struct AtomProjection final {
    std::optional<double> raw_gain_db;
    std::optional<double> limited_gain_db;
    AtomSeedReason reason{AtomSeedReason::invalid_active_population};
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept {return false;}
};
[[nodiscard]] inline AtomProjection project_atom_unweighted_g0(
    std::span<const AtomProfileCell> exact_active_population,
    bool stereo_side)
{
    if(exact_active_population.empty())
        return {{},{},AtomSeedReason::invalid_active_population};
    double numerator=0.0;
    double denominator=0.0;
    double weight_sum=0.0;
    for(const auto& c:exact_active_population) {
        if(!std::isfinite(c.residual_db)||
           !std::isfinite(c.atom_plus_one_db_response)||
           !std::isfinite(c.frozen_normalized_weight)||
           c.frozen_normalized_weight<=0.0)
            return {{},{},AtomSeedReason::invalid_active_population};
        numerator+=c.residual_db*c.atom_plus_one_db_response;
        denominator+=c.atom_plus_one_db_response*
                     c.atom_plus_one_db_response;
        weight_sum+=c.frozen_normalized_weight;
    }
    if(!std::isfinite(numerator)||!std::isfinite(denominator)||
       !std::isfinite(weight_sum)||weight_sum<=0.0)
        return {{},{},AtomSeedReason::invalid_active_population};
    if(denominator==0.0)
        return {{},{},AtomSeedReason::unusable_zero_denominator};
    // Unweighted g0 is frozen and distinct from weighted selection.
    const volatile double positive_denominator=denominator;
    const double raw=numerator/positive_denominator;
    if(!std::isfinite(raw))
        return {{},{},AtomSeedReason::invalid_active_population};
    const double limit=stereo_side?1.5:3.0;
    const double bounded=std::clamp(raw,-limit,limit);
    return {raw,bounded,raw==bounded?AtomSeedReason::ready:
               AtomSeedReason::seed_parameter_limited};
}
struct AtomFitEvidence final {
    AtomFilter filter{AtomFilter::bell};
    double canonical_frequency_hz{};
    double q_or_slope{};
    AtomProjection projection;
    // Actual canonical static EQ response at projection.limited_gain_db.
    // Supplies exactly one response for EVERY active comparison cell.
    std::vector<double> exact_canonical_response_at_limited_gain;
    bool exact_response_content_and_gain_attested{};
};
struct AtomFitResult final {
    AtomFitEvidence atom;
    std::optional<double> before_error;
    std::optional<double> after_error;
    std::optional<double> relative_improvement;
    AtomSeedReason reason{AtomSeedReason::missing_exact_static_response};
    [[nodiscard]] constexpr bool authorizes_master_publication()
        const noexcept{return false;}
};
[[nodiscard]] inline AtomFitResult evaluate_weighted_atom(
    std::span<const AtomProfileCell> exact_active_population,
    AtomFitEvidence source)
{
    AtomFitResult result;
    result.atom=std::move(source);
    if(!result.atom.projection.limited_gain_db||
       !result.atom.exact_response_content_and_gain_attested||
       result.atom.exact_canonical_response_at_limited_gain.size()
           !=exact_active_population.size())
        return result;
    if(exact_active_population.empty()||
       !std::isfinite(result.atom.canonical_frequency_hz)||
       result.atom.canonical_frequency_hz<=0.0||
       !std::isfinite(result.atom.q_or_slope)||
       result.atom.q_or_slope<=0.0) {
        result.reason=AtomSeedReason::invalid_active_population;
        return result;
    }
    double old_error=0.0;
    double new_error=0.0;
    for(std::size_t i=0;i<exact_active_population.size();++i) {
        const auto& cell=exact_active_population[i];
        const double actual=
            result.atom.exact_canonical_response_at_limited_gain[i];
        if(!std::isfinite(cell.residual_db)||
           !std::isfinite(cell.frozen_normalized_weight)||
           cell.frozen_normalized_weight<=0.0||
           !std::isfinite(actual)) {
            result.reason=AtomSeedReason::invalid_active_population;
            return result;
        }
        const double new_residual=cell.residual_db-actual;
        old_error+=cell.frozen_normalized_weight*
                   cell.residual_db*cell.residual_db;
        new_error+=cell.frozen_normalized_weight*
                   new_residual*new_residual;
    }
    if(!std::isfinite(old_error)||!std::isfinite(new_error)||
       old_error<0.0||new_error<0.0) {
        result.reason=AtomSeedReason::invalid_active_population;
        return result;
    }
    result.before_error=old_error;
    result.after_error=new_error;
    if(old_error==0.0) {
        result.reason=AtomSeedReason::below_minimum_relative_improvement;
        return result;
    }
    const volatile double positive_old_error=old_error;
    const double improvement=(old_error-new_error)/positive_old_error;
    if(!std::isfinite(improvement)) {
        result.reason=AtomSeedReason::invalid_active_population;
        return result;
    }
    result.relative_improvement=improvement;
    if(improvement<0.0)
        result.reason=AtomSeedReason::error_not_improved;
    else if(improvement<0.01)
        result.reason=AtomSeedReason::below_minimum_relative_improvement;
    else
        result.reason=result.atom.projection.reason==
            AtomSeedReason::seed_parameter_limited?
            AtomSeedReason::seed_parameter_limited:AtomSeedReason::ready;
    return result;
}
[[nodiscard]] inline bool usable_greedy_atom(const AtomFitResult& a)
    noexcept {
    return (a.reason==AtomSeedReason::ready||
            a.reason==AtomSeedReason::seed_parameter_limited)&&
           a.after_error.has_value();
}
// Same weighted error: LOW_SHELF, BELL, HIGH_SHELF, then increasing
// canonical frequency and Q/slope. This is selection order; the
// final persisted band order is a separate subsequent operation.
[[nodiscard]] inline bool atom_precedes_for_selection(
    const AtomFitResult& a,const AtomFitResult& b) noexcept {
    if(*a.after_error!=*b.after_error)
        return *a.after_error<*b.after_error;
    if(a.atom.filter!=b.atom.filter)
        return a.atom.filter<b.atom.filter;
    if(a.atom.canonical_frequency_hz!=b.atom.canonical_frequency_hz)
        return a.atom.canonical_frequency_hz<
               b.atom.canonical_frequency_hz;
    return a.atom.q_or_slope<b.atom.q_or_slope;
}
[[nodiscard]] inline std::optional<std::size_t>
best_weighted_eligible_atom(std::span<const AtomFitResult> atoms) {
    std::optional<std::size_t> best;
    for(std::size_t i=0;i<atoms.size();++i) {
        if(!usable_greedy_atom(atoms[i]))continue;
        if(!best||atom_precedes_for_selection(atoms[i],atoms[*best]))
            best=i;
    }
    return best;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
