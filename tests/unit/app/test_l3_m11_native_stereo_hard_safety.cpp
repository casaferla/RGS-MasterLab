#include "internal/l3_m11_native_stereo_hard_safety.hpp"
#include <QtTest/QTest>
#include <array>
#include <limits>
namespace rgsml::tests {
namespace {
namespace m11=rgsml::app::l3_automatch::m11_frozen;
[[nodiscard]] m11::NativeStereoRawSafety input() {
    m11::NativeStereoRawSafety e;
    e.exact_current_source_candidate_and_canonical_safety_revision=true;
    e.source_and_candidate_stereo=true;
    e.no_forbidden_decorrelation_or_haas=true;
    e.prepared_correlation_p10_raw=-0.10;
    e.candidate_correlation_p10_raw=-0.11;
    e.prepared_negative_correlation_occupancy=0.20;
    e.candidate_negative_correlation_occupancy=0.21;
    e.prepared_low_end_width={0.2,0.3,0.4};
    e.candidate_low_end_width={0.1,0.2,0.3};
    e.prepared_low_end_rho={0.5,0.6,0.7};
    e.candidate_low_end_rho={0.49,0.59,0.69};
    return e;
}
class NativeStereoSafetyTest final:public QObject {
    Q_OBJECT
private slots:
    void rawSafetyAcceptsInclusiveFrozenSlack();
    void zeroSideAndAnyLowEndWideningAlwaysVeto();
    void correlationAndOccupancyHaveSeparateRawHardVeto();
    void missingUnsafeEvidenceCannotBeReplacedWithScore();
};
void NativeStereoSafetyTest::rawSafetyAcceptsInclusiveFrozenSlack() {
    auto e=input();
    e.candidate_low_end_width={0.2,0.3,0.4};
    e.candidate_low_end_rho={0.49,0.59,0.69};
    e.candidate_negative_correlation_occupancy=0.21;
    const auto r=m11::verify_m11_native_stereo_hard_safety(e);
    QVERIFY(r.passes_hard_gate());
    QVERIFY(!r.authorizes_export());
}
void NativeStereoSafetyTest::
zeroSideAndAnyLowEndWideningAlwaysVeto(){
    auto e=input();
    e.candidate_low_end_width[1]=0.31;
    const auto r=m11::verify_m11_native_stereo_hard_safety(e);
    QCOMPARE(r.reason,m11::NativeStereoSafetyReason::
        forbidden_low_end_widening);
    QCOMPARE(*r.low_end_cutoff_ordinal,std::size_t{1});
    e=input();
    e.prepared_low_end_width[0]=0.0;
    e.prepared_low_end_exact_zero_side[0]=true;
    e.candidate_low_end_width[0]=0.0001;
    QCOMPARE(m11::verify_m11_native_stereo_hard_safety(e).reason,
             m11::NativeStereoSafetyReason::
                 forbidden_low_end_widening);
}
void NativeStereoSafetyTest::
correlationAndOccupancyHaveSeparateRawHardVeto(){
    auto e=input();
    e.candidate_low_end_rho[0]=0.47;
    QCOMPARE(m11::verify_m11_native_stereo_hard_safety(e).reason,
             m11::NativeStereoSafetyReason::low_end_rho_regressed);
    e=input();e.prepared_correlation_p10_raw=0.0;
    e.candidate_correlation_p10_raw=-0.00001;
    QCOMPARE(m11::verify_m11_native_stereo_hard_safety(e).reason,
             m11::NativeStereoSafetyReason::global_p10_regressed);
    e=input();e.candidate_negative_correlation_occupancy=0.23;
    QCOMPARE(m11::verify_m11_native_stereo_hard_safety(e).reason,
             m11::NativeStereoSafetyReason::
                 negative_occupancy_regressed);
}
void NativeStereoSafetyTest::
missingUnsafeEvidenceCannotBeReplacedWithScore(){
    auto e=input();
    e.candidate_low_end_rho[2].reset();
    QVERIFY(!m11::verify_m11_native_stereo_hard_safety(e)
         .passes_hard_gate());
    e=input();e.exact_current_source_candidate_and_canonical_safety_revision=
        false;
    QCOMPARE(m11::verify_m11_native_stereo_hard_safety(e).reason,
             m11::NativeStereoSafetyReason::missing_or_stale_safety_evidence);
    e=input();e.no_forbidden_decorrelation_or_haas=false;
    QCOMPARE(m11::verify_m11_native_stereo_hard_safety(e).reason,
             m11::NativeStereoSafetyReason::
                 artificial_decorrelation_disallowed);
    e=input();
    e.candidate_correlation_p10_raw=
        std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(m11::verify_m11_native_stereo_hard_safety(e).reason,
             m11::NativeStereoSafetyReason::nonfinite_or_invalid);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::NativeStereoSafetyTest)
#include "test_l3_m11_native_stereo_hard_safety.moc"
