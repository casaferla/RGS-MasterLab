#include "internal/l3_m19_native_macro_loudness_bindings.hpp"
#include <QtTest/QTest>
#include <array>
#include <cstddef>
#include <string>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] real::M19NativeTriple populated() {
    real::M19NativeTriple e;
    const std::array<std::string,3> ids{"PREP","GOLD","CANDIDATE"};
    const std::array<real::SourceReadings*,3> owners{{
        &e.prepared,&e.reference,&e.actual_candidate}};
    for(std::size_t j=0;j<3U;++j) {
        auto& s=*owners[j];
        s.owner={ids[j],static_cast<std::uint64_t>(j+1),true};
        s.sample_rate_hz=48000;
        s.scope_frame_count=48000*70;
        const auto value=10.0+static_cast<double>(j);
        s.plr={value,real::Readiness::measured};
        s.global_crest_db={value,real::Readiness::measured};
        s.integrated_lufs={-20.0+static_cast<double>(j),
                           real::Readiness::measured};
        s.short_term_p50_lufs={-18.0+static_cast<double>(j),
                               real::Readiness::measured};
        s.true_peak_db_tp={-1.0+static_cast<double>(j)/10.0,
                           real::Readiness::measured};
        s.short_term_native_support=j==0?0.9:j==1?0.75:0.6;
    }
    for(std::size_t i=0;i<5U;++i)
        e.authority[i]={
            "exact-m19-phase6-feature-definition-"+std::to_string(i),
            true,{true,true,true}};
    return e;
}
class M19MacroLoudnessBindingTest final:public QObject {
    Q_OBJECT
private slots:
    void exactFiveSlotsAndNativeL02PairMinOnce();
    void truePeakUnqualifiedCannotBePromoted();
    void absentReferenceGradeRetainsConfiguredMass();
    void staleOwnerOrSourceDependencyRejectsEntireProjection();
};
void M19MacroLoudnessBindingTest::
exactFiveSlotsAndNativeL02PairMinOnce() {
    const auto e=populated();
    const auto r=real::bind_real_m19_loudness_macro(e);
    QCOMPARE(r.reason,real::M19SlotFailure::none);
    QVERIFY(r.feature_slots);
    QCOMPARE(r.feature_slots->size(),std::size_t{5});
    for(std::size_t i=0;i<5U;++i) {
        QCOMPARE((*r.feature_slots)[i].configured.size(),std::size_t{1});
        QCOMPARE((*r.feature_slots)[i].binding_id,
                 std::string{real::kM19NativeBindingIds[i]});
        QVERIFY((*r.feature_slots)[i].configured[0].current_valid);
    }
    QCOMPARE((*r.feature_slots)[3].configured[0].paired_native_support,0.75);
    QCOMPARE((*r.feature_slots)[4].configured[0].paired_native_support,1.0);
    QVERIFY(!r.authorizes_solver_or_publish());
}
void M19MacroLoudnessBindingTest::
truePeakUnqualifiedCannotBePromoted() {
    auto e=populated();
    e.reference.true_peak_db_tp.state=real::Readiness::unqualified;
    const auto r=real::bind_real_m19_loudness_macro(e);
    QVERIFY(r.feature_slots);
    QVERIFY(!(*r.feature_slots)[4].configured[0].current_valid);
    QVERIFY(!(*r.feature_slots)[4].configured[0].reference.z);
    QCOMPARE((*r.feature_slots)[4].configured[0].paired_native_support,0.0);
}
void M19MacroLoudnessBindingTest::
absentReferenceGradeRetainsConfiguredMass() {
    auto e=populated();
    e.authority[2].source_reference_grade[1]=false;
    const auto r=real::bind_real_m19_loudness_macro(e);
    QVERIFY(r.feature_slots);
    QVERIFY(!(*r.feature_slots)[2].configured[0].current_valid);
    QCOMPARE((*r.feature_slots)[2].configured.size(),std::size_t{1});
    QVERIFY(!(*r.feature_slots)[2].configured[0].reference.z);
}
void M19MacroLoudnessBindingTest::
staleOwnerOrSourceDependencyRejectsEntireProjection() {
    auto e=populated();
    e.actual_candidate.owner.resource_identity="GOLD";
    QCOMPARE(real::bind_real_m19_loudness_macro(e).reason,
             real::M19SlotFailure::stale_or_unauthenticated_owners);
    e=populated();
    e.authority[2].exact_feature_dependency.clear();
    QCOMPARE(real::bind_real_m19_loudness_macro(e).reason,
             real::M19SlotFailure::invalid_feature_authority);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M19MacroLoudnessBindingTest)
#include "test_l3_m19_native_macro_loudness_bindings.moc"
