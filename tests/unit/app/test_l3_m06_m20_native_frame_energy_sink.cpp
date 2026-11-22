#include "internal/l3_m06_m20_native_frame_energy_sink.hpp"
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest/QTest>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
class M20SelectedFrameSinkTest final:public QObject{
    Q_OBJECT
private slots:
    void sameOriginalFftAndPsdWithAndWithoutNativeFrameExtraction();
    void badSourceGridAndStalePolicyFailClosed();
};
void M20SelectedFrameSinkTest::
sameOriginalFftAndPsdWithAndWithoutNativeFrameExtraction(){
    const auto fs=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    const auto fc=core::FrameCount::create(48000*3);
    auto buffer=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*fc.value());
    QVERIFY(buffer);
    auto mono=buffer.value()->mutable_view().channel(0);
    QVERIFY(mono);
    for(std::size_t i=0;i<mono.value()->size();++i)
        (*mono.value())[i]=0.2*std::sin(
            2.0*std::numbers::pi*600.0*
            static_cast<double>(i)/48000.0);
    const auto before=analysis::measure_tonal_stereo_scope(
        buffer.value()->view(),core::FrameIndex{0},*fc.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(before);
    real::AuthenticM20FrameEnergySink sink{
        48000U,4096U,{{0,100.0,400.0},{1,400.0,800.0},
                     {2,800.0,1600.0}},true};
    analysis::TonalStereoTrustedSupport support;
    support.selected_native_frame_sink=&sink;
    const auto after=analysis::measure_tonal_stereo_scope(
        buffer.value()->view(),core::FrameIndex{0},*fc.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,support);
    QVERIFY(after);
    QVERIFY(sink.valid());
    QCOMPARE(after.value()->selected_spectral_windows,
             before.value()->selected_spectral_windows);
    QCOMPARE(after.value()->mean_linear_psd.size(),
             before.value()->mean_linear_psd.size());
    for(std::size_t i=0;i<before.value()->mean_linear_psd.size();++i)
        QCOMPARE(after.value()->mean_linear_psd[i],
                 before.value()->mean_linear_psd[i]);
    QCOMPARE(sink.exact_e_tb().size(),
             static_cast<std::size_t>(after.value()->selected_spectral_windows));
    QCOMPARE(sink.canonical_original_ordinals().size(),
             sink.exact_e_tb().size());
    for(const auto& frame:sink.exact_e_tb()) {
        QCOMPARE(frame.size(),std::size_t{3});
        for(const auto& e:frame) {
            QVERIFY(e);
            QVERIFY(*e>=0.0);
        }
    }
    QVERIFY(!sink.authorizes_m10());
}
void M20SelectedFrameSinkTest::badSourceGridAndStalePolicyFailClosed(){
    real::AuthenticM20FrameEnergySink stale{
        48000U,4096U,{{0,100.0,400.0}},false};
    QVERIFY(!stale.valid());
    real::AuthenticM20FrameEnergySink overlapping{
        48000U,4096U,{{0,100.0,400.0},{1,399.0,800.0}},true};
    QVERIFY(!overlapping.valid());
    real::AuthenticM20FrameEnergySink good{
        48000U,4096U,{{0,100.0,400.0}},true};
    const std::vector<double> fake(2049,0.1);
    QVERIFY(!good.on_selected_native_frame(0,44100,4096,fake));
    QVERIFY(!good.valid());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M20SelectedFrameSinkTest)
#include "test_l3_m06_m20_native_frame_energy_sink.moc"
