#pragma once
// L3-M13 P02 ACCEPTED/FROZEN deterministic currentness projection.
// Historical immutable verifier run outcome is separate from this
// CURRENT Product projection. A provisional QTP never qualifies VERIFIED.
#include "l3_m13_qtp_27_projection.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m13_frozen {
enum class VerificationState:std::uint8_t {
    not_verified,verified,verification_failed,
    verification_unavailable,stale
};
enum class VerificationStage:std::uint8_t {
    dependency_currentness, input_integrity,
    materialization_structure, execution_prerequisites,
    render_timeline, analysis, categorical_endpoint,
    safety, protected_component, feature_numeric, proximity
};
enum class PredicateState:std::uint8_t {
    pass,failed,unavailable,not_evaluated,stale
};
struct VerificationReason final {
    VerificationStage stage{VerificationStage::input_integrity};
    std::string code;
    std::string field_path;
    std::string binding_key;
    std::string component_key;
    std::string dependency_ref;
    std::string expected_ref_or_value;
    std::string actual_ref_or_value;
    std::string numeric_category;
    std::optional<double> measured_delta;
    std::optional<double> allowed_bound;
    PredicateState predicate{PredicateState::not_evaluated};
    friend bool operator==(const VerificationReason&,
                           const VerificationReason&)=default;
};
// Preserve historical Phase-7 reasons and frozen P02 additions.
// The code stage is a *canonical* descriptor, not a display priority
// inferred from which error happens to arrive first.
struct ReasonDescriptor final {
    std::string_view code;
    VerificationStage stage;
};
inline constexpr std::array<ReasonDescriptor,20> kReasonDescriptors{{
    {"STALE_DEPENDENCY",VerificationStage::dependency_currentness},
    {"SIGNATURE_MISMATCH",VerificationStage::input_integrity},
    {"VERIFICATION_INPUT_UNAVAILABLE",VerificationStage::input_integrity},
    {"VERIFICATION_ARTIFACT_INTEGRITY_FAILURE",
        VerificationStage::input_integrity},
    {"MATERIALIZED_TOPOLOGY_MISMATCH",
        VerificationStage::materialization_structure},
    {"MATERIALIZATION_UNAVAILABLE",
        VerificationStage::materialization_structure},
    {"VERIFICATION_PROFILE_UNAVAILABLE",
        VerificationStage::execution_prerequisites},
    {"VERIFICATION_RUNTIME_UNAVAILABLE",
        VerificationStage::execution_prerequisites},
    {"FRESH_VERIFICATION_RENDER_REQUIRED",
        VerificationStage::execution_prerequisites},
    {"VERIFICATION_RENDER_UNAVAILABLE",
        VerificationStage::render_timeline},
    {"OUTPUT_SAMPLE_COUNT_MISMATCH",
        VerificationStage::render_timeline},
    {"OUTPUT_TIMELINE_MISMATCH",
        VerificationStage::render_timeline},
    {"ANALYSIS_CONFIGURATION_MISMATCH",
        VerificationStage::analysis},
    {"VERIFICATION_ANALYSIS_UNAVAILABLE",
        VerificationStage::analysis},
    {"CANDIDATE_REQUIRED_COMPONENT_MISSING",
        VerificationStage::analysis},
    {"VERIFICATION_CATEGORICAL_MISMATCH",
        VerificationStage::categorical_endpoint},
    {"HARD_SAFETY_VIOLATION",VerificationStage::safety},
    {"PROTECTED_COMPONENT_VERIFICATION_REGRESSION",
        VerificationStage::protected_component},
    {"FEATURE_VERIFICATION_MISMATCH",
        VerificationStage::feature_numeric},
    {"PROXIMITY_VERIFICATION_MISMATCH",
        VerificationStage::proximity}
}};
[[nodiscard]] inline bool accepted_reason_stage(
    const VerificationReason& reason) noexcept
{
    for(const auto& d:kReasonDescriptors)
        if(d.code==reason.code)return d.stage==reason.stage;
    return false;
}
[[nodiscard]] inline bool unsigned_utf8_less(
    std::string_view a,std::string_view b) noexcept
{
    return std::lexicographical_compare(a.begin(),a.end(),b.begin(),b.end(),
        [](char x,char y){return
            static_cast<unsigned char>(x)<
            static_cast<unsigned char>(y);});
}
[[nodiscard]] inline bool reason_canonical_less(
    const VerificationReason& a,const VerificationReason& b) noexcept
{
    if(a.stage!=b.stage)return
        static_cast<std::uint8_t>(a.stage)<
        static_cast<std::uint8_t>(b.stage);
    for(const auto [left,right]:std::array<std::pair<std::string_view,
             std::string_view>,5>{{
             {a.code,b.code},{a.field_path,b.field_path},
             {a.binding_key,b.binding_key},
             {a.component_key,b.component_key},
             {a.dependency_ref,b.dependency_ref}}}) {
        if(left!=right)return unsigned_utf8_less(left,right);
    }
    // Expected/actual and predicate are retained in the full record;
    // tie-break all fields so ordered output is independent of input order.
    const auto less=[](std::string_view l,std::string_view r){
        return unsigned_utf8_less(l,r);
    };
    if(a.expected_ref_or_value!=b.expected_ref_or_value)
        return less(a.expected_ref_or_value,b.expected_ref_or_value);
    if(a.actual_ref_or_value!=b.actual_ref_or_value)
        return less(a.actual_ref_or_value,b.actual_ref_or_value);
    if(a.numeric_category!=b.numeric_category)
        return less(a.numeric_category,b.numeric_category);
    if(a.predicate!=b.predicate)return
        static_cast<std::uint8_t>(a.predicate)<
        static_cast<std::uint8_t>(b.predicate);
    if(a.measured_delta!=b.measured_delta)
        return a.measured_delta<b.measured_delta;
    return a.allowed_bound<b.allowed_bound;
}
[[nodiscard]] inline std::optional<std::vector<VerificationReason>>
normalize_verification_reasons(std::vector<VerificationReason> reasons)
{
    for(const auto& r:reasons)
        if(!accepted_reason_stage(r)||r.code.empty()||
           (r.measured_delta&&(!std::isfinite(*r.measured_delta)||
             *r.measured_delta<0.0))||
           (r.allowed_bound&&(!std::isfinite(*r.allowed_bound)||
             *r.allowed_bound<0.0)))
            return std::nullopt;
    std::sort(reasons.begin(),reasons.end(),reason_canonical_less);
    reasons.erase(std::unique(reasons.begin(),reasons.end()),reasons.end());
    return reasons;
}
struct VerificationStateEvidence final {
    bool materialization_exists{};
    bool any_attempt{};
    bool exact_dependency_known_changed{};
    bool evaluated_predicate_failed{};
    bool mandatory_evidence_missing{};
    bool all_complete_conjunction_passed{};
    bool qtp_qualified_for_claimed_scope{};
    bool report_integrity_passed{};
};
[[nodiscard]] inline VerificationState project_verification_state(
    const VerificationStateEvidence& e) noexcept
{
    if(e.exact_dependency_known_changed)return VerificationState::stale;
    if(e.evaluated_predicate_failed)
        return VerificationState::verification_failed;
    if(!e.materialization_exists||e.mandatory_evidence_missing)
        return VerificationState::verification_unavailable;
    if(!e.any_attempt)return VerificationState::not_verified;
    if(e.all_complete_conjunction_passed&&
       e.qtp_qualified_for_claimed_scope&&e.report_integrity_passed)
        return VerificationState::verified;
    return VerificationState::verification_unavailable;
}
} // namespace rgsml::app::l3_automatch::m13_frozen
