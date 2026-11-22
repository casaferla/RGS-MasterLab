#include "internal/l3_m06_m20_stereo_to_m11_raw_safety.hpp"
#include <QtTest/QTest>
#include <cstddef>
namespace rgsml::tests {
namespace {
namespace n=rgsml::app::l3_automatch::m05m06_real;
namespace m=rgsml::app::l3_automatch::m11_frozen;
[[nodiscard]] n::NativeStereoSafetyPair sample() {
    n::NativeStereoSafetyPair p;
    p.prepared_owner={"PREPARED",1,true};
    p.candidate_owner={"CANDIDATE",2,true};
    p.exact_current_phase6_frame_scopes_and_owners=true;
    p.exact_native_low_end_frame_gate_equal_to_m19=true;
    p.no_forbidden_haas_or_decorrelation_in_candidate=true;
    auto init=[](rgsml::analysis::TonalStereoAnalysisResult& x) {
        x.sample_rate_hz=48000;x.scope_frame_count=48000*3;
        x.channel_count=2;
        x.correlation_p10={0.2,
            rgsml::analysis::TonalStereoMetricState::AVAILABLE};
        for(std::size_t i=0;i<7U;++i)
            x.correlation_grid100.push_back({
                4800U*i,{i==0? -0.01:0.5,
                    rgsml::analysis::TonalStereoMetricState::AVAILABLE}});
    };
    init(p.prepared_m20);init(p.candidate_m20);
    p.prepared_low.numerical_source_valid=true;
    p.candidate_low.numerical_source_valid=true;
    p.prepared_low.low_end_gate_current_and_identical=true;
    p.candidate_low.low_end_gate_current_and_identical=true;
    for(std::size_t i=0;i<3U;++i) {
        auto& pre=p.prepared_low;
        auto& cand=p.candidate_low;
        pre.low_end_width[i].raw_ratio_or_rho=0.25;
        cand.low_end_width[i].raw_ratio_or_rho=0.20;
        pre.low_end_correlation[i].raw_ratio_or_rho=0.7;
        cand.low_end_correlation[i].raw_ratio_or_rho=0.69;
    }
    return p;
}
class StereoSafetyBridgeTest final:public QObject {
    Q_OBJECT
private slots:
    void realNativeSourcesSupplyEveryM11RawCutoff();
    void missingRawCorrelationOrPhase6LowEndGateIsHardReject();
    void actualCandidateLowEndWideningCannotHideBehindTarget();
};
void StereoSafetyBridgeTest::realNativeSourcesSupplyEveryM11RawCutoff(){
    const auto mapped=n::bind_actual_m20_stereo_safety_to_m11(sample());
    QCOMPARE(mapped.failure,n::StereoSafetyBridgeFailure::none);
    QVERIFY(mapped.raw_evidence);
    QVERIFY(m::verify_m11_native_stereo_hard_safety(
        *mapped.raw_evidence).passes_hard_gate());
    QVERIFY(!mapped.authorizes_release());
}
void StereoSafetyBridgeTest::
missingRawCorrelationOrPhase6LowEndGateIsHardReject(){
    auto x=sample();
    x.candidate_m20.correlation_grid100.clear();
    QCOMPARE(n::bind_actual_m20_stereo_safety_to_m11(x).failure,
             n::StereoSafetyBridgeFailure::missing_raw_local_corr);
    x=sample();
    x.exact_native_low_end_frame_gate_equal_to_m19=false;
    QCOMPARE(n::bind_actual_m20_stereo_safety_to_m11(x).failure,
             n::StereoSafetyBridgeFailure::invalid_coherent_low_end);
}
void StereoSafetyBridgeTest::
actualCandidateLowEndWideningCannotHideBehindTarget(){
    auto x=sample();
    x.candidate_low.low_end_width[2].raw_ratio_or_rho=0.3;
    const auto facts=n::bind_actual_m20_stereo_safety_to_m11(x);
    QVERIFY(facts.raw_evidence);
    const auto verdict=m::verify_m11_native_stereo_hard_safety(
        *facts.raw_evidence);
    QCOMPARE(verdict.reason,
             m::NativeStereoSafetyReason::forbidden_low_end_widening);
    QCOMPARE(*verdict.low_end_cutoff_ordinal,std::size_t{2});
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::StereoSafetyBridgeTest)
#include "test_l3_m06_m20_stereo_to_m11_raw_safety.moc"
