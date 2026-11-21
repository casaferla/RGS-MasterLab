#include "internal/l3_m06_d02_lra_binding.hpp"
#include <QtTest/QTest>
#include <string>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] real::LraTripleEvidence source() {
    real::LraNativeObservation a,b,c;
    a.lra_lu=6.0;a.reason=real::LraNativeReason::ready;
    b.lra_lu=8.0;b.reason=real::LraNativeReason::ready;
    c.lra_lu=7.0;c.reason=real::LraNativeReason::ready;
    return {a,b,c,{"PREPARED",1,true},{"REFERENCE",2,true},
            {"CANDIDATE",3,true},true,
            "exact-Phase6-D02-EBU-definition-sha"};
}
class LraBindingTest final:public QObject {
    Q_OBJECT
private slots:
    void exactlyOneLraSlotWithScalarSupportOne();
    void unqualifiedReferenceRetainsConfiguredMassButNoTarget();
    void unqualifiedCandidateNeverPretendsAchieved();
    void staleOwnerAndChecksumRejectAll();
};
void LraBindingTest::exactlyOneLraSlotWithScalarSupportOne() {
    const auto out=real::bind_real_d02_lra(source());
    QCOMPARE(out.reason,real::LraBindingReason::ready);
    QVERIFY(out.slot);
    QCOMPARE(out.slot->binding_id,
             std::string{"rgsml.reference-match.binding.dynamics.d02"});
    QCOMPARE(out.slot->configured.size(),std::size_t{1});
    const auto& c=out.slot->configured[0];
    QVERIFY(c.current_valid);
    QCOMPARE(c.paired_native_support,1.0);
    QCOMPARE(*c.prepared.z,6.0);
    QCOMPARE(*c.reference.z,8.0);
    QCOMPARE(*c.current_candidate_z,7.0);
    QVERIFY(!out.authorizes_plan());
}
void LraBindingTest::
unqualifiedReferenceRetainsConfiguredMassButNoTarget() {
    auto input=source();
    input.reference.reason=real::LraNativeReason::
        computed_but_reference_grade_unattested;
    const auto out=real::bind_real_d02_lra(input);
    QVERIFY(out.slot);
    QCOMPARE(out.slot->configured.size(),std::size_t{1});
    QVERIFY(!out.slot->configured[0].current_valid);
    QVERIFY(!out.slot->configured[0].reference.z);
    QCOMPARE(out.slot->configured[0].paired_native_support,0.0);
}
void LraBindingTest::unqualifiedCandidateNeverPretendsAchieved() {
    auto input=source();
    input.actual_candidate.reason=real::LraNativeReason::
        insufficient_active_duration_or_retained;
    const auto out=real::bind_real_d02_lra(input);
    QVERIFY(out.slot);
    QVERIFY(out.slot->configured[0].current_valid);
    QVERIFY(!out.slot->configured[0].current_candidate_z);
}
void LraBindingTest::staleOwnerAndChecksumRejectAll() {
    auto input=source();
    input.candidate_owner.resource_identity="REFERENCE";
    QCOMPARE(real::bind_real_d02_lra(input).reason,
             real::LraBindingReason::unqualified_source_or_revision);
    input=source();
    input.exact_feature_dependency.clear();
    QCOMPARE(real::bind_real_d02_lra(input).reason,
             real::LraBindingReason::invalid_feature_identity);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::LraBindingTest)
#include "test_l3_m06_d02_lra_binding.moc"
