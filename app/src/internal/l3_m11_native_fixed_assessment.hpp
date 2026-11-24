#pragma once
// M11 P02 whole-vector Fixed assessment. Component protection alone
// does not establish the aggregate directional/orthogonal conjunction.
#include "l3_m11_native_fixed_population.hpp"
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace rgsml::app::l3_automatch::m11_frozen {
enum class FixedTargetStatus : std::uint8_t {
    unavailable,already_close,met,residual,not_requested
};
struct FixedFeatureAssessment final {
    FixedTargetStatus status{FixedTargetStatus::unavailable};
    std::optional<double> directional_progress;
    std::optional<double> orthogonal_error;
    std::optional<double> orthogonal_tolerance;
    std::optional<double> prepared_gold_distance;
    std::optional<double> fixed_gold_distance;
};
[[nodiscard]] inline FixedFeatureAssessment assess_fixed_feature(
    const m08_frozen::AuthenticatedPairSlot& slot,
    const m07_frozen::Binding& binding,double amount)
{
    FixedFeatureAssessment out;
    const auto n=slot.configured.size();
    if(n==0U||!std::isfinite(amount)||amount<0.0||amount>1.0||
       (binding.component_share==m07_frozen::ComponentShare::one&&n!=1U)||
       (binding.component_share==m07_frozen::ComponentShare::thirds&&n!=3U))
        return out;
    if(amount==0.0){out.status=FixedTargetStatus::not_requested;return out;}
    struct Part{double delta,move,b,w;bool directional;};
    std::vector<Part> parts;
    double mass=0.0;
    for(const auto& c:slot.configured) {
        if(!std::isfinite(c.paired_native_support)||c.paired_native_support<0.0||
           c.paired_native_support>1.0)return out;
        if(!c.current_valid||c.paired_native_support==0.0)continue;
        if(!c.prepared.z||!c.reference.z||!c.current_candidate_z)return out;
        const double delta=(*c.reference.z-*c.prepared.z)/binding.scale;
        const double move=(*c.current_candidate_z-*c.prepared.z)/binding.scale;
        const double b=binding.deadband/binding.scale;
        if(!std::isfinite(delta)||!std::isfinite(move)||!std::isfinite(b))
            return out;
        parts.push_back({delta,move,b,c.paired_native_support,std::abs(delta)>b});
        mass+=c.paired_native_support;
    }
    if(parts.empty()||!std::isfinite(mass)||mass<=0.0)return out;
    // Within this single configured vector, all structural component
    // shares are equal. The inherited normalized effective weights are
    // proportional to the original paired support, never Fixed survivors.
    const volatile double denominator=mass;
    for(auto& p:parts)p.w/=denominator;
    if(binding.component_share==m07_frozen::ComponentShare::one) {
        const auto& p=parts.front();
        if(!p.directional) {
            out.status=std::abs(p.move-p.delta)<=p.b
                ?FixedTargetStatus::already_close:FixedTargetStatus::residual;
            return out;
        }
        const volatile double d=p.delta;
        const double q=p.move/d;
        if(!std::isfinite(q))return out;
        out.directional_progress=q;
        out.status=q>=amount?FixedTargetStatus::met:FixedTargetStatus::residual;
        return out;
    }
    double dot=0.0,norm=0.0,tol2=0.0,pdist2=0.0,fdist2=0.0;
    bool guards=true,has_direction=false;
    for(const auto& p:parts) {
        const double remaining=p.move-p.delta;
        pdist2+=p.w*p.delta*p.delta;
        fdist2+=p.w*remaining*remaining;
        guards=guards&&std::abs(remaining)<=std::abs(p.delta)+p.b;
        if(p.directional) {
            has_direction=true;dot+=p.w*p.move*p.delta;
            norm+=p.w*p.delta*p.delta;tol2+=p.w*p.b*p.b;
        }else guards=guards&&std::abs(remaining)<=p.b;
    }
    if(!std::isfinite(pdist2)||!std::isfinite(fdist2)||
       !std::isfinite(norm)||!std::isfinite(dot)||!std::isfinite(tol2))
        return out;
    out.prepared_gold_distance=std::sqrt(pdist2);
    out.fixed_gold_distance=std::sqrt(fdist2);
    if(!has_direction) {
        out.status=guards?FixedTargetStatus::already_close:FixedTargetStatus::residual;
        return out;
    }
    if(norm<=0.0)return out;
    const volatile double checked_norm=norm;
    const double q=dot/checked_norm;
    if(!std::isfinite(q))return out;
    double error2=0.0;
    for(const auto& p:parts)if(p.directional) {
        const double e=p.move-q*p.delta;
        error2+=p.w*e*e;
    }
    if(!std::isfinite(error2))return out;
    out.directional_progress=q;
    out.orthogonal_error=std::sqrt(error2);
    out.orthogonal_tolerance=std::sqrt(tol2);
    out.status=q>=amount-1e-9&&
        *out.orthogonal_error<=*out.orthogonal_tolerance&&
        *out.fixed_gold_distance<=*out.prepared_gold_distance+*out.orthogonal_tolerance&&guards
        ?FixedTargetStatus::met:FixedTargetStatus::residual;
    return out;
}
struct NativeFixedTargetAssessment final {
    bool valid_current{};
    std::array<bool,4> requested{};
    std::array<bool,4> corrective{};
    std::array<std::optional<double>,4> coverage{};
    std::vector<FixedFeatureAssessment> features;
    bool all_requested_targets_met{};
};
[[nodiscard]] inline NativeFixedTargetAssessment assess_native_fixed_targets(
    const NativeFixedPopulation& fixed,const m08_frozen::AmountConfig& amount,
    const std::array<bool,4>& requested)
{
    NativeFixedTargetAssessment out;
    out.requested=requested;
    if(!fixed.exact_independent_fixed_and_endpoint_safety_attested||
       amount.independent_amount_revision==0U||!std::isfinite(amount.global)||
       amount.global<0.0||amount.global>1.0||
       fixed.measured_fixed.size()!=m07_frozen::kBindings.size())return out;
    std::array<bool,4> active{},target_met{true,true,true,true};
    for(std::size_t d=0;d<4U;++d) {
        const auto a=m08_frozen::effective_amount(amount,d);
        if(!std::isfinite(amount.domains[d])||amount.domains[d]<0.0||
           amount.domains[d]>1.0||a.positive_underflow_to_zero)return out;
        active[d]=requested[d]&&a.rounded>0.0;
    }
    const auto population=m08_frozen::build_targeted_population(
        amount,fixed.measured_fixed,active);
    if(!population.value||!bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount))
        return out;
    out.features.reserve(fixed.measured_fixed.size());
    for(std::size_t i=0;i<fixed.measured_fixed.size();++i) {
        const auto& binding=m07_frozen::kBindings[i];
        const auto domain=static_cast<std::size_t>(binding.domain);
        auto assessment=assess_fixed_feature(fixed.measured_fixed[i],binding,
            active[domain]?m08_frozen::effective_amount(amount,domain).rounded:0.0);
        if(active[domain]&&assessment.status!=FixedTargetStatus::met&&
           assessment.status!=FixedTargetStatus::already_close)target_met[domain]=false;
        out.features.push_back(std::move(assessment));
    }
    out.all_requested_targets_met=true;
    for(std::size_t d=0;d<4U;++d) {
        if(!active[d])continue;
        double configured=0.0,effective=0.0;
        for(const auto& c:population.value->objective_input[d].configured) {
            configured+=c.configured_mass;
            if(c.eligible_for_correction)effective+=c.configured_mass*c.pair_support;
        }
        if(!std::isfinite(configured)||configured<=0.0||!std::isfinite(effective))
            return NativeFixedTargetAssessment{};
        const volatile double denominator=configured;
        out.coverage[d]=effective/denominator;
        if(*out.coverage[d]<0.60||!target_met[d]) {
            out.corrective[d]=true;
            out.all_requested_targets_met=false;
        }
    }
    out.valid_current=true;
    return out;
}
} // namespace rgsml::app::l3_automatch::m11_frozen
