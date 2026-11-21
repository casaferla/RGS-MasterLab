#include "internal/l3_m09_frozen_gaussian_grid.hpp"
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
class M09GaussianTest final:public QObject {
    Q_OBJECT
private slots:
    void primaryAndFallbackHaveDifferentPhysicalBinRadii();
    void validRunReflectionDoesNotBridgeMissingEvidence();
    void threeCellGateNeverInterpretsMissingAsZero();
    void illegalInputFailsWithoutInventingEvidence();
};
void M09GaussianTest::primaryAndFallbackHaveDifferentPhysicalBinRadii() {
    const std::vector<std::optional<double>> curve{
        0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0};
    const auto fine=m09::smooth_selected_valid_runs(curve,1.0/12.0);
    const auto coarse=m09::smooth_selected_valid_runs(curve,1.0/3.0);
    QCOMPARE(fine.failure,m09::GridFailure::none);
    QCOMPARE(coarse.failure,m09::GridFailure::none);
    QCOMPARE(fine.values.size(),curve.size());
    QCOMPARE(coarse.values.size(),curve.size());
    QVERIFY(fine.values[4]&&coarse.values[4]);
    QVERIFY(std::abs(*fine.values[4]-*coarse.values[4])>0.01);
    QVERIFY(!fine.authorizes_render_or_match());
}
void M09GaussianTest::validRunReflectionDoesNotBridgeMissingEvidence() {
    const std::vector<std::optional<double>> grid{
        2.0,2.0,2.0,std::nullopt,30.0,30.0,std::nullopt,7.0};
    const auto smoothed=m09::smooth_selected_valid_runs(grid,1.0/12.0);
    QCOMPARE(smoothed.failure,m09::GridFailure::none);
    QCOMPARE(smoothed.values.size(),grid.size());
    for(std::size_t i=0;i<3;++i) {
        QVERIFY(smoothed.values[i]);
        QVERIFY(std::abs(*smoothed.values[i]-2.0)<1e-12);
    }
    QVERIFY(!smoothed.values[3]);
    for(std::size_t i=4;i<6;++i) {
        QVERIFY(smoothed.values[i]);
        QVERIFY(std::abs(*smoothed.values[i]-30.0)<1e-12);
    }
    QVERIFY(!smoothed.values[6]);
    QVERIFY(smoothed.values[7]);
    QCOMPARE(*smoothed.values[7],7.0); // singleton self-reflection
    QCOMPARE(m09::reflected_valid_run_index(-1,3),std::size_t{1});
    QCOMPARE(m09::reflected_valid_run_index(3,3),std::size_t{1});
    QCOMPARE(m09::reflected_valid_run_index(-8,1),std::size_t{0});
}
void M09GaussianTest::threeCellGateNeverInterpretsMissingAsZero() {
    const std::array<std::optional<double>,6> same{
        std::nullopt,0.6,0.7,0.8,std::nullopt,-0.8};
    QVERIFY(m09::has_three_adjacent_signed_action(same,0.5));
    const std::array<std::optional<double>,6> gap{
        1.0,1.0,std::nullopt,1.0,1.0,std::nullopt};
    QVERIFY(!m09::has_three_adjacent_signed_action(gap,0.5));
    const std::array<std::optional<double>,5> changed{
        0.7,0.6,-0.7,-0.8,std::nullopt};
    QVERIFY(!m09::has_three_adjacent_signed_action(changed,0.5));
    const std::array<std::optional<double>,4> boundary{
        0.5,0.6,0.7,std::nullopt};
    QVERIFY(!m09::has_three_adjacent_signed_action(boundary,0.5));
    QVERIFY(!m09::has_three_adjacent_signed_action(same,-0.5));
}
void M09GaussianTest::illegalInputFailsWithoutInventingEvidence() {
    const std::vector<std::optional<double>> no_cells{};
    QCOMPARE(m09::smooth_selected_valid_runs(no_cells,1.0/12.0).failure,
             m09::GridFailure::empty_grid);
    const std::vector<std::optional<double>> valid{1.0,2.0};
    QCOMPARE(m09::smooth_selected_valid_runs(valid,0.0).failure,
             m09::GridFailure::illegal_spacing);
    QCOMPARE(m09::smooth_selected_valid_runs(valid,
             std::numeric_limits<double>::quiet_NaN()).failure,
             m09::GridFailure::illegal_spacing);
    const std::vector<std::optional<double>> invalid{
        1.0,std::numeric_limits<double>::infinity(),2.0};
    const auto failed=m09::smooth_selected_valid_runs(invalid,1.0/12.0);
    QCOMPARE(failed.failure,m09::GridFailure::nonfinite_observed_cell);
    QVERIFY(failed.values.empty());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M09GaussianTest)
#include "test_l3_m09_frozen_gaussian_grid.moc"
