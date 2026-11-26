#pragma once
// Product M13 composition of independent measurements and immutable
// accepted winner evidence. No caller-supplied proximity or feature PASS.
#include "l3_m13_fresh_native_population.hpp"
#include "l3_m13_provisional_verification_assessment.hpp"
#include "l3_m10_native_prediction_snapshot.hpp"
#include "l3_m10_frozen_coordinate_refiner.hpp"
#include "l3_m11_actual_native_stereo_safety.hpp"
#include "l3_m13_native_graph_safety.hpp"

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
    MandatoryHardPredicate other_hard_safety,
    bool derive_product_native_safety=false)
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
    if(derive_product_native_safety&&other_hard_safety!=MandatoryHardPredicate::failed) {
        const auto graph=current?audit_m13_native_graph_safety(request,fresh):NativeGraphSafety::unavailable;
        other_hard_safety=graph==NativeGraphSafety::passed&&independent_endpoint_and_safety_attested
            ?MandatoryHardPredicate::passed:graph==NativeGraphSafety::failed
            ?MandatoryHardPredicate::failed:MandatoryHardPredicate::unavailable;
    }
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
        // The original Prepared and freshly measured Master own the
        // raw stereo safety, never the accepted solver's candidate rho.
        if(independent_pair.prepared.original_m19_m20.channels==2U) {
            const auto fresh_readings=m05m06_real::import_actual_full_source(
                fresh.fresh_m19,fresh.fresh_m20,m13_fresh_master_native_identity(request));
            const auto raw=fresh_readings.source&&fresh.additional_native
                ?m11_frozen::bind_actual_native_stereo_safety(
                    independent_pair.prepared.original_m19_m20,
                    independent_pair.prepared.full_native.s03_s06,
                    *fresh_readings.source,fresh.additional_native->s03_s06,
                    independent_endpoint_and_safety_attested,
                    independent_endpoint_and_safety_attested&&
                        (!derive_product_native_safety||
                         other_hard_safety==MandatoryHardPredicate::passed))
                :m11_frozen::NativeStereoRawSafety{};
            const auto check=m11_frozen::verify_m11_native_stereo_hard_safety(raw);
            if(other_hard_safety!=MandatoryHardPredicate::failed) {
                if(check.reason==m11_frozen::NativeStereoSafetyReason::missing_or_stale_safety_evidence||
                   check.reason==m11_frozen::NativeStereoSafetyReason::nonfinite_or_invalid)
                    other_hard_safety=MandatoryHardPredicate::unavailable;
                else if(!check.passes_hard_gate())
                    other_hard_safety=MandatoryHardPredicate::failed;
            }
        }
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
