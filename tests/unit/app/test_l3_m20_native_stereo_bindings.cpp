#include "internal/l3_m20_native_stereo_bindings.hpp"
#include <QtTest/QTest>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
namespace m08=rgsml::app::l3_automatch::m08_frozen;
[[nodiscard]] real::M20StereoTriple sample() {
    real::M20StereoTriple r;
    const std::array<std::string,3> ids{"PREP","GOLD","CANDIDATE"};
    const std::array<real::SourceReadings*,3> streams{{
        &r.prepared,&r.reference,&r.actual_candidate}};
    for(std::size_t j=0;j<3U;++j) {
        auto& s=*streams[j];
        s.owner={ids[j],static_cast<std::uint64_t>(j+1),true};
        s.sample_rate_hz=48000;s.scope_frame_count=48000*10;
        const auto raw=j==0?0.0:(j==1?1.0:0.5);
        s.broadband_width={raw,real::Readiness::measured};
        s.broadband_width_z=real::width_mono_endpoint_log2(
            s.broadband_width);
        for(std::size_t i=0;i<3U;++i)
            s.correlation_fisher_z[i]={
                static_cast<double>(i)/10.0+static_cast<double>(j)/20.0,
                real::Readiness::measured};
        s.correlation_native_support=j==0?0.9:j==1?0.8:0.7;
    }
    for(std::size_t i=0;i<2U;++i)
        r.authority[i]={
            "exact-phase6-m20-stereo-feature-"+std::to_string(i),
            true,{true,true,true}};
    return r;
}
class M20NativeStereoBindingTest final:public QObject {
    Q_OBJECT
private slots:
    void monoEndpointDistinctFromPositiveWidthAndThreeFisher();
    void missingLocalCorrelationDoesNotDeleteComponents();
    void staleAuthorityOrInvalidSupportCannotYieldFalseMatch();
};
void M20NativeStereoBindingTest::
monoEndpointDistinctFromPositiveWidthAndThreeFisher() {
    const auto r=real::bind_real_m20_s01_s02(sample());
    QCOMPARE(r.reason,real::M20StereoFailure::none);
    QVERIFY(r.feature_slots);
    QCOMPARE((*r.feature_slots)[0].configured.size(),std::size_t{1});
    QCOMPARE((*r.feature_slots)[1].configured.size(),std::size_t{3});
    const auto& width=(*r.feature_slots)[0].configured[0];
    QCOMPARE(width.prepared.endpoint,m08::Endpoint::mono_side_muted);
    QCOMPARE(width.reference.endpoint,m08::Endpoint::ordinary);
    QCOMPARE(*width.prepared.z,-24.0);
    QCOMPARE(width.paired_native_support,1.0);
    for(const auto& c:(*r.feature_slots)[1].configured)
        QCOMPARE(c.paired_native_support,0.8);
    QVERIFY(!r.authorizes_product_publication());
}
void M20NativeStereoBindingTest::
missingLocalCorrelationDoesNotDeleteComponents() {
    auto source=sample();
    source.reference.correlation_fisher_z[1].state=
        real::Readiness::missing;
    const auto r=real::bind_real_m20_s01_s02(source);
    QVERIFY(r.feature_slots);
    QCOMPARE((*r.feature_slots)[1].configured.size(),std::size_t{3});
    QVERIFY(!(*r.feature_slots)[1].configured[1].current_valid);
    QCOMPARE((*r.feature_slots)[1].configured[1].paired_native_support,0.0);
    QVERIFY((*r.feature_slots)[1].configured[0].current_valid);
}
void M20NativeStereoBindingTest::
staleAuthorityOrInvalidSupportCannotYieldFalseMatch() {
    auto source=sample();
    source.prepared.correlation_native_support=1.01;
    QCOMPARE(real::bind_real_m20_s01_s02(source).reason,
             real::M20StereoFailure::invalid_native_support);
    source=sample();
    source.authority[0].exact_feature_dependency.clear();
    QCOMPARE(real::bind_real_m20_s01_s02(source).reason,
             real::M20StereoFailure::unresolved_phase6_feature);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M20NativeStereoBindingTest)
#include "test_l3_m20_native_stereo_bindings.moc"
