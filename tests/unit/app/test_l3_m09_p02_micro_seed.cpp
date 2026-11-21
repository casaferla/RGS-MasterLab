#include "internal/l3_m09_p02_micro_seed.hpp"
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
[[nodiscard]] m09::MicroTriplet triplet(double delta) {
    m09::MicroTriplet t{};
    for(auto& c:t)c={true,false,true,10.0,10.0+delta,
                     0.1,1.0/3.0,1.0};
    return t;
}
[[nodiscard]] m09::MicroTriplet context() {
    auto t=triplet(0.0);
    for(auto& c:t)c.requested=false;
    return t;
}
class M09MicroSeedTest final:public QObject {
    Q_OBJECT
private slots:
    void frozenWeightedVectorMixtureAndLimit();
    void coherenceIsRequiredWithinAndAcrossVectors();
    void d09OnlyAmbiguityNotTimeAmplitudeMapping();
    void missingContextProtectionSupportAndStaleFailClosed();
};
void M09MicroSeedTest::frozenWeightedVectorMixtureAndLimit() {
    const auto positive=m09::derive_micro_p02(
        triplet(2.0),triplet(1.0),context(),true,true);
    QCOMPARE(positive.reason,m09::MicroReason::ready);
    QVERIFY(positive.raw_db&&positive.limited_db);
    QVERIFY(std::abs(*positive.raw_db-1.7)<1e-12);
    QVERIFY(std::abs(*positive.limited_db-1.7)<1e-12);
    const auto limited=m09::derive_micro_p02(
        triplet(5.0),triplet(4.0),context(),true,true);
    QCOMPARE(limited.reason,m09::MicroReason::limited_to_seed_range);
    QCOMPARE(*limited.limited_db,3.0);
    QVERIFY(*limited.raw_db>3.0);
    const auto negative=m09::derive_micro_p02(
        triplet(-5.0),triplet(-4.0),context(),true,true);
    QCOMPARE(*negative.limited_db,-3.0);
    QVERIFY(!positive.authorizes_product_publication());
}
void M09MicroSeedTest::coherenceIsRequiredWithinAndAcrossVectors() {
    auto contrast=triplet(1.0);
    contrast[2].optimization_z=8.0;
    QCOMPARE(m09::derive_micro_p02(
        contrast,triplet(1.0),context(),true,true).reason,
        m09::MicroReason::incoherent_vector_direction);
    QCOMPARE(m09::derive_micro_p02(
        triplet(1.0),triplet(-1.0),context(),true,true).reason,
        m09::MicroReason::incoherent_vector_direction);
}
void M09MicroSeedTest::d09OnlyAmbiguityNotTimeAmplitudeMapping() {
    auto d09=triplet(1.0);
    d09[2].optimization_z=9.0;
    QCOMPARE(m09::derive_micro_p02(
        triplet(1.0),triplet(1.0),d09,true,true).reason,
        m09::MicroReason::conflicting_d09_rise_direction);
    d09[2].optimization_z=11.0;
    const auto accepted=m09::derive_micro_p02(
        triplet(1.0),triplet(1.0),d09,true,true);
    QCOMPARE(accepted.reason,m09::MicroReason::ready);
    QCOMPARE(*accepted.limited_db,1.0); // D09 never changes amplitude
}
void M09MicroSeedTest::missingContextProtectionSupportAndStaleFailClosed() {
    const auto nominal=triplet(1.0);
    const auto d09=context();
    QCOMPARE(m09::derive_micro_p02(
        nominal,nominal,d09,false,true).reason,
        m09::MicroReason::missing_exact_fixed_or_event_evidence);
    QCOMPARE(m09::derive_micro_p02(
        nominal,nominal,d09,true,false).reason,
        m09::MicroReason::missing_exact_fixed_or_event_evidence);
    auto bad=nominal;
    bad[0].exact_current=false;
    QCOMPARE(m09::derive_micro_p02(
        bad,nominal,d09,true,true).reason,
        m09::MicroReason::missing_exact_fixed_or_event_evidence);
    bad=nominal;
    bad[0].paired_support=0.0;
    bad[1].paired_support=0.5;
    const auto supported=m09::derive_micro_p02(
        bad,nominal,d09,true,true);
    QVERIFY(supported.raw_db);
    QVERIFY(std::abs(*supported.raw_db-1.0)<1e-12);
    bad=nominal;
    for(auto& c:bad)c.protected_from_action=true;
    QCOMPARE(m09::derive_micro_p02(
        bad,nominal,d09,true,true).reason,
        m09::MicroReason::missing_non_deadband_movement);
    bad=nominal;
    bad[1].optimization_z=std::numeric_limits<double>::infinity();
    QCOMPARE(m09::derive_micro_p02(
        bad,nominal,d09,true,true).reason,
        m09::MicroReason::invalid_component);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M09MicroSeedTest)
#include "test_l3_m09_p02_micro_seed.moc"
