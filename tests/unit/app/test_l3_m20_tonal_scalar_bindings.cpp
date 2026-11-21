#include "internal/l3_m20_tonal_scalar_bindings.hpp"
#include <QtTest/QTest>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] real::M20TonalTriple sample() {
    real::M20TonalTriple out;
    out.exact_common={20.0,20000.0};
    out.exact_accepted_density_floor=1e-12;
    const std::array<std::string,3> ids{"PREP","GOLD","CAND"};
    for(std::size_t i=0;i<3U;++i) {
        auto& s=out.sources[i];
        s.owner={ids[i],static_cast<std::uint64_t>(i+1),true};
        const double offset=static_cast<double>(i);
        s.observed_from_real_m20.t3_centroid_t4t5_rolloffs=
            real::TonalT3T5{
                {20.0,20000.0},1.0,1000.0,1200.0,1500.0,
                9.0+offset,10.0+offset,11.0+offset};
        s.observed_from_real_m20.t2_spectral_tilt=
            real::TonalSlope{1.0+offset,72,70,1e-12};
        s.observed_from_real_m20.t6_high_frequency_slope=
            real::TonalSlope{2.0+offset,12,10,1e-12};
    }
    for(std::size_t i=0;i<5U;++i)
        out.features[i]={
            "exact-m20-phase6-tonal-definition-"+std::to_string(i),
            true,{true,true,true}};
    return out;
}
class M20TonalBindingTest final:public QObject {
    Q_OBJECT
private slots:
    void fiveOriginalSlotsPreserveLog2AndSlopeCoordinates();
    void missingReferenceGradeKeepsOriginalMass();
    void changedCommonBandwidthAndFloorFailClosed();
};
void M20TonalBindingTest::
fiveOriginalSlotsPreserveLog2AndSlopeCoordinates() {
    const auto r=real::bind_real_m20_t02_t06(sample());
    QCOMPARE(r.problem,real::TonalBindingProblem::none);
    QVERIFY(r.slots);
    QCOMPARE(r.slots->size(),std::size_t{5});
    for(std::size_t i=0;i<5U;++i) {
        QCOMPARE((*r.slots)[i].configured.size(),std::size_t{1});
        QVERIFY((*r.slots)[i].configured[0].current_valid);
        QCOMPARE((*r.slots)[i].configured[0].paired_native_support,1.0);
    }
    QCOMPARE(*(*r.slots)[0].configured[0].prepared.z,1.0);
    QCOMPARE(*(*r.slots)[1].configured[0].prepared.z,9.0);
    QCOMPARE(*(*r.slots)[2].configured[0].prepared.z,10.0);
    QCOMPARE(*(*r.slots)[3].configured[0].prepared.z,11.0);
    QCOMPARE(*(*r.slots)[4].configured[0].prepared.z,2.0);
    QVERIFY(!r.authorizes_m10());
}
void M20TonalBindingTest::missingReferenceGradeKeepsOriginalMass() {
    auto x=sample();
    x.features[2].reference_grade[1]=false;
    const auto r=real::bind_real_m20_t02_t06(x);
    QVERIFY(r.slots);
    QCOMPARE((*r.slots)[2].configured.size(),std::size_t{1});
    QVERIFY(!(*r.slots)[2].configured[0].current_valid);
    QCOMPARE((*r.slots)[2].configured[0].paired_native_support,0.0);
}
void M20TonalBindingTest::
changedCommonBandwidthAndFloorFailClosed() {
    auto x=sample();
    x.sources[0].observed_from_real_m20
        .t3_centroid_t4t5_rolloffs->common.high_hz=12000.0;
    QCOMPARE(real::bind_real_m20_t02_t06(x).problem,
             real::TonalBindingProblem::
                common_bandwidth_or_floor_unresolved);
    x=sample();
    x.sources[1].observed_from_real_m20
        .t2_spectral_tilt->supplied_density_floor=1e-9;
    QCOMPARE(real::bind_real_m20_t02_t06(x).problem,
             real::TonalBindingProblem::
                common_bandwidth_or_floor_unresolved);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M20TonalBindingTest)
#include "test_l3_m20_tonal_scalar_bindings.moc"
