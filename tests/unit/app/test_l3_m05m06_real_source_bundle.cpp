#include "internal/l3_m05m06_real_source_bundle.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <QtTest/QTest>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <cmath>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
class NativeProductBundleTest final:public QObject {
    Q_OBJECT
private slots:
    void onePhysicalPcmFeedsActualM19M20AndNativeM06();
    void mismatchedPhysicalOutputCannotBorrowOtherSources();
};
void NativeProductBundleTest::
onePhysicalPcmFeedsActualM19M20AndNativeM06() {
    auto fs=core::SampleRate::create(48000);
    auto format=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    auto fc=core::FrameCount::create(48000);
    auto pcm=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*fc.value());
    QVERIFY(pcm);
    auto l=pcm.value()->mutable_view().channel(0);
    auto r=pcm.value()->mutable_view().channel(1);
    for(std::size_t i=0;i<l.value()->size();++i) {
        const double value=0.2*std::sin(
            2.0*std::numbers::pi*440.0*
            static_cast<double>(i)/48000.0);
        (*l.value())[i]=value;
        (*r.value())[i]=-value*0.8;
    }
    const auto levels=analysis::measure_basic_level_full_resource(
        *pcm.value());
    QVERIFY(levels);
    const auto tonal=analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{0},*fc.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(tonal);
    const auto bundled=real::bundle_actual_product_m05m06(
        pcm.value()->view(),*levels.value(),*tonal.value(),
        {"real-current-test-owner",1,true},{20.0,20000.0},
        1e-12,48000,false);
    QVERIFY(bundled);
    QCOMPARE(bundled->m19_m20_current_readings.scope_frame_count,
             std::int64_t{48000});
    QCOMPARE(bundled->native_d03_d04.complete_native_windows.size(),
             std::size_t{7});
    QCOMPARE(bundled->native_d03_d04.problem,
             real::NativeWindowProblem::none);
    QVERIFY(!bundled->native_d02_lra.is_reference_grade());
    QVERIFY(!bundled->authorizes_m10_or_export());
}
void NativeProductBundleTest::
mismatchedPhysicalOutputCannotBorrowOtherSources() {
    auto fs=core::SampleRate::create(48000);
    auto format=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    auto fc=core::FrameCount::create(48000);
    auto mono=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*fc.value());
    QVERIFY(mono);
    auto c=mono.value()->mutable_view().channel(0);
    for(auto& sample:*c.value())sample=0.1;
    const auto levels=analysis::measure_basic_level_full_resource(
        *mono.value());
    QVERIFY(levels);
    auto tonal=analysis::measure_tonal_stereo_scope(
        mono.value()->view(),core::FrameIndex{0},*fc.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(tonal);
    auto source=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    auto stereo=audio::AudioBuffer::create(
        *source.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*fc.value());
    QVERIFY(stereo);
    QVERIFY(!real::bundle_actual_product_m05m06(
        stereo.value()->view(),*levels.value(),*tonal.value(),
        {"mismatched",1,true},{20.0,20000.0},1e-12,48000,true));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::NativeProductBundleTest)
#include "test_l3_m05m06_real_source_bundle.moc"
