#include "internal/l3_m06_s03_s06_native_binding.hpp"
#include <QtTest/QTest>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace rgsml::tests {
namespace {
namespace m=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] m::CoherentStereoTripleAuthority facts() {
    m::CoherentStereoTripleAuthority e;
    e.exact_current_common_band_grid_and_retained_gate_policy=true;
    e.exact_canonical_band_indices={1,2,3,4};
    const std::array<std::string,3> ids{"PREP","REFERENCE","CAND"};
    for(std::size_t j=0;j<3U;++j) {
        e.independent_source_owners[j]={
            ids[j],static_cast<std::uint64_t>(j+1),true};
        auto& s=e.observed[j];
        s.numerical_source_valid=true;s.native_selected_frames=100;
        s.low_end_gate_current_and_identical=true;
        for(int b:e.exact_canonical_band_indices) {
            m::StereoCanonicalBand cell;
            cell.canonical_index=b;
            cell.width.transformed_z=static_cast<double>(j)/10.0;
            cell.width.raw_ratio_or_rho=1.0;
            cell.width.native_support=0.8;
            cell.correlation.transformed_z=0.2+0.1*j;
            cell.correlation.raw_ratio_or_rho=0.2;
            cell.correlation.native_support=0.9;
            s.multiband.push_back(cell);
        }
        for(std::size_t k=0;k<3U;++k) {
            s.low_end_width[k].transformed_z=-0.5;
            s.low_end_correlation[k].transformed_z=0.4;
            s.low_end_width[k].native_support=1.0;
            s.low_end_correlation[k].native_support=1.0;
        }
    }
    for(std::size_t i=0;i<4U;++i) {
        e.source_phase6_reference_grade[i]={true,true,true};
        e.immutable_feature_definition_digests[i]=
            "exact-Phase6-S03-S06-definition-"+std::to_string(i);
    }
    return e;
}
class S03S06NativeBindingTest final:public QObject {
    Q_OBJECT
private slots:
    void entireFourSlotsAndOriginalConfiguredMass();
    void exactSideEndpointAndNoAccidentalLowEndGate();
    void malformedGridFailsButMissingFeatureKeepsMass();
};
void S03S06NativeBindingTest::
entireFourSlotsAndOriginalConfiguredMass() {
    const auto r=m::bind_real_s03_s06(facts());
    QCOMPARE(r.failure,m::CoherentStereoBindingFailure::none);
    QVERIFY(r.feature_bindings);
    QCOMPARE((*r.feature_bindings)[0].configured.size(),std::size_t{4});
    QCOMPARE((*r.feature_bindings)[1].configured.size(),std::size_t{4});
    QCOMPARE((*r.feature_bindings)[2].configured.size(),std::size_t{3});
    QCOMPARE((*r.feature_bindings)[3].configured.size(),std::size_t{3});
    QCOMPARE((*r.feature_bindings)[0].configured[0].paired_native_support,0.8);
    QCOMPARE((*r.feature_bindings)[1].configured[0].paired_native_support,0.9);
    QCOMPARE((*r.feature_bindings)[2].configured[0].paired_native_support,1.0);
    QVERIFY(!r.authorizes_m10_or_m13());
}
void S03S06NativeBindingTest::
exactSideEndpointAndNoAccidentalLowEndGate() {
    auto e=facts();
    e.observed[0].multiband[0].width.transformed_z=-24.0;
    e.observed[0].multiband[0].width.actual_zero_side_endpoint=true;
    e.observed[1].multiband[0].width.transformed_z=-24.0;
    e.observed[1].multiband[0].width.actual_zero_side_endpoint=true;
    e.observed[1].low_end_gate_current_and_identical=false;
    const auto r=m::bind_real_s03_s06(e);
    QVERIFY(r.feature_bindings);
    const auto& width=(*r.feature_bindings)[0].configured[0];
    QCOMPARE(width.prepared.endpoint,
             rgsml::app::l3_automatch::m08_frozen::Endpoint::mono_side_muted);
    QVERIFY(width.current_valid);
    for(const auto& low:(*r.feature_bindings)[2].configured)
        QVERIFY(!low.current_valid);
    QVERIFY((*r.feature_bindings)[0].configured[1].current_valid);
}
void S03S06NativeBindingTest::
malformedGridFailsButMissingFeatureKeepsMass() {
    auto e=facts();
    e.observed[0].multiband[1].canonical_index=99;
    QCOMPARE(m::bind_real_s03_s06(e).failure,
             m::CoherentStereoBindingFailure::invalid_original_grid);
    e=facts();
    e.source_phase6_reference_grade[1][1]=false;
    const auto r=m::bind_real_s03_s06(e);
    QVERIFY(r.feature_bindings);
    QCOMPARE((*r.feature_bindings)[1].configured.size(),std::size_t{4});
    QVERIFY(!(*r.feature_bindings)[1].configured[0].current_valid);
    QVERIFY((*r.feature_bindings)[0].configured[0].current_valid);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::S03S06NativeBindingTest)
#include "test_l3_m06_s03_s06_native_binding.moc"
