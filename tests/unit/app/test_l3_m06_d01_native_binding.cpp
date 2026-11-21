#include "internal/l3_m06_d01_native_binding.hpp"
#include <QtTest/QTest>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] real::D01TripleEvidence input() {
    real::D01TripleEvidence e;
    e.exact_feature_dependency="exact-phase6-shortterm-3s-native-sha";
    e.exact_phase6_feature_revision_and_context_current=true;
    e.phase6_reference_grade={true,true,true};
    const std::array<std::string,3> names{"PREP","GOLD","CANDIDATE"};
    for(std::size_t j=0;j<3U;++j) {
        auto& s=e.source[j];
        s.owner={names[j],static_cast<std::uint64_t>(j+1),true};
        s.sample_rate_hz=48000;s.scope_frame_count=48000*13;
        s.short_term_complete_windows=101;
        s.short_term_valid_windows=101;
        s.short_term_native_support=1.0;
        s.short_term_p50_lufs={-20.0+static_cast<double>(j),
                               real::Readiness::measured};
        for(std::int64_t i=0;i<101;++i)
            s.short_term_3s_grid100.push_back({
                i*4800,
                {-20.0+static_cast<double>(j),
                 real::Readiness::measured}});
    }
    return e;
}
class D01NativeBindingTest final:public QObject {
    Q_OBJECT
private slots:
    void centeredD01Type7ThreeSlotsNoNewAbsoluteLocation();
    void missingNativeReferenceCannotBecomeThreeTargetValues();
    void candidateMissingRemainsUnachieved();
};
void D01NativeBindingTest::
centeredD01Type7ThreeSlotsNoNewAbsoluteLocation() {
    const auto bound=real::bind_real_d01_native(input());
    QCOMPARE(bound.failure,real::D01BindingFailure::none);
    QVERIFY(bound.slot);
    QCOMPARE(bound.slot->binding_id,
             std::string{"rgsml.reference-match.binding.dynamics.d01"});
    QCOMPARE(bound.slot->configured.size(),std::size_t{3});
    for(const auto& c:bound.slot->configured) {
        QVERIFY(c.current_valid);
        QCOMPARE(*c.prepared.z,0.0);
        QCOMPARE(*c.reference.z,0.0);
        QCOMPARE(*c.current_candidate_z,0.0);
        QCOMPARE(c.paired_native_support,1.0);
    }
    QVERIFY(!bound.authorizes_product_publication());
}
void D01NativeBindingTest::
missingNativeReferenceCannotBecomeThreeTargetValues() {
    auto evidence=input();
    evidence.source[1].short_term_p50_lufs.state=
        real::Readiness::missing;
    const auto out=real::bind_real_d01_native(evidence);
    QVERIFY(out.slot);
    QCOMPARE(out.slot->configured.size(),std::size_t{3});
    for(const auto& c:out.slot->configured) {
        QVERIFY(!c.current_valid);
        QVERIFY(!c.prepared.z);
        QVERIFY(!c.reference.z);
        QCOMPARE(c.paired_native_support,0.0);
    }
}
void D01NativeBindingTest::candidateMissingRemainsUnachieved() {
    auto evidence=input();
    evidence.source[2].short_term_valid_windows=0;
    const auto out=real::bind_real_d01_native(evidence);
    QVERIFY(out.slot);
    for(const auto& c:out.slot->configured) {
        QVERIFY(c.current_valid);
        QVERIFY(!c.current_candidate_z);
    }
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::D01NativeBindingTest)
#include "test_l3_m06_d01_native_binding.moc"
