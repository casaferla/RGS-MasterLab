#include "internal/l3_m11_p03_protected_no_regression.hpp"
#include <QtTest/QTest>
#include <limits>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m11=rgsml::app::l3_automatch::m11_frozen;
[[nodiscard]] m11::ProtectedComponent c(
    double p,double g,double f,double x,double d) {
    return {true,true,true,p,g,f,x,d};
}
class M11ProtectionTest final:public QObject {
    Q_OBJECT
private slots:
    void protectedAlreadyCloseRemainsWithinDeadband();
    void fixedProgressNeedsBothDirectionalAndDistanceInequalities();
    void noAggregateCompensationAndNegativeDirection();
    void missingAndNonfiniteFailsClosed();
};
void M11ProtectionTest::protectedAlreadyCloseRemainsWithinDeadband() {
    const std::vector<m11::ProtectedComponent> pass{c(0,0.2,0.3,0.65,0.5)};
    auto result=m11::verify_frozen_protected_no_regression(pass,true,true);
    QVERIFY(result.is_admissible());
    QCOMPARE(result.protected_count,std::size_t{1});
    auto fail=pass;
    fail[0].candidate_z=0.71;
    result=m11::verify_frozen_protected_no_regression(fail,true,true);
    QCOMPARE(result.reason,m11::NoRegressionReason::protected_component_regressed);
    QCOMPARE(*result.first_bad_canonical_index,std::size_t{0});
}
void M11ProtectionTest::
fixedProgressNeedsBothDirectionalAndDistanceInequalities() {
    // P=0,G=10,F=8,d=.5, directional floor=7.5 and
    // absolute Gold distance bound=2.5. 7.5 is exactly admissible.
    const std::vector<m11::ProtectedComponent> pass{c(0,10,8,7.5,0.5)};
    QVERIFY(m11::verify_frozen_protected_no_regression(
        pass,true,true).is_admissible());
    auto bad=pass;bad[0].candidate_z=7.499;
    QCOMPARE(m11::verify_frozen_protected_no_regression(
        bad,true,true).reason,
        m11::NoRegressionReason::protected_component_regressed);
    bad=pass;bad[0].candidate_z=12.51; // passes floor; fails |C-G|
    QCOMPARE(m11::verify_frozen_protected_no_regression(
        bad,true,true).reason,
        m11::NoRegressionReason::protected_component_regressed);
}
void M11ProtectionTest::noAggregateCompensationAndNegativeDirection() {
    std::vector<m11::ProtectedComponent> pair{
        c(0,10,8,10,0.25),c(10,0,2,2.5,0.5)};
    QVERIFY(m11::verify_frozen_protected_no_regression(
        pair,true,true).is_admissible());
    pair[1].candidate_z=3.0;
    const auto failed=m11::verify_frozen_protected_no_regression(
        pair,true,true);
    QCOMPARE(failed.reason,
             m11::NoRegressionReason::protected_component_regressed);
    QCOMPARE(*failed.first_bad_canonical_index,std::size_t{1});
    QVERIFY(!failed.authorizes_product_publication());
}
void M11ProtectionTest::missingAndNonfiniteFailsClosed() {
    const std::vector<m11::ProtectedComponent> valid{c(0,10,9,9,0.1)};
    QVERIFY(!m11::verify_frozen_protected_no_regression(
        valid,false,true).is_admissible());
    QVERIFY(!m11::verify_frozen_protected_no_regression(
        valid,true,false).is_admissible());
    std::vector<m11::ProtectedComponent> empty;
    QVERIFY(!m11::verify_frozen_protected_no_regression(
        empty,true,true).is_admissible());
    auto stale=valid;stale[0].exact_current=false;
    QCOMPARE(m11::verify_frozen_protected_no_regression(
        stale,true,true).reason,
        m11::NoRegressionReason::incomplete_required_component);
    stale=valid;stale[0].candidate_z=
        std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(m11::verify_frozen_protected_no_regression(
        stale,true,true).reason,m11::NoRegressionReason::invalid_numeric);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M11ProtectionTest)
#include "test_l3_m11_p03_protected_no_regression.moc"
