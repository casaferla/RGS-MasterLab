#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest/QTest>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>

namespace rgsml::tests {
namespace {
class CoherentStereoTap final:public analysis::TonalStereoStereoFrameSink {
public:
    std::uint64_t total{};
    double sum_left{},sum_right{},sum_cross{};
    bool malformed{};
    [[nodiscard]] bool on_selected_stereo_frame(
        std::uint64_t ordinal,std::uint32_t fs,std::uint64_t fft,
        std::span<const double> left,std::span<const double> right,
        std::span<const double> real_cross) override {
        if(fs!=48000U||fft!=4096U||left.size()!=2049U||
           left.size()!=right.size()||left.size()!=real_cross.size()||
           ordinal!=total) {
            malformed=true;return false;
        }
        for(std::size_t k=0;k<left.size();++k) {
            const auto l=left[k],r=right[k],c=real_cross[k];
            if(!std::isfinite(l)||!std::isfinite(r)||!std::isfinite(c)||
               l<0.0||r<0.0||std::abs(c)>std::sqrt(l*r)+1e-14) {
                malformed=true;return false;
            }
            sum_left+=l;sum_right+=r;sum_cross+=c;
        }
        ++total;return true;
    }
};
class StereoPsdTapTest final:public QObject {
    Q_OBJECT
private slots:
    void exactSameM20AutoPsdAndCoherentCrossSigns();
    void absentStereoTapPreservesOriginalM20();
};
void StereoPsdTapTest::exactSameM20AutoPsdAndCoherentCrossSigns() {
    auto fs=core::SampleRate::create(48000);
    auto format=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    auto count=core::FrameCount::create(48000);
    auto audio=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*count.value());
    QVERIFY(audio);
    auto l=audio.value()->mutable_view().channel(0);
    auto r=audio.value()->mutable_view().channel(1);
    QVERIFY(l&&r);
    for(std::size_t i=0;i<l.value()->size();++i) {
        const auto v=0.2*std::sin(2.0*std::numbers::pi*700.0*
                                 static_cast<double>(i)/48000.0);
        (*l.value())[i]=v;
        (*r.value())[i]=-v;
    }
    const auto plain=analysis::measure_tonal_stereo_scope(
        audio.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(plain);
    CoherentStereoTap tap;
    analysis::TonalStereoTrustedSupport support;
    support.selected_stereo_frame_sink=&tap;
    const auto measured=analysis::measure_tonal_stereo_scope(
        audio.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,support);
    QVERIFY(measured);
    QVERIFY(!tap.malformed);
    QCOMPARE(tap.total,measured.value()->selected_spectral_windows);
    QVERIFY(tap.sum_left>0.0);
    QVERIFY(tap.sum_right>0.0);
    QVERIFY(std::abs(tap.sum_cross+tap.sum_left)/
            tap.sum_left<1e-10);
    QCOMPARE(measured.value()->mean_linear_psd,
             plain.value()->mean_linear_psd);
    QCOMPARE(measured.value()->selected_spectral_windows,
             plain.value()->selected_spectral_windows);
    for(auto& sample:*r.value())sample=-sample;
    CoherentStereoTap mono_tap;
    support.selected_stereo_frame_sink=&mono_tap;
    const auto mid=analysis::measure_tonal_stereo_scope(
        audio.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,support);
    QVERIFY(mid);
    QVERIFY(std::abs(mono_tap.sum_cross-mono_tap.sum_left)/
            mono_tap.sum_left<1e-10);
}
void StereoPsdTapTest::absentStereoTapPreservesOriginalM20() {
    auto fs=core::SampleRate::create(48000);
    auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    auto frames=core::FrameCount::create(4096);
    auto pcm=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(pcm);
    auto samples=pcm.value()->mutable_view().channel(0);
    QVERIFY(samples);
    for(auto& v:*samples.value())v=0.1;
    const auto untouched=analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{0},*frames.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(untouched);
    CoherentStereoTap tap;
    analysis::TonalStereoTrustedSupport supplied;
    supplied.selected_stereo_frame_sink=&tap;
    QVERIFY(!analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{0},*frames.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,supplied));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::StereoPsdTapTest)
#include "test_l3_m06_m20_coherent_stereo_psd_tap.moc"
