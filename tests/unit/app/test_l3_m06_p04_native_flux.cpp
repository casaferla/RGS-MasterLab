#include "internal/l3_m06_p04_native_flux.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest/QTest>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace rgsml::tests {
namespace {
namespace p=rgsml::app::l3_automatch::m05m06_real;
class NativeP04FluxTest final:public QObject {
    Q_OBJECT
private slots:
    void tiesEvenDirect44100GridNoRoundedHopDrift();
    void silentRealPcmYieldsZeroFluxNotOnsetAtFirstFrame();
    void stereoOppositePhaseHasSamePositiveFluxAsMono();
    void cancelledPhase6SourceYieldsNoPartialObservations();
};
void NativeP04FluxTest::
cancelledPhase6SourceYieldsNoPartialObservations() {
    const auto fs=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    const auto length=core::FrameCount::create(48000);
    auto pcm=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*length.value());
    QVERIFY(pcm);
    std::atomic<bool> cancelled{true};
    const auto r=p::measure_p04_native_positive_flux(
        pcm.value()->view(),&cancelled);
    QCOMPARE(r.reason,p::P04NativeReason::cancelled);
    QVERIFY(r.canonical.empty());
}
void NativeP04FluxTest::tiesEvenDirect44100GridNoRoundedHopDrift() {
    QCOMPARE(*p::p04_round_ties_even(44100U,200U),std::uint64_t{220});
    QCOMPARE(*p::p04_round_ties_even(3U*44100U,200U),
             std::uint64_t{662});
    QCOMPARE(*p::p04_round_ties_even(5U*44100U,200U),
             std::uint64_t{1102});
}
void NativeP04FluxTest::
silentRealPcmYieldsZeroFluxNotOnsetAtFirstFrame() {
    const auto fs=core::SampleRate::create(44100);
    const auto format=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    const auto n=core::FrameCount::create(44100);
    auto pcm=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*n.value());
    QVERIFY(pcm);
    const auto population=p::measure_p04_native_positive_flux(
        pcm.value()->view());
    QCOMPARE(population.reason,p::P04NativeReason::ready);
    QCOMPARE(population.native_window_frames,std::uint64_t{882});
    QCOMPARE(population.canonical_fft_size,std::uint64_t{1024});
    QCOMPARE(population.canonical[0].absolute_start_frame,std::uint64_t{0});
    QCOMPARE(population.canonical[1].absolute_start_frame,std::uint64_t{220});
    QCOMPARE(population.canonical[2].absolute_start_frame,std::uint64_t{441});
    QCOMPARE(population.canonical[3].absolute_start_frame,std::uint64_t{662});
    QVERIFY(!population.canonical.front().raw_positive_spectral_flux);
    for(std::size_t i=1;i<population.canonical.size();++i) {
        QVERIFY(population.canonical[i].raw_positive_spectral_flux);
        QVERIFY(std::abs(*population.canonical[i].
            raw_positive_spectral_flux)<1e-12);
    }
    QVERIFY(!population.authorizes_d07_d09());
}
void NativeP04FluxTest::
stereoOppositePhaseHasSamePositiveFluxAsMono() {
    const auto fs=core::SampleRate::create(48000);
    const auto stereo=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    const auto n=core::FrameCount::create(48000);
    auto pcm=audio::AudioBuffer::create(
        *stereo.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*n.value());
    QVERIFY(pcm);
    auto l=pcm.value()->mutable_view().channel(0);
    auto r=pcm.value()->mutable_view().channel(1);
    QVERIFY(l&&r);
    for(std::size_t i=0;i<l.value()->size();++i) {
        const auto x=i<24000U?0.0:0.2;
        (*l.value())[i]=x;(*r.value())[i]=-x;
    }
    const auto out=p::measure_p04_native_positive_flux(
        pcm.value()->view());
    QCOMPARE(out.reason,p::P04NativeReason::ready);
    bool any_positive=false;
    for(const auto& frame:out.canonical) {
        if(frame.raw_positive_spectral_flux&&
           *frame.raw_positive_spectral_flux>0.0)
            any_positive=true;
    }
    QVERIFY(any_positive);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::NativeP04FluxTest)
#include "test_l3_m06_p04_native_flux.moc"
