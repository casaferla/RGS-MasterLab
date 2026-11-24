#pragma once
// M13 independently re-rendered Master -> ORIGINAL M05/M06/M07.
// No numerical candidate fields from the accepted M10 plan/resolver
// survive this binding. This does not grant final QTP/VERIFIED.
#include "l3_m13_fresh_original_native_import.hpp"
#include "l3_m10_p2_verified_candidate_triple.hpp"
#include "l3_m11_p03_protected_no_regression.hpp"
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m13_frozen {
struct M13FreshNativePopulation final {
    m07_frozen::PartialFeatureInventory inventory;
    std::optional<std::vector<m08_frozen::AuthenticatedPairSlot>> slots;
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
    bool current_product_dependencies_attested,
    bool exact_file_based_lra_policy_attested)
{
    if(!current_product_dependencies_attested||
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
    if(!fresh.slots||accepted.size()!=m07_frozen::kBindings.size()||
       fresh.slots->size()!=accepted.size())return false;
    for(std::size_t i=0;i<accepted.size();++i) {
        const auto& a=accepted[i];
        const auto& b=(*fresh.slots)[i];
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
    for(const auto& slot:*fresh.slots) {
        for(const auto& c:slot.configured) {
            if(ordinal>=accepted_k.size()||
               !c.prepared.z||!c.reference.z||!c.current_candidate_z)
                return {};
            auto& guard=accepted_k[ordinal++];
            if(!guard.exact_current||!guard.required_in_frozen_k||
               guard.prepared_z!=*c.prepared.z||
               guard.gold_z!=*c.reference.z||
               !std::isfinite(*c.current_candidate_z))
                return {};
            guard.candidate_z=*c.current_candidate_z;
        }
    }
    if(ordinal!=accepted_k.size())return {};
    return m11_frozen::verify_frozen_protected_no_regression(
        accepted_k,true,true);
}
} // namespace rgsml::app::l3_automatch::m13_frozen
