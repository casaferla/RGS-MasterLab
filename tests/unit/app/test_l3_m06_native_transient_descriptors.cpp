#include "internal/l3_m06_native_transient_descriptors.hpp"
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] std::vector<real::CanonicalTransientEvent>
events() {
    std::vector<real::CanonicalTransientEvent> out;
    for(std::uint64_t i=0;i<12U;++i)
        out.push_back({i,0.5+static_cast<double>(i),6.0,
            1.0+static_cast<double>(i),
            2.0+static_cast<double>(i),
            4.0+static_cast<double>(i)});
    return out;
}
class M06TransientNativeTest final:public QObject {
    Q_OBJECT
private slots:
    void independentDescriptorMasksPreserveCanonicalDenominator();
    void D09QuantileBeforeLog2AndNoTimeKnob();
    void eightEventsAndFiveSecondsReferenceGradeGates();
    void invalidPopulationOrNonpositiveRiseNeverImputed();
};
void M06TransientNativeTest::
independentDescriptorMasksPreserveCanonicalDenominator() {
    auto e=events();
    e[1].peak_minus_local_rms_db.reset();
    e[3].peak_minus_local_rms_db.reset();
    const auto out=real::observe_phase6_native_transient_descriptors(
        e,48000*7,48000,true);
    const auto& d07=out.d07_d08_d09[0];
    const auto& d08=out.d07_d08_d09[1];
    QCOMPARE(d07.reason,real::TransientDescriptorFailure::ready);
    QCOMPARE(d08.reason,real::TransientDescriptorFailure::ready);
    QCOMPARE(d07.valid_descriptor_events,std::uint64_t{12});
    QCOMPARE(d08.valid_descriptor_events,std::uint64_t{10});
    QCOMPARE(*d07.native_support,1.0);
    QCOMPARE(*d08.native_support,10.0/12.0);
    QVERIFY(d08.canonical_type7_p10_p50_p90[1]);
    QVERIFY(!out.authorizes_m10());
}
void M06TransientNativeTest::
D09QuantileBeforeLog2AndNoTimeKnob() {
    const auto out=real::observe_phase6_native_transient_descriptors(
        events(),48000*7,48000,true);
    const auto& rise=out.d07_d08_d09[2];
    QCOMPARE(rise.reason,real::TransientDescriptorFailure::ready);
    QVERIFY(rise.canonical_type7_p10_p50_p90[1]);
    QVERIFY(std::abs(*rise.canonical_type7_p10_p50_p90[1]-
                     std::log2(9.5))<1e-12);
}
void M06TransientNativeTest::
eightEventsAndFiveSecondsReferenceGradeGates() {
    const auto short_scope=real::observe_phase6_native_transient_descriptors(
        events(),48000*4,48000,true);
    for(const auto& x:short_scope.d07_d08_d09)
        QCOMPARE(x.reason,
                 real::TransientDescriptorFailure::
                    insufficient_events_or_active_scope);
    auto few=events();few.resize(7);
    const auto too_few=real::observe_phase6_native_transient_descriptors(
        few,48000*7,48000,true);
    for(const auto& x:too_few.d07_d08_d09)
        QVERIFY(!x.reference_grade());
}
void M06TransientNativeTest::
invalidPopulationOrNonpositiveRiseNeverImputed() {
    auto source=events();
    source[0].attack_rise_time_ms=0.0;
    const auto invalid=real::observe_phase6_native_transient_descriptors(
        source,48000*7,48000,true);
    for(const auto& x:invalid.d07_d08_d09)
        QCOMPARE(x.reason,
                 real::TransientDescriptorFailure::
                    invalid_descriptor_or_numeric);
    source=events();source[4].canonical_event_ordinal=2;
    const auto nonmonotone=real::observe_phase6_native_transient_descriptors(
        source,48000*7,48000,true);
    for(const auto& x:nonmonotone.d07_d08_d09)
        QCOMPARE(x.reason,
                 real::TransientDescriptorFailure::
                    invalid_canonical_population);
    QVERIFY(!real::observe_phase6_native_transient_descriptors(
        events(),48000*7,48000,false)
            .d07_d08_d09[0].reference_grade());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M06TransientNativeTest)
#include "test_l3_m06_native_transient_descriptors.moc"
