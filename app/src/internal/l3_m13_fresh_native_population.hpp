#pragma once
// M13 independently re-rendered Master -> ORIGINAL M05/M06/M07.
// No numerical candidate fields from the accepted M10 plan/resolver
// survive this binding. This does not grant final QTP/VERIFIED.
#include "l3_m13_fresh_original_native_import.hpp"
#include "l3_m10_p2_verified_candidate_triple.hpp"
#include "l3_m11_p03_protected_no_regression.hpp"
#include "l3_m08_native_gold_proximity.hpp"
#include "l3_m13_predicted_vs_verified_proximity.hpp"
#include "l3_m13_27_binding_conformance.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m13_frozen {
struct M13FreshNativePopulation final {
    m07_frozen::PartialFeatureInventory inventory;
    std::optional<std::vector<m08_frozen::AuthenticatedPairSlot>> native_population;
    [[nodiscard]] constexpr bool authorizes_m13_verified()const noexcept
    {return false;}
};
[[nodiscard]] inline M13FreshNativePopulation
bind_m13_fresh_native_population(
    const M13ImmutableProductFreshProbeRequest& request,
    const M13FreshProductMasterProbe& fresh,
    const OriginalPreparedReferenceNativeFacts& independent_pair,
    const OfflineProductOwnerStamp& exact_source_reference_owner,
    m10_frozen::P2OriginalRealNativeTriple original_definitions,
    std::string exact_phase6_m07_generation,
    const m05m06_real::SourceIdentity& accepted_candidate_owner,
    const std::string& exact_accepted_source_reference_scope_revision,
    bool current_product_dependencies_attested,
    bool exact_file_based_lra_policy_attested)
{
    const auto& replay=request.exact_materialized_preview_owner().replay;
    if(!replay||exact_accepted_source_reference_scope_revision.empty()||
       replay->exact_source_reference_scope_revision!=
           exact_accepted_source_reference_scope_revision||
       !current_product_dependencies_attested||
       exact_phase6_m07_generation.empty()||
       !fresh.additional_native||
       !original_pair_matches_product_owner(independent_pair,
                                             exact_source_reference_owner))
        return {};
    const auto& additional=*fresh.additional_native;
    const auto view=fresh.fresh_full_master_pcm.view();
    if(view.frame_count().value()<=0||
       additional.exact_phase6_active_frames>
         static_cast<std::uint64_t>(view.frame_count().value())||
       additional.selected_original_m20_ordinals.size()!=
          fresh.fresh_m20.selected_spectral_windows)
        return {};
    // Import the real M19/M20 values, then use the same original
    // projections as M10 for the native series. Do not re-analyze M20.
    const auto actual=import_m13_product_fresh_original_native_source(
        request,fresh,{},{},0,false,false);
    if(!actual)return {};
    const auto& measured=actual.value()->fresh_full_m19_m20;
    if(!m10_frozen::rebind_original_candidate_owner(
           original_definitions,accepted_candidate_owner,measured.owner))return {};
    if(!m10_frozen::rebind_independent_original_native_pair(
        original_definitions,independent_pair,measured.owner))
        return {};
    auto bound=m10_frozen::bind_original_native_triple_from_measured_source(
        std::move(original_definitions),measured,fresh.fresh_m20,view,
        additional.d07_d09,additional.d10_d12,additional.s03_s06,
        measured.owner,
        static_cast<std::int64_t>(additional.exact_phase6_active_frames),
        exact_file_based_lra_policy_attested);
    if(!bound.fragments)return {};
    m07_frozen::ExactTwentySevenFeatureAssembler assembly{
        std::move(exact_phase6_m07_generation),true};
    const auto admission=m07_frozen::collect_existing_m05m06_native_fragments(
        assembly,*bound.fragments);
    if(admission.status!=m07_frozen::NativeFragmentAdmission::accepted_complete)
        return {admission.inventory,std::nullopt};
    return {admission.inventory,assembly.complete_canonical_population()};
}
// The accepted population owns configuration and P/G, not the fresh
// candidate numbers. Do not silently use a different support/slot set.
[[nodiscard]] inline bool m13_same_accepted_native_population(
    const std::vector<m08_frozen::AuthenticatedPairSlot>& accepted,
    const M13FreshNativePopulation& fresh)
{
    if(!fresh.native_population||accepted.size()!=m07_frozen::kBindings.size()||
       fresh.native_population->size()!=accepted.size())return false;
    for(std::size_t i=0;i<accepted.size();++i) {
        const auto& a=accepted[i];
        const auto& b=(*fresh.native_population)[i];
        if(a.binding_id!=m07_frozen::kBindings[i].binding_id||
           a.binding_id!=b.binding_id||
           a.source_feature_id!=b.source_feature_id||
           a.exact_feature_dependency.empty()||
           a.exact_feature_dependency!=b.exact_feature_dependency||
           !a.feature_authority_attested||!b.feature_authority_attested||
           a.configured.empty()||a.configured.size()!=b.configured.size())
            return false;
        for(std::size_t j=0;j<a.configured.size();++j) {
            const auto& x=a.configured[j];
            const auto& y=b.configured[j];
            if(x.current_valid!=y.current_valid||
               x.paired_native_support!=y.paired_native_support||
               x.prepared.z!=y.prepared.z||
               x.prepared.endpoint!=y.prepared.endpoint||
               x.reference.z!=y.reference.z||
               x.reference.endpoint!=y.reference.endpoint||
               x.candidate_required!=y.candidate_required)
                return false;
        }
    }
    return true;
}
// Preserve the exact M11 Fixed, mask and deadband from the accepted
// run, but replace EVERY candidate coordinate by the independent M13
// observation. Missing values cannot become an artificial safety PASS.
[[nodiscard]] inline m11_frozen::NoRegressionResult
verify_m13_fresh_protected_population(
    const std::vector<m08_frozen::AuthenticatedPairSlot>& accepted,
    const M13FreshNativePopulation& fresh,
    std::vector<m11_frozen::ProtectedComponent> accepted_k,
    bool exact_accepted_fixed_and_mask_still_current,
    bool independent_endpoint_and_safety_attested)
{
    if(!exact_accepted_fixed_and_mask_still_current||
       !independent_endpoint_and_safety_attested||
       !m13_same_accepted_native_population(accepted,fresh))
        return {};
    std::size_t ordinal=0;
    std::vector<m11_frozen::ProtectedComponent> required_k;
    for(const auto& slot:*fresh.native_population) {
        for(const auto& c:slot.configured) {
            if(ordinal>=accepted_k.size())return {};
            auto& guard=accepted_k[ordinal++];
            if(!guard.required_in_frozen_k) {
                if(!guard.exact_current||guard.protected_by_m11_mask||
                   (c.current_valid&&c.paired_native_support>0.0))return {};
                continue;
            }
            if(!c.prepared.z||!c.reference.z||!c.current_candidate_z||
               !guard.exact_current||
               guard.prepared_z!=*c.prepared.z||
               guard.gold_z!=*c.reference.z||
               !std::isfinite(*c.current_candidate_z))
                return {};
            guard.candidate_z=*c.current_candidate_z;
            required_k.push_back(guard);
        }
    }
    if(ordinal!=accepted_k.size())return {};
    return m11_frozen::verify_frozen_protected_no_regression(
        required_k,true,true);
}
// Same-run prediction and verification are measured against the same
// original reference and comparison population. A stale source or
// candidate dropout cannot be repaired by a report-supplied percentage.
struct M13NativeProximityComparison final {
    m08_frozen::NativeProximityPopulationResult predicted;
    m08_frozen::NativeProximityPopulationResult verified;
    FullM08Comparison conformance;
};
[[nodiscard]] inline M13NativeProximityComparison
compare_m13_accepted_and_fresh_native_proximity(
    const std::vector<m08_frozen::AuthenticatedPairSlot>& accepted,
    const M13FreshNativePopulation& fresh,
    const std::array<bool,4>& exact_requested_domains,
    bool accepted_prediction_and_current_dependencies_attested)
{
    M13NativeProximityComparison out;
    if(!accepted_prediction_and_current_dependencies_attested||
       !m13_same_accepted_native_population(accepted,fresh))
        return out;
    out.predicted=m08_frozen::measure_native_gold_proximity(
        accepted,exact_requested_domains,true);
    out.verified=m08_frozen::measure_native_gold_proximity(
        *fresh.native_population,exact_requested_domains,true);
    if(!out.predicted.canonical_population_current||
       !out.verified.canonical_population_current)return out;
    out.conformance=compare_frozen_m08_prediction_with_fresh_verified(
        out.predicted.domains,out.verified.domains);
    return out;
}
// Verify each native value against the actual transformed solver value.
// Raw correlation and width endpoints must survive the projection;
// neither is inferred by inverting a clipped transform.
[[nodiscard]] inline bool m13_native_coordinate_consistent(
    const m07_frozen::Binding& binding,
    const m08_frozen::AuthenticatedPairComponent& component)
{
    const auto& native=component.candidate_native;
    if(!native.value||!component.current_candidate_z||
       !std::isfinite(*native.value)||
       !std::isfinite(*component.current_candidate_z))return false;
    const auto x=*native.value;
    double z{};
    using T=m07_frozen::Transform;
    switch(binding.transform) {
    case T::identity:
    case T::power_fraction_db:
        z=x;break;
    case T::log2_positive:
        if(x<=0.0)return false;
        z=std::log2(x);break;
    case T::mono_endpoint_log2:
        if(native.endpoint==m08_frozen::Endpoint::mono_side_muted) {
            if(x!=0.0)return false;
            z=-24.0;
        } else {
            if(x<std::exp2(-24.0))return false;
            z=std::log2(x);
        }
        break;
    case T::fisher_z:
        if(x< -1.0||x>1.0)return false;
        // S02 uses the canonical M20 broadband conversion; D12 and
        // coherent stereo originate in their own existing producers.
        if(binding.binding_id=="rgsml.reference-match.binding.stereo.s02") {
            const auto mapped=m05m06_real::correlation_fisher_z(
                {x,m05m06_real::Readiness::measured});
            if(!mapped.ready())return false;
            z=*mapped.unrounded;
        } else {
            z=std::atanh(std::clamp(x,-1.0+1e-6,1.0-1e-6));
        }
        break;
    }
    return std::isfinite(z)&&z==*component.current_candidate_z;
}
// candidate_required is a conditional dropout policy for a supported
// comparison component. Native producers retain it even on a nominal
// configured slot with no reference-grade P/G evidence (e.g. mono S01).
// Preserve every slot and its configured count. A supported component,
// any actual candidate observation, or a changed unavailable observation
// remains mandatory. Only identical genuinely absent observations outside
// the original paired K are N/A; never relabel a measured dropout N/A.
[[nodiscard]] inline bool m13_requires_native_numeric_comparison(
    const m08_frozen::AuthenticatedPairComponent& predicted,
    const m08_frozen::AuthenticatedPairComponent& fresh) noexcept
{
    return (predicted.current_valid&&predicted.paired_native_support>0.0)||
        (fresh.current_valid&&fresh.paired_native_support>0.0)||
        predicted.current_candidate_z.has_value()||fresh.current_candidate_z.has_value()||
        predicted.candidate_native.value.has_value()||fresh.candidate_native.value.has_value()||
        predicted.candidate_native!=fresh.candidate_native;
}
[[nodiscard]] inline M13TwentySevenConformance
compare_m13_accepted_and_fresh_native_bindings(
    const std::vector<m08_frozen::AuthenticatedPairSlot>& accepted,
    const M13FreshNativePopulation& fresh,
    bool accepted_prediction_and_current_dependencies_attested)
{
    if(!accepted_prediction_and_current_dependencies_attested||
       !m13_same_accepted_native_population(accepted,fresh))
        return compare_m13_frozen_twenty_seven_native_bindings({});
    std::vector<M13BindingNativeEvidence> evidence;
    evidence.reserve(accepted.size());
    for(std::size_t i=0;i<accepted.size();++i) {
        const auto& a=accepted[i];
        const auto& b=(*fresh.native_population)[i];
        M13BindingNativeEvidence slot{
            a.binding_id,a.source_feature_id,a.exact_feature_dependency,
            true,{}};
        slot.configured.reserve(a.configured.size());
        for(std::size_t j=0;j<a.configured.size();++j) {
            const auto& x=a.configured[j];
            const auto& y=b.configured[j];
            const auto& p=x.candidate_native;
            const auto& v=y.candidate_native;
            ExpectedFreshNativeComponent c;
            c.expected_selector=p.selector;
            c.fresh_selector=v.selector;
            c.expected_population_id=p.population_identity;
            c.fresh_population_id=v.population_identity;
            c.predicted_native=p.value;
            c.actual_fresh_native=v.value;
            c.predicted_endpoint=p.endpoint;
            c.actual_endpoint=v.endpoint;
            if(p.valid_count&&p.configured_count) {
                c.expected_support_numerator=*p.valid_count;
                c.expected_support_denominator=*p.configured_count;
            }
            if(v.valid_count&&v.configured_count) {
                c.actual_support_numerator=*v.valid_count;
                c.actual_support_denominator=*v.configured_count;
            }
            c.required=m13_requires_native_numeric_comparison(x,y);
            c.exact_fresh_native_transform_attested=
                m13_native_coordinate_consistent(m07_frozen::kBindings[i],x)&&
                m13_native_coordinate_consistent(m07_frozen::kBindings[i],y);
            slot.configured.push_back(std::move(c));
        }
        evidence.push_back(std::move(slot));
    }
    return compare_m13_frozen_twenty_seven_native_bindings(evidence);
}
} // namespace rgsml::app::l3_automatch::m13_frozen
