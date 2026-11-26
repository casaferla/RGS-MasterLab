#pragma once
// L3-M13: one immutable PRODUCT FRESH measurement, accepted M07/M08
// authoritative comparison inputs, and deterministic P02 diagnosis.
// An assessment is NOT a ReferenceMatchVerificationReport checksum,
// qualification approval, persistent run or a VERIFIED release state.
// An unavailable mandatory QTP authority cannot be filled by PASS flags.
#include "l3_m13_product_fresh_postmaterialization_probe.hpp"
#include "l3_m13_27_binding_conformance.hpp"
#include "l3_m13_predicted_vs_verified_proximity.hpp"
#include "l3_m13_verification_state_reasons.hpp"
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m13_frozen {
enum class MandatoryHardPredicate:std::uint8_t {
    passed,failed,unavailable
};
struct M13ProvisionalAssessment final {
    VerificationState state{VerificationState::verification_unavailable};
    std::vector<VerificationReason> canonical_reasons;
    std::size_t configured_native_components{};
    std::size_t required_native_components{};
    bool fresh_master_execution_attested{};
    bool actual_27_binding_population_attested{};
    bool same_run_proximity_attested{};
    bool verified_profile_approval_exists{}; // always false in this lane
    // Development checkpoint only: distinguish successful physical
    // native verification from missing formal QTP approval. No release
    // permission is created, and any other missing/failed/stale predicate
    // continues to block this numerical checkpoint.
    [[nodiscard]] bool native_numerical_evidence_passed() const noexcept {
        return state==VerificationState::verification_unavailable&&
            fresh_master_execution_attested&&actual_27_binding_population_attested&&
            same_run_proximity_attested&&required_native_components>0U&&
            configured_native_components>=required_native_components&&
            !verified_profile_approval_exists&&canonical_reasons.size()==1U&&
            canonical_reasons.front().stage==VerificationStage::execution_prerequisites&&
            canonical_reasons.front().code=="VERIFICATION_PROFILE_UNAVAILABLE"&&
            canonical_reasons.front().predicate==PredicateState::unavailable&&
            canonical_reasons.front().field_path=="/qualificationToleranceProfile/approvalRef";
    }
    [[nodiscard]] constexpr bool authorizes_product_verified()
        const noexcept{return false;}
    [[nodiscard]] constexpr bool authorizes_master_wav_export()
        const noexcept{return false;}
};
[[nodiscard]] inline M13ProvisionalAssessment
assess_provisional_m13_product_fresh_evidence(
    const M13ImmutableProductFreshProbeRequest& request,
    const M13FreshProductMasterProbe& fresh,
    const M13TwentySevenConformance& features,
    const FullM08Comparison& proximity,
    bool exact_product_m12_owner_still_current_after_worker,
    bool independent_source_reference_and_scopes_current,
    MandatoryHardPredicate actual_protected_m11_no_regression,
    MandatoryHardPredicate all_other_applicable_hard_safety)
{
    M13ProvisionalAssessment output;
    output.configured_native_components=features.configured_components;
    output.required_native_components=features.required_components;
    VerificationStateEvidence state;
    state.materialization_exists=true;
    state.any_attempt=true;
    std::vector<VerificationReason> reasons;
    const auto record=[&](VerificationStage stage,const char* code,
                          std::string field,PredicateState predicate,
                          std::string binding="") {
        VerificationReason r;
        r.stage=stage;r.code=code;r.field_path=std::move(field);
        r.binding_key=std::move(binding);r.predicate=predicate;
        reasons.push_back(std::move(r));
    };
    if(!exact_product_m12_owner_still_current_after_worker||
       !fresh.matches_executed_request(request)) {
        state.exact_dependency_known_changed=true;
        record(VerificationStage::dependency_currentness,
            "STALE_DEPENDENCY","/currentM12ProductOwner",
            PredicateState::stale);
    }
    if(!independent_source_reference_and_scopes_current) {
        state.mandatory_evidence_missing=true;
        record(VerificationStage::input_integrity,
            "VERIFICATION_INPUT_UNAVAILABLE",
            "/Source/Prepared/Reference/AnalysisScope",
            PredicateState::unavailable);
    }
    const auto& owner=request.exact_materialized_preview_owner();
    const auto& source=owner.exact_prepared_owner;
    const bool correctFresh=source&&owner.replay&&
        fresh.matches_executed_request(request)&&
        fresh.independent_m16_ceiling_db_tp==request.frozen_independent_m16_ceiling_db_tp()&&
        fresh.logical_verification_evaluations==1U&&
        &fresh.fresh_full_master_pcm.buffer()!=&source->buffer()&&
        fresh.fresh_full_master_pcm.view().absolute_range()==
            source->view().absolute_range()&&
        fresh.fresh_full_master_pcm.view().format()==
            source->view().format()&&
        m12_frozen::m12_native_stage_witness_matches_replay_plan(
            *owner.replay,fresh.stage_witness);
    output.fresh_master_execution_attested=correctFresh;
    if(!correctFresh) {
        state.evaluated_predicate_failed=true;
        record(VerificationStage::materialization_structure,
            "MATERIALIZED_TOPOLOGY_MISMATCH",
            "/freshRender/actualDSPExecution",
            PredicateState::failed);
    }
    const auto rate=fresh.fresh_full_master_pcm.view().format().
        sample_rate().value();
    if(fresh.fresh_m16.sample_rate_hz()!=rate||
       fresh.fresh_m19.sample_rate_hz!=rate||
       fresh.fresh_m20.sample_rate_hz!=rate||
       fresh.fresh_m19.scope_begin_frame!=0||
       fresh.fresh_m20.scope_begin_frame!=0||
       fresh.fresh_m19.scope_frame_count!=
         fresh.fresh_full_master_pcm.view().frame_count().value()||
       fresh.fresh_m20.scope_frame_count!=
         fresh.fresh_full_master_pcm.view().frame_count().value()) {
        state.evaluated_predicate_failed=true;
        record(VerificationStage::analysis,
            "ANALYSIS_CONFIGURATION_MISMATCH",
            "/freshM16M19M20/SourceProcessingRate",
            PredicateState::failed);
    }
    const double maxAllowed=std::pow(
        10.0,request.frozen_independent_m16_ceiling_db_tp()/20.0);
    const double measured=fresh.fresh_m16.global_linear_peak();
    if(!std::isfinite(maxAllowed)||maxAllowed<=0.0||
       !std::isfinite(measured)||measured<0.0||measured>maxAllowed) {
        state.evaluated_predicate_failed=true;
        record(VerificationStage::safety,"HARD_SAFETY_VIOLATION",
            "/safetyResults/freshM16",PredicateState::failed);
    }
    for(const auto& row:features.rows) {
        if(row.outcome==BindingConformance::failed) {
            state.evaluated_predicate_failed=true;
            record(VerificationStage::feature_numeric,
                "FEATURE_VERIFICATION_MISMATCH",
                "/featureResults/"+row.binding_id,PredicateState::failed,
                row.binding_id);
        } else if(row.outcome==BindingConformance::unavailable) {
            state.mandatory_evidence_missing=true;
            record(VerificationStage::analysis,
                "VERIFICATION_ANALYSIS_UNAVAILABLE",
                "/featureResults/"+row.binding_id,
                PredicateState::unavailable,row.binding_id);
        }
    }
    if(!features.every_required_binding_complete) {
        state.mandatory_evidence_missing=true;
        record(VerificationStage::analysis,
            "CANDIDATE_REQUIRED_COMPONENT_MISSING",
            "/featureResults/configuredPopulation",PredicateState::unavailable);
    }
    output.actual_27_binding_population_attested=
        features.complete_passing_evidence();
    if(features.every_required_binding_complete&&
       features.every_required_binding_passed&&
       !output.actual_27_binding_population_attested) {
        state.evaluated_predicate_failed=true;
        record(VerificationStage::input_integrity,
            "VERIFICATION_ARTIFACT_INTEGRITY_FAILURE",
            "/featureResults/summary",PredicateState::failed);
    }
    for(std::size_t i=0;i<proximity.domains.size();++i) {
        const auto& domain=proximity.domains[i];
        if(domain.status==ProximityComparisonStatus::failed) {
            state.evaluated_predicate_failed=true;
            record(VerificationStage::proximity,
                "PROXIMITY_VERIFICATION_MISMATCH",
                "/proximityResults/domain/"+std::to_string(i),
                PredicateState::failed);
        }
        if(domain.status==ProximityComparisonStatus::unavailable) {
            state.mandatory_evidence_missing=true;
            record(VerificationStage::analysis,
                "VERIFICATION_ANALYSIS_UNAVAILABLE",
                "/proximityResults/domain/"+std::to_string(i),
                PredicateState::unavailable);
        }
    }
    if(proximity.global.status==ProximityComparisonStatus::failed) {
        state.evaluated_predicate_failed=true;
        record(VerificationStage::proximity,
            "PROXIMITY_VERIFICATION_MISMATCH",
            "/proximityResults/global",PredicateState::failed);
    } else if(!proximity.all_required_pass()) {
        state.mandatory_evidence_missing=true;
        record(VerificationStage::analysis,
            "VERIFICATION_ANALYSIS_UNAVAILABLE",
            "/proximityResults/global",PredicateState::unavailable);
    }
    output.same_run_proximity_attested=proximity.all_required_pass();
    const auto hard=[&](MandatoryHardPredicate input,
                        const char* field,const char* failedCode,
                        VerificationStage failedStage) {
        if(input==MandatoryHardPredicate::passed)return;
        const bool failed=input==MandatoryHardPredicate::failed;
        if(failed)state.evaluated_predicate_failed=true;
        else state.mandatory_evidence_missing=true;
        record(failed?failedStage:VerificationStage::analysis,
            failed?failedCode:"VERIFICATION_ANALYSIS_UNAVAILABLE",field,
            failed?PredicateState::failed:PredicateState::unavailable);
    };
    hard(actual_protected_m11_no_regression,
         "/protectedComponentResults",
         "PROTECTED_COMPONENT_VERIFICATION_REGRESSION",
         VerificationStage::protected_component);
    hard(all_other_applicable_hard_safety,
         "/safetyResults/allOtherHardPredicates",
         "HARD_SAFETY_VIOLATION",VerificationStage::safety);
    // Frozen PO documentary closure is NOT a QTP approval record.
    // The only accepted state here is PROVISIONAL; never mark verified.
    state.qtp_qualified_for_claimed_scope=false;
    state.mandatory_evidence_missing=true;
    record(VerificationStage::execution_prerequisites,
        "VERIFICATION_PROFILE_UNAVAILABLE",
        "/qualificationToleranceProfile/approvalRef",
        PredicateState::unavailable);
    const auto normalized=normalize_verification_reasons(
        std::move(reasons));
    if(normalized)output.canonical_reasons=std::move(*normalized);
    output.state=project_verification_state(state);
    return output;
}
} // namespace rgsml::app::l3_automatch::m13_frozen
