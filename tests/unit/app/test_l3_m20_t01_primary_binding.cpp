#include "internal/l3_m20_t01_primary_binding.hpp"
#include <QtTest/QTest>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] real::T01TripleEvidence make_input() {
    real::T01TripleEvidence e;
    e.authoritative_primary_grid={{0,95.0,100.0,105.0},
                                  {1,105.0,110.0,115.0},
                                  {2,115.0,120.0,125.0}};
    e.exact_native_band_support_prepared_gold={
        std::vector<std::optional<double>>{1.0,0.8,0.4},
        std::vector<std::optional<double>>{1.0,0.5,0.2}};
    e.phase6_source_reference_grade={true,true,true};
    e.exact_primary_t01_feature_dependency="exact-phase6-primary-T01";
    e.exact_phase6_grid_and_native_support_current=true;
    const std::array<std::string,3> names{"PREP","GOLD","CAND"};
    for(std::size_t k=0;k<3U;++k) {
        auto& s=e.actual_sources[k];
        s.owner={names[k],static_cast<std::uint64_t>(k+1),true};
        s.sample_rate_hz=48000;s.scope_frame_count=48000*10;
        for(const auto& b:e.authoritative_primary_grid)
            s.profile_1_12.push_back({
                b.canonical_index,b.center_hz,b.low_hz,b.high_hz,
                {-10.0+static_cast<double>(b.canonical_index),
                 real::Readiness::measured}});
    }
    return e;
}
class T01PrimaryBindingTest final:public QObject {
    Q_OBJECT
private slots:
    void primaryConfiguredNativeSupportAndNoMassRenormalization();
    void missingBandCannotDisappearOrBeSubstituted();
    void modifiedCommonGeometryAndSupportRejected();
};
void T01PrimaryBindingTest::
primaryConfiguredNativeSupportAndNoMassRenormalization() {
    const auto r=real::bind_real_t01_primary(make_input());
    QCOMPARE(r.reason,real::T01BindingFailure::none);
    QVERIFY(r.slot);
    QCOMPARE(r.slot->binding_id,
             std::string{"rgsml.reference-match.binding.tonal.t01"});
    QCOMPARE(r.slot->configured.size(),std::size_t{3});
    QCOMPARE(r.slot->configured[0].paired_native_support,1.0);
    QCOMPARE(r.slot->configured[1].paired_native_support,0.5);
    QCOMPARE(r.slot->configured[2].paired_native_support,0.2);
    QVERIFY(!r.authorizes_plan());
}
void T01PrimaryBindingTest::missingBandCannotDisappearOrBeSubstituted() {
    auto input=make_input();
    input.actual_sources[1].profile_1_12.erase(
        input.actual_sources[1].profile_1_12.begin()+1);
    const auto r=real::bind_real_t01_primary(input);
    QVERIFY(r.slot);
    QCOMPARE(r.slot->configured.size(),std::size_t{3});
    QVERIFY(!r.slot->configured[1].current_valid);
    QCOMPARE(r.slot->configured[1].paired_native_support,0.0);
    QVERIFY(r.slot->configured[0].current_valid);
}
void T01PrimaryBindingTest::modifiedCommonGeometryAndSupportRejected() {
    auto input=make_input();
    input.actual_sources[0].profile_1_12[1].low_hz=106.0;
    QCOMPARE(real::bind_real_t01_primary(input).reason,
             real::T01BindingFailure::mismatched_authoritative_geometry);
    input=make_input();
    input.exact_native_band_support_prepared_gold[0][1]=1.2;
    QCOMPARE(real::bind_real_t01_primary(input).reason,
             real::T01BindingFailure::invalid_native_support);
    input=make_input();
    input.exact_phase6_grid_and_native_support_current=false;
    QVERIFY(!real::bind_real_t01_primary(input).slot);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::T01PrimaryBindingTest)
#include "test_l3_m20_t01_primary_binding.moc"
