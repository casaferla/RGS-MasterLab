#include "internal/l3_m06_m20_coherent_stereo_band_sink.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
namespace rgsml::tests {
namespace {
namespace native=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] core::Result<audio::AudioBuffer>
stereo_wave(bool opposite) {
    const auto fs=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    const auto n=core::FrameCount::create(48000*3);
    auto b=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*n.value());
    if(b) {
        auto l=b.value()->mutable_view().channel(0);
        auto r=b.value()->mutable_view().channel(1);
        for(std::size_t i=0;i<l.value()->size();++i) {
            const auto t=static_cast<double>(i)/48000.0;
            const double v=0.1*std::sin(2.0*std::numbers::pi*50.0*t)
                          +0.1*std::sin(2.0*std::numbers::pi*440.0*t);
            (*l.value())[i]=v;(*r.value())[i]=opposite?-v:v;
        }
    }
    return b;
}
class NativeStereoBandTest final:public QObject {
    Q_OBJECT
private slots:
    void realPsdInPhaseMonoEndpointNeedsExactEvidence();
    void antiPhaseRhoNegativeAndNeverInventSideSource();
    void lowEndOnlyWhenRetainedGateSameAndAttested();
    void lessThanTwentyFramesNeverGradeMultiband();
    void identicalCountsDoNotConcealDifferentOrdinals();
};
void NativeStereoBandTest::realPsdInPhaseMonoEndpointNeedsExactEvidence(){
    auto pcm=stereo_wave(false);QVERIFY(pcm);
    native::AuthenticM20CoherentStereoSink sink{
        48000,4096,{{0,20,80},{1,80,200},{2,200,1000}},true,true};
    analysis::TonalStereoTrustedSupport support;
    support.selected_stereo_frame_sink=&sink;
    const auto measured=analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{0},
        *core::FrameCount::create(48000*3).value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,support);
    QVERIFY(measured);QVERIFY(sink.valid());
    const std::array<bool,3> exact_mono{true,true,true};
    const auto native_result=sink.finalize(
        48000*3,true,exact_mono,{{true,true,true}});
    QVERIFY(native_result.numerical_source_valid);
    QCOMPARE(native_result.multiband.size(),std::size_t{3});
    QCOMPARE(native_result.native_selected_frames,
             measured.value()->selected_spectral_windows);
    for(const auto& b:native_result.multiband) {
        QVERIFY(b.correlation.raw_ratio_or_rho);
        QVERIFY(std::abs(*b.correlation.raw_ratio_or_rho-1.0)<1e-8);
        QVERIFY(b.width.transformed_z);
        QCOMPARE(*b.width.transformed_z,-24.0);
        QVERIFY(b.width.actual_zero_side_endpoint);
    }
    for(const auto& w:native_result.low_end_width) {
        QVERIFY(w.transformed_z);
        QCOMPARE(*w.transformed_z,-24.0);
    }
    QVERIFY(!native_result.authorizes_m10_or_stereo_safety());
}
void NativeStereoBandTest::
antiPhaseRhoNegativeAndNeverInventSideSource(){
    auto pcm=stereo_wave(true);QVERIFY(pcm);
    native::AuthenticM20CoherentStereoSink sink{
        48000,4096,{{0,20,80},{1,80,200},{2,200,1000}},true,true};
    analysis::TonalStereoTrustedSupport support;
    support.selected_stereo_frame_sink=&sink;
    const auto measured=analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{0},
        *core::FrameCount::create(48000*3).value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,support);
    QVERIFY(measured);
    const auto s=sink.finalize(48000*3,true);
    for(const auto& b:s.multiband) {
        QVERIFY(b.correlation.raw_ratio_or_rho);
        QVERIFY(*b.correlation.raw_ratio_or_rho< -0.999999);
        QVERIFY(!b.width.transformed_z); // Mid energy zero => no width
    }
    for(const auto& low:s.low_end_correlation) {
        QVERIFY(low.raw_ratio_or_rho);
        QVERIFY(*low.raw_ratio_or_rho< -0.999999);
    }
}
void NativeStereoBandTest::
lowEndOnlyWhenRetainedGateSameAndAttested(){
    auto pcm=stereo_wave(false);QVERIFY(pcm);
    native::AuthenticM20CoherentStereoSink sink{
        48000,4096,{{0,20,80}},true,false};
    analysis::TonalStereoTrustedSupport support;
    support.selected_stereo_frame_sink=&sink;
    const auto measured=analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{0},
        *core::FrameCount::create(48000*3).value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,support);
    QVERIFY(measured);
    const auto out=sink.finalize(48000*3,true);
    QVERIFY(!out.low_end_gate_current_and_identical);
    for(const auto& low:out.low_end_width)
        QVERIFY(!low.transformed_z);
    QVERIFY(out.multiband.front().correlation.transformed_z);
}
void NativeStereoBandTest::
lessThanTwentyFramesNeverGradeMultiband(){
    native::AuthenticM20CoherentStereoSink sink{
        48000,4096,{{0,20,80}},true,true};
    const auto p=std::vector<double>(2049,0.001);
    const auto c=std::vector<double>(2049,0.0);
    for(std::uint64_t i=0;i<19U;++i)
        QVERIFY(sink.on_selected_stereo_frame(
            i,48000,4096,p,p,c));
    const auto out=sink.finalize(48000*2,true);
    QCOMPARE(out.multiband.front().correlation.valid_frames,
             std::uint64_t{19});
    QVERIFY(!out.multiband.front().correlation.transformed_z);
}
void NativeStereoBandTest::identicalCountsDoNotConcealDifferentOrdinals()
{
    native::AuthenticM20CoherentStereoSink first{
        48000,4096,{{0,20,80}},true,true};
    native::AuthenticM20CoherentStereoSink second{
        48000,4096,{{0,20,80}},true,true};
    const std::vector<double> p(2049,0.001),c(2049,0.0);
    for(std::uint64_t i=0;i<25U;++i) {
        QVERIFY(first.on_selected_stereo_frame(i,48000,4096,p,p,c));
        QVERIFY(second.on_selected_stereo_frame(i+1U,48000,4096,p,p,c));
    }
    const auto a=first.finalize(48000*2,true);
    const auto b=second.finalize(48000*2,true);
    const auto repeated=first.finalize(48000*2,true);
    const auto& x=a.multiband.front();
    const auto& y=b.multiband.front();
    QCOMPARE(x.width.valid_frames,y.width.valid_frames);
    QCOMPARE(x.width.raw_ratio_or_rho,y.width.raw_ratio_or_rho);
    QVERIFY(!x.width.exact_frame_population_identity.empty());
    QVERIFY(x.width.exact_frame_population_identity!=y.width.exact_frame_population_identity);
    QVERIFY(x.correlation.exact_frame_population_identity!=y.correlation.exact_frame_population_identity);
    QCOMPARE(x.width.exact_frame_population_identity,
             repeated.multiband.front().width.exact_frame_population_identity);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::NativeStereoBandTest)
#include "test_l3_m06_native_coherent_stereo_band_sink.moc"
