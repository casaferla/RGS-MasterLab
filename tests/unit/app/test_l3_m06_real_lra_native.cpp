#include "internal/l3_m06_real_lra_native.hpp"
#include <QtTest/QTest>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] real::SourceReadings source(
    std::size_t windows,bool with_outlier=false) {
    real::SourceReadings result;
    result.owner={"real-M19-eligible-owner",5,true};
    result.sample_rate_hz=48000;
    result.scope_frame_count=static_cast<std::int64_t>(
        windows*4800+3*48000);
    result.short_term_complete_windows=windows;
    result.short_term_valid_windows=windows;
    for(std::size_t i=0;i<windows;++i) {
        double value=-40.0+static_cast<double>(i%21U);
        if(with_outlier&&i==0)value=-90.0;
        result.short_term_3s_grid100.push_back({
            static_cast<std::int64_t>(i)*4800,
            {value,real::Readiness::measured}});
    }
    return result;
}
class LraNativeTest final:public QObject {
    Q_OBJECT
private slots:
    void EbuNearestPercentileIsNotType7();
    void AbsoluteRelativeGateAndFullReferenceGrade();
    void NoVirtualTrailingOrActiveSecondsFabricated();
    void InvalidOrStaleNativeDataCannotAppearAuthoritative();
};
void LraNativeTest::EbuNearestPercentileIsNotType7() {
    std::vector<double> v;
    for(int i=0;i<20;++i)v.push_back(static_cast<double>(i));
    QCOMPARE(*real::discrete_ebu_percentile(v,0.10),2.0);
    QCOMPARE(*real::discrete_ebu_percentile(v,0.95),18.0);
    QCOMPARE(*real::discrete_ebu_percentile(v,0.95)-
             *real::discrete_ebu_percentile(v,0.10),16.0);
}
void LraNativeTest::AbsoluteRelativeGateAndFullReferenceGrade() {
    const auto real_source=source(610,true);
    const auto r=real::observe_lra_from_real_m19(
        real_source,60*48000,true);
    QCOMPARE(r.reason,real::LraNativeReason::ready);
    QCOMPARE(r.absolute_gated_windows,std::uint64_t{609});
    QVERIFY(r.relative_retained_windows>=100);
    QVERIFY(r.is_reference_grade());
    QVERIFY(r.lra_lu);
    QVERIFY(*r.lra_lu>=0.0);
    QVERIFY(!r.authorizes_m10());
}
void LraNativeTest::NoVirtualTrailingOrActiveSecondsFabricated() {
    const auto in=source(610);
    const auto unknown=real::observe_lra_from_real_m19(
        in,60*48000,false);
    QCOMPARE(unknown.reason,
             real::LraNativeReason::
                 computed_but_reference_grade_unattested);
    QVERIFY(unknown.lra_lu);
    QVERIFY(!unknown.is_reference_grade());
    const auto short_active=real::observe_lra_from_real_m19(
        in,59*48000,true);
    QCOMPARE(short_active.reason,
             real::LraNativeReason::
                 insufficient_active_duration_or_retained);
}
void LraNativeTest::InvalidOrStaleNativeDataCannotAppearAuthoritative() {
    auto in=source(610);
    in.short_term_3s_grid100[5].absolute_start_frame+=1;
    const auto invalid=real::observe_lra_from_real_m19(
        in,60*48000,true);
    QCOMPARE(invalid.reason,real::LraNativeReason::invalid_grid_or_numeric);
    in=source(610);
    in.short_term_valid_windows=0;
    QVERIFY(!real::observe_lra_from_real_m19(
        in,60*48000,true).is_reference_grade());
    in=source(610);
    in.owner.full_track_owner_confirmed=false;
    QVERIFY(!real::observe_lra_from_real_m19(
        in,60*48000,true).is_reference_grade());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::LraNativeTest)
#include "test_l3_m06_real_lra_native.moc"
