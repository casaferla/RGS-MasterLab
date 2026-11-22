#include "internal/l3_m06_native_spectral_d10_d12.hpp"
#include <QtTest/QTest>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] std::vector<std::vector<std::optional<double>>>
native(std::size_t frames,bool use_missing=false) {
    std::vector<std::vector<std::optional<double>>> e;
    for(std::size_t i=0;i<frames;++i) {
        const double t=static_cast<double>(i);
        const double x=0.1+0.003*t;
        const double y=0.3+0.002*t+0.01*(i%3U);
        const double z=0.2+0.005*t;
        e.push_back({x,y,z});
    }
    if(use_missing)e[0][1].reset();
    return e;
}
class SpectralNativeD10D12Test final:public QObject {
    Q_OBJECT
private slots:
    void actualAlignedTemporalEnergyVsSortedQuantileDifference();
    void constantBroadbandInvalidatesOnlyCorrelation();
    void missingMemberCannotReshapeCommonBandwidth();
    void missingGridOrInsufficientSupportNeverClaimsReady();
};
void SpectralNativeD10D12Test::
actualAlignedTemporalEnergyVsSortedQuantileDifference() {
    const auto s=real::reduce_native_spectral_d10_d12(
        native(50),3,true,48000*3,48000);
    QCOMPARE(s.source_reason,real::SpectralNativeReason::ready);
    QCOMPARE(s.canonical_band_results.size(),std::size_t{3});
    for(const auto& b:s.canonical_band_results) {
        for(const auto reason:b.reason)
            QCOMPARE(reason,real::SpectralNativeReason::ready);
        QCOMPARE(b.valid_native_counts[0],std::uint64_t{50});
        QCOMPARE(b.native_support[0],1.0);
        QVERIFY(b.d10_d11_d12_z[0]);
        QVERIFY(b.d10_d11_d12_z[1]);
        QVERIFY(b.d10_d11_d12_z[2]);
    }
    QVERIFY(!s.authorizes_m10());
}
void SpectralNativeD10D12Test::
constantBroadbandInvalidatesOnlyCorrelation() {
    std::vector<std::vector<std::optional<double>>> e;
    for(std::size_t i=0;i<40U;++i) {
        const double x=0.1+0.002*static_cast<double>(i);
        e.push_back({x,1.0-x});
    }
    const auto s=real::reduce_native_spectral_d10_d12(
        e,2,true,48000*3,48000);
    for(const auto& b:s.canonical_band_results) {
        QCOMPARE(b.reason[0],real::SpectralNativeReason::ready);
        QCOMPARE(b.reason[1],real::SpectralNativeReason::ready);
        QCOMPARE(b.reason[2],
                 real::SpectralNativeReason::undefined_temporal_variance);
    }
}
void SpectralNativeD10D12Test::
missingMemberCannotReshapeCommonBandwidth() {
    const auto s=real::reduce_native_spectral_d10_d12(
        native(40,true),3,true,48000*3,48000);
    QCOMPARE(s.source_reason,real::SpectralNativeReason::ready);
    for(const auto& b:s.canonical_band_results) {
        QCOMPARE(b.valid_native_counts[0],std::uint64_t{39});
        QCOMPARE(b.native_support[0],39.0/40.0);
    }
}
void SpectralNativeD10D12Test::
missingGridOrInsufficientSupportNeverClaimsReady() {
    auto empty=real::reduce_native_spectral_d10_d12(
        native(40),3,false,48000*3,48000);
    QCOMPARE(empty.source_reason,
             real::SpectralNativeReason::missing_exact_stft_or_grid);
    auto too_few=real::reduce_native_spectral_d10_d12(
        native(19),3,true,48000*3,48000);
    for(const auto& b:too_few.canonical_band_results)
        QCOMPARE(b.reason[0],
                 real::SpectralNativeReason::insufficient_band_frame_support);
    auto short_scope=real::reduce_native_spectral_d10_d12(
        native(40),3,true,48000,48000);
    QCOMPARE(short_scope.source_reason,
             real::SpectralNativeReason::insufficient_band_frame_support);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::SpectralNativeD10D12Test)
#include "test_l3_m06_native_spectral_d10_d12.moc"
