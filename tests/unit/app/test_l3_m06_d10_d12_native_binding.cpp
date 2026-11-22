#include "internal/l3_m06_d10_d12_native_binding.hpp"
#include <QtTest/QTest>
#include <cstdint>
#include <string>
#include <vector>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] real::SpectralTripleAuthority sample() {
    real::SpectralTripleAuthority a;
    a.exact_current_full_stft_grid_and_policy_attested=true;
    for(std::size_t d=0;d<3U;++d) {
        a.source_reference_grade[d]={true,true,true};
        a.canonical_feature_dependency[d]=
            "exact-canonical-energy-digest-"+std::to_string(d);
    }
    const std::array<std::string,3> names{"PREP","GOLD","CAND"};
    for(std::size_t j=0;j<3U;++j) {
        a.source_owner[j]={names[j],static_cast<std::uint64_t>(j+1),true};
        auto& s=a.observed[j];
        s.source_reason=real::SpectralNativeReason::ready;
        s.candidate_active_frames=40;
        for(std::size_t i=0;i<4U;++i) {
            real::SpectralBandD10D12 band;
            for(std::size_t d=0;d<3U;++d) {
                band.d10_d11_d12_z[d]=static_cast<double>(i+d+j);
                band.valid_native_counts[d]=40;
                band.native_support[d]=j==0?0.75:j==1?0.8:1.0;
                band.reason[d]=real::SpectralNativeReason::ready;
            }
            s.canonical_band_results.push_back(band);
        }
    }
    return a;
}
class SpectralNativeBindingTest final:public QObject {
    Q_OBJECT
private slots:
    void allOriginalBandsAndThreeBindingIdentities();
    void missingSingleBandKeepsFullMass();
    void sourceRevisionOrGeometryFailsWholeGeneration();
};
void SpectralNativeBindingTest::
allOriginalBandsAndThreeBindingIdentities() {
    const auto mapped=real::bind_real_d10_d12(sample());
    QCOMPARE(mapped.failure,real::SpectralSlotFailure::none);
    QVERIFY(mapped.feature_bindings);
    for(std::size_t d=0;d<3U;++d) {
        const auto& slot=(*mapped.feature_bindings)[d];
        QCOMPARE(slot.configured.size(),std::size_t{4});
        for(const auto& b:slot.configured) {
            QCOMPARE(b.paired_native_support,0.75);
            QVERIFY(b.current_valid);
            QVERIFY(b.current_candidate_z);
        }
    }
    QVERIFY(!mapped.authorizes_product_publication());
}
void SpectralNativeBindingTest::
missingSingleBandKeepsFullMass() {
    auto s=sample();
    s.observed[1].canonical_band_results[1].reason[2]=
        real::SpectralNativeReason::undefined_temporal_variance;
    const auto r=real::bind_real_d10_d12(s);
    QVERIFY(r.feature_bindings);
    QCOMPARE((*r.feature_bindings)[2].configured.size(),
             std::size_t{4});
    QVERIFY(!(*r.feature_bindings)[2].configured[1].current_valid);
    QCOMPARE((*r.feature_bindings)[2].configured[1]
                 .paired_native_support,0.0);
    QVERIFY((*r.feature_bindings)[2].configured[0].current_valid);
}
void SpectralNativeBindingTest::
sourceRevisionOrGeometryFailsWholeGeneration() {
    auto s=sample();
    s.exact_current_full_stft_grid_and_policy_attested=false;
    QVERIFY(!real::bind_real_d10_d12(s).feature_bindings);
    s=sample();
    s.observed[2].canonical_band_results.pop_back();
    QCOMPARE(real::bind_real_d10_d12(s).failure,
             real::SpectralSlotFailure::unbound_or_stale_stft);
    s=sample();
    s.canonical_feature_dependency[0].clear();
    QCOMPARE(real::bind_real_d10_d12(s).failure,
             real::SpectralSlotFailure::
                 missing_feature_definition_or_revision);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::SpectralNativeBindingTest)
#include "test_l3_m06_d10_d12_native_binding.moc"
