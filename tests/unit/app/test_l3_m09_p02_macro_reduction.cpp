#include "internal/l3_m09_p02_macro_reduction.hpp"
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
[[nodiscard]] std::array<m09::MacroGroup,6> valid_groups() {
    std::array<m09::MacroGroup,6> groups;
    groups[0].components.resize(3);
    for(std::size_t g=1;g<6;++g)groups[g].components.resize(g==3U?3U:1U);
    for(std::size_t g=0;g<groups.size();++g)
        for(std::size_t j=0;j<groups[g].components.size();++j)
            groups[g].components[j]={
                false,false,true,10.0,9.0,1.0,0.5,
                1.0,1.0};
    // D01 sign orientation [-1,+1,+1]. Each canonical centered
    // component is a third of ONE group, not an extra coherent vote.
    groups[0].components[0]={true,false,true,9.0,10.0,1.0,
                             0.5,1.0/3.0,1.0};
    groups[0].components[1]={true,false,true,10.0,9.0,1.0,
                             0.5,1.0/3.0,1.0};
    groups[0].components[2]={true,false,true,10.0,9.0,1.0,
                             0.5,1.0/3.0,1.0};
    groups[1].components[0].requested=true;
    return groups;
}
class M09MacroTest final:public QObject {
    Q_OBJECT
private slots:
    void twoDistinctCoherentGroupsAndWeightedMovement();
    void d01AloneDoesNotCountAsThreeGroups();
    void d04CrestIsOneCoherentVectorGroup();
    void expansionAndMissingFixedFailClosed();
    void supportAppliedOnceAndDeadbandProtectedDoNotVote();
    void thresholdNativeType7P75AndNoFabricatedSeries();
};
void M09MacroTest::twoDistinctCoherentGroupsAndWeightedMovement() {
    const auto result=m09::reduce_macro_p02(valid_groups(),true);
    QCOMPARE(result.reason,m09::MacroSeedReason::ready);
    QCOMPARE(result.coherent_distinct_groups,std::uint32_t{2});
    QVERIFY(result.required_movement);
    QVERIFY(std::abs(*result.required_movement-1.0)<1e-12);
    QVERIFY(!result.authorizes_compression_or_publication());
}
void M09MacroTest::d01AloneDoesNotCountAsThreeGroups() {
    auto groups=valid_groups();
    groups[1].components[0].requested=false;
    const auto result=m09::reduce_macro_p02(groups,true);
    QCOMPARE(result.reason,
             m09::MacroSeedReason::insufficient_coherent_distinct_groups);
    QCOMPARE(result.coherent_distinct_groups,std::uint32_t{1});
    QVERIFY(!result.required_movement);
}
void M09MacroTest::d04CrestIsOneCoherentVectorGroup() {
    auto groups=valid_groups();
    groups[1].components[0].requested=false;
    for(auto& c:groups[0].components)c.requested=false;
    for(auto& c:groups[3].components)c.requested=true;
    const auto one=m09::reduce_macro_p02(groups,true);
    QCOMPARE(one.coherent_distinct_groups,std::uint32_t{1});
    QVERIFY(!one.required_movement);
    groups[1].components[0].requested=true;
    QVERIFY(m09::reduce_macro_p02(groups,true).required_movement);
    groups[3].components[2].z_optimization=11.0;
    QCOMPARE(m09::reduce_macro_p02(groups,true).reason,m09::MacroSeedReason::contradictory_expansion);
}
void M09MacroTest::expansionAndMissingFixedFailClosed() {
    auto groups=valid_groups();
    groups[2].components[0].requested=true;
    groups[2].components[0].z_optimization=11.0;
    const auto conflict=m09::reduce_macro_p02(groups,true);
    QCOMPARE(conflict.reason,m09::MacroSeedReason::contradictory_expansion);
    QVERIFY(!conflict.required_movement);
    groups=valid_groups();
    groups[0].components[1].exact_current=false;
    QCOMPARE(m09::reduce_macro_p02(groups,true).reason,
             m09::MacroSeedReason::missing_exact_evidence);
    QCOMPARE(m09::reduce_macro_p02(groups,false).reason,
             m09::MacroSeedReason::missing_exact_evidence);
}
void M09MacroTest::supportAppliedOnceAndDeadbandProtectedDoNotVote() {
    auto groups=valid_groups();
    groups[0].components[0].paired_support=0.0;
    groups[0].components[1].paired_support=0.0;
    groups[0].components[2].paired_support=0.5;
    groups[1].components[0].z_fixed=11.0;
    groups[1].components[0].z_optimization=9.0; // reduction2
    const auto result=m09::reduce_macro_p02(groups,true);
    QCOMPARE(result.reason,m09::MacroSeedReason::ready);
    QVERIFY(result.required_movement);
    const double expected=(1.0/6.0+2.0)/(1.0/6.0+1.0);
    QVERIFY(std::abs(*result.required_movement-expected)<1e-12);
    // Protected D02 cannot be used to manufacture a second group.
    groups[1].components[0].protected_from_action=true;
    QCOMPARE(m09::reduce_macro_p02(groups,true).reason,
             m09::MacroSeedReason::insufficient_coherent_distinct_groups);
    groups=valid_groups();
    groups[1].components[0].z_fixed=9.4;
    groups[1].components[0].z_optimization=9.0;
    QCOMPARE(m09::reduce_macro_p02(groups,true).reason,
             m09::MacroSeedReason::insufficient_coherent_distinct_groups);
}
void M09MacroTest::thresholdNativeType7P75AndNoFabricatedSeries() {
    const std::vector<double> rms{-20.0,-16.0,-12.0,-8.0,-4.0};
    const auto result=m09::compressor_threshold_type7_p75(
        rms,6.0,true);
    QCOMPARE(result.reason,m09::ThresholdSeedReason::ready);
    QVERIFY(result.threshold_dbfs);
    QCOMPARE(*result.threshold_dbfs,-11.0); // P75=-8, knee/2=3
    QCOMPARE(m09::compressor_threshold_type7_p75(
        rms,6.0,false).reason,
        m09::ThresholdSeedReason::missing_exact_native_rms);
    const std::vector<double> invalid{
        1.0,std::numeric_limits<double>::quiet_NaN()};
    QCOMPARE(m09::compressor_threshold_type7_p75(
        invalid,6.0,true).reason,
        m09::ThresholdSeedReason::invalid_native_series);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M09MacroTest)
#include "test_l3_m09_p02_macro_reduction.moc"
