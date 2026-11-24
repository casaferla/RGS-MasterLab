#pragma once
// Product M13 composition of independent measurements and immutable
// accepted winner evidence. No caller-supplied proximity or feature PASS.
#include "l3_m13_fresh_native_population.hpp"
#include "l3_m13_provisional_verification_assessment.hpp"
#include "l3_m10_native_prediction_snapshot.hpp"
#include "l3_m10_frozen_coordinate_refiner.hpp"

namespace rgsml::app::l3_automatch::m13_frozen {
[[nodiscard]] inline M13ProvisionalAssessment
assess_m13_actual_native_completion(
    const M13ImmutableProductFreshProbeRequest& request,
    const M13FreshProductMasterProbe& fresh,
    const OriginalPreparedReferenceNativeFacts& independent_pair,
    const OfflineProductOwnerStamp& exact_source_reference_owner,
    m10_frozen::P2OriginalRealNativeTriple original_definitions,
    std::string exact_phase6_m07_generation,
    const std::string& accepted_scope_revision,
    const m10_frozen::MeasuredOrderKey& accepted_winner,
    const std::string& accepted_winner_checksum,
    const std::string& accepted_protection_checksum,
    bool accepted_winner_evidence_attested,
    bool exact_product_m12_owner_still_current_after_worker,
    bool independent_source_reference_and_scopes_current,
    bool exact_file_based_lra_policy_attested,
    bool independent_endpoint_and_safety_attested,
    MandatoryHardPredicate other_hard_safety)
{
    const auto& replay=request.exact_materialized_preview_owner().replay;
    const bool current=replay&&accepted_winner_evidence_attested&&
        exact_product_m12_owner_still_current_after_worker&&
        independent_source_reference_and_scopes_current&&
        fresh.matches_executed_request(request)&&
        !accepted_winner_checksum.empty()&&
        !accepted_protection_checksum.empty()&&
        replay->exact_m10_winner_checksum==accepted_winner_checksum&&
        replay->exact_m11_protection_checksum==accepted_protection_checksum&&
        m10_frozen::valid_key(accepted_winner)&&
        accepted_winner.native_prediction;
    M13TwentySevenConformance bindings=
        compare_m13_frozen_twenty_seven_native_bindings({});
    FullM08Comparison proximity;
    auto protection=MandatoryHardPredicate::unavailable;
    if(current) {
        const auto& prediction=*accepted_winner.native_prediction;
        const m05m06_real::SourceIdentity accepted_candidate_owner{
            prediction.candidate_resource_identity,prediction.candidate_generation,true};
        const bool exact_accepted_native_owner=
            prediction.exact_m07_generation==exact_phase6_m07_generation&&
            authentic_product_candidate_identity(accepted_candidate_owner,
                exact_source_reference_owner,prediction.exact_candidate_ticket);
        const auto actual=bind_m13_fresh_native_population(
            request,fresh,independent_pair,exact_source_reference_owner,
            std::move(original_definitions),std::move(exact_phase6_m07_generation),
            accepted_candidate_owner,accepted_scope_revision,
            exact_accepted_native_owner,exact_file_based_lra_policy_attested);
        bindings=compare_m13_accepted_and_fresh_native_bindings(
            prediction.native_population,actual,true);
        if(prediction.requested_reporting_domains)
            proximity=compare_m13_accepted_and_fresh_native_proximity(
                prediction.native_population,actual,*prediction.requested_reporting_domains,
                true).conformance;
        const auto guard=verify_m13_fresh_protected_population(
            prediction.native_population,actual,prediction.protected_k,true,
            independent_endpoint_and_safety_attested);
        if(guard.is_admissible())protection=MandatoryHardPredicate::passed;
        else if(guard.reason==
            m11_frozen::NoRegressionReason::protected_component_regressed)
            protection=MandatoryHardPredicate::failed;
    }
    return assess_provisional_m13_product_fresh_evidence(
        request,fresh,bindings,proximity,
        exact_product_m12_owner_still_current_after_worker,
        independent_source_reference_and_scopes_current,
        protection,other_hard_safety);
}
} // namespace rgsml::app::l3_automatch::m13_frozen
