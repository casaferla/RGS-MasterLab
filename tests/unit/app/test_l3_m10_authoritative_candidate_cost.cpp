#include "internal/l3_m10_authoritative_candidate_cost.hpp"
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m07=rgsml::app::l3_automatch::m07_frozen;
namespace m08=rgsml::app::l3_automatch::m08_frozen;
namespace m10=rgsml::app::l3_automatch::m10_frozen;
[[nodiscard]] m10::AuthoritativeCostInput source() {
    const std::array<m08::DomainAmount,4> ordered{{
        {m07::kDomainIds[0],1.0},{m07::kDomainIds[1],1.0},
        {m07::kDomainIds[2],1.0},{m07::kDomainIds[3],1.0}}};
    const auto a=m08::create_amount_config(0.5,ordered,11);
    m10::AuthoritativeCostInput input;
    input.amount=*a.config;
    input.m08_corrective_mask={true,true,true,true};
    input.optimization_target_equals_desired_attested=true;
    input.exact_m05_m06_source_and_m07_dependency_current=true;
    input.m11_fixed_and_protected_snapshot_current=true;
    input.fresh_candidate_render_and_analysis_current=true;
    input.hard_m11_and_independent_safety_preflight_passed=true;
    input.m11_full_k_dependency_and_order_attested=true;
    input.active_owned_modules=2;
    input.enabled_artistic_bands=3;
    input.canonical_current_tuple={0x01};
    input.exact_topology_evidence_identity={0x02};
    for(const auto& binding:m07::kBindings) {
        m08::AuthenticatedPairSlot slot;
        slot.binding_id=std::string{binding.binding_id};
        slot.source_feature_id=std::string{binding.source_feature_id};
        slot.exact_feature_dependency="real-phase6-artifact-checksum";
        slot.feature_authority_attested=true;
        const auto n=binding.component_share==m07::ComponentShare::thirds
            ?std::size_t{3}:
            binding.component_share==m07::ComponentShare::configured_n
            ?std::size_t{2}:std::size_t{1};
        for(std::size_t i=0;i<n;++i) {
            slot.configured.push_back({
                true,1.0,{12.0,m08::Endpoint::ordinary},
                {9.0,m08::Endpoint::ordinary},10.5,true});
            input.protected_full_k.push_back({
                true,true,true,12.0,9.0,10.5,10.5,0.5});
        }
        input.exact_pair_slots.push_back(std::move(slot));
    }
    return input;
}
class M10AuthoritativeCostTest final:public QObject {
    Q_OBJECT
private slots:
    void full27SlotExactTargetAndNonzeroComplexity();
    void missingSourceOrFixedCannotBeReplacedWithM21();
    void candidateRequiredDropoutRejectsInsteadOfRenormalizing();
    void noCorrectionNeedsIndependentFixedProofNotNoneTop8();
    void m11HardGuardAppliesBeforeAnyNumericScore();
    void independentGuardAndCandidateCannotDisagree();
    void currentNativeStereoSafetyVetoesBeforeMeasuredM10Cost();
};
void M10AuthoritativeCostTest::
currentNativeStereoSafetyVetoesBeforeMeasuredM10Cost() {
    auto input=source();
    input.native_stereo_raw_safety_required_by_current_owner=true;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::
                 missing_actual_candidate_or_protection);
    rgsml::app::l3_automatch::m11_frozen::NativeStereoRawSafety native;
    native.exact_current_source_candidate_and_canonical_safety_revision=true;
    native.source_and_candidate_stereo=true;
    native.no_forbidden_decorrelation_or_haas=true;
    native.prepared_correlation_p10_raw=0.1;
    native.candidate_correlation_p10_raw=0.2;
    native.prepared_negative_correlation_occupancy=0.05;
    native.candidate_negative_correlation_occupancy=0.04;
    native.prepared_low_end_width={0.5,0.5,0.5};
    native.candidate_low_end_width={0.4,0.4,0.4};
    native.prepared_low_end_rho={0.6,0.6,0.6};
    native.candidate_low_end_rho={0.6,0.6,0.6};
    input.native_stereo_raw_safety=native;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::measured);
    input.native_stereo_raw_safety->candidate_low_end_width[1]=0.6;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::
                 missing_actual_candidate_or_protection);
}
void M10AuthoritativeCostTest::full27SlotExactTargetAndNonzeroComplexity() {
    const auto r=m10::score_exact_candidate_population(source());
    QCOMPARE(r.reason,m10::AuthoritativeCostReason::measured);
    QVERIFY(r.measured&&r.target_evidence);
    QVERIFY(r.target_evidence->valid_measured());
    QVERIFY(std::abs(*r.target_evidence->j_target)<1e-12);
    QVERIFY(std::abs(r.measured->total_j-0.0025)<1e-12);
    QVERIFY(!r.authorizes_product_publication());
}
void M10AuthoritativeCostTest::
missingSourceOrFixedCannotBeReplacedWithM21() {
    auto input=source();
    input.exact_m05_m06_source_and_m07_dependency_current=false;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::stale_or_missing_owner);
    input=source();
    input.optimization_target_equals_desired_attested=false;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::stale_or_missing_owner);
    input=source();
    input.m11_fixed_and_protected_snapshot_current=false;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::stale_or_missing_owner);
    input=source();
    input.fresh_candidate_render_and_analysis_current=false;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::
                missing_actual_candidate_or_protection);
}
void M10AuthoritativeCostTest::
candidateRequiredDropoutRejectsInsteadOfRenormalizing() {
    auto input=source();
    input.exact_pair_slots[0].configured[0].current_candidate_z.reset();
    const auto missing=m10::score_exact_candidate_population(input);
    QVERIFY(!missing.measured);
    QVERIFY(missing.reason!=m10::AuthoritativeCostReason::measured);
    input=source();
    input.exact_pair_slots.pop_back(); // can't use 26 instead of 27
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::invalid_27_slot_population);
}
void M10AuthoritativeCostTest::
m11HardGuardAppliesBeforeAnyNumericScore() {
    auto input=source();
    input.protected_full_k[0].candidate_z=20.0;
    const auto rejected=m10::score_exact_candidate_population(input);
    QCOMPARE(rejected.reason,
             m10::AuthoritativeCostReason::
                missing_actual_candidate_or_protection);
    QVERIFY(!rejected.measured);
    input=source();
    input.protected_full_k.pop_back();
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::
                missing_actual_candidate_or_protection);
}
void M10AuthoritativeCostTest::
independentGuardAndCandidateCannotDisagree() {
    auto input=source();
    // Both individually look safe against Fixed, but do not describe
    // the same candidate measurement: rejection must be unconditional.
    input.exact_pair_slots[0].configured[0].current_candidate_z=10.0;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::
                 missing_actual_candidate_or_protection);
    input=source();
    input.protected_full_k[0].prepared_z=11.9;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::
                 missing_actual_candidate_or_protection);
    input=source();
    input.protected_full_k[0].gold_z=9.1;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::
                 missing_actual_candidate_or_protection);
    input=source();
    input.protected_full_k[0].exact_current=false;
    QCOMPARE(m10::score_exact_candidate_population(input).reason,
             m10::AuthoritativeCostReason::
                 missing_actual_candidate_or_protection);
}
void M10AuthoritativeCostTest::
noCorrectionNeedsIndependentFixedProofNotNoneTop8() {
    auto input=source();
    input.m08_corrective_mask={false,false,false,false};
    input.fresh_candidate_render_and_analysis_current=false;
    auto no_proof=m10::score_exact_candidate_population(input);
    QVERIFY(!no_proof.measured);
    QCOMPARE(no_proof.reason,
             m10::AuthoritativeCostReason::stale_or_missing_owner);
    input.m11_no_correction_full_evidence_attested=true;
    const auto result=m10::score_exact_candidate_population(input);
    QCOMPARE(result.reason,
             m10::AuthoritativeCostReason::proven_no_correction_required);
    QVERIFY(!result.measured);
    QVERIFY(!result.target_evidence); // never fabricate zero objective
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M10AuthoritativeCostTest)
#include "test_l3_m10_authoritative_candidate_cost.moc"
