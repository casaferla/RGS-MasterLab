#include "internal/l3_m06_p04_native_attack_rise.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest/QTest>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace rgsml::tests {
namespace {
namespace p=rgsml::app::l3_automatch::m05m06_real;
class NativeP04AttackRiseTest final:public QObject {
    Q_OBJECT
private slots:
    void realCausalOneMsEnvelopeYieldsPositiveAttackTime();
    void constantEnvelopeAndMissingHistoryKeepD09Null();
};
void NativeP04AttackRiseTest::
realCausalOneMsEnvelopeYieldsPositiveAttackTime() {
    auto fs=core::SampleRate::create(48000);
    auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    auto fc=core::FrameCount::create(48000*3);
    auto pcm=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*fc.value());
    QVERIFY(pcm);
    auto ch=pcm.value()->mutable_view().channel(0);
    QVERIFY(ch);
    constexpr std::int64_t center=300*240+480;
    constexpr std::int64_t ramp0=center-480;
    constexpr std::int64_t ramp1=center+1440;
    for(std::size_t i=0;i<ch.value()->size();++i) {
        const auto n=static_cast<std::int64_t>(i);
        (*ch.value())[i]=n<ramp0?0.0:
            n>=ramp1?0.5:
            0.5*static_cast<double>(n-ramp0)/
            static_cast<double>(ramp1-ramp0);
    }
    p::P04DetectedNativePopulation detector;
    detector.reason=p::P04SelectionReason::ready;
    detector.canonical_events.push_back({
        300,300*240,2*300*240+960,0.8,50.0});
    const auto ctx=p::derive_real_p04_d07_d08_contexts(
        pcm.value()->view(),detector,{true,48000});
    QVERIFY(ctx.source_valid);
    const auto result=p::derive_real_p04_d09_attack_rise(
        pcm.value()->view(),detector,ctx,true);
    QVERIFY(result.exact_source_and_canonical_selection_current);
    QCOMPARE(result.same_detector_event_population.size(),
             std::size_t{1});
    const auto rise=result.same_detector_event_population[0].
        attack_rise_time_ms;
    QVERIFY(rise);
    QVERIFY(*rise>0.0);
    QVERIFY(*rise<40.0);
    QVERIFY(!result.authorizes_solver());
}
void NativeP04AttackRiseTest::
constantEnvelopeAndMissingHistoryKeepD09Null() {
    auto fs=core::SampleRate::create(48000);
    auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    auto fc=core::FrameCount::create(48000);
    auto pcm=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*fc.value());
    QVERIFY(pcm);
    auto channel=pcm.value()->mutable_view().channel(0);
    QVERIFY(channel);
    for(auto& v:*channel.value())v=0.5;
    p::P04DetectedNativePopulation detector;
    detector.reason=p::P04SelectionReason::ready;
    detector.canonical_events.push_back({
        300,300*240,2*300*240+960,0.8,50.0});
    const auto ctx=p::derive_real_p04_d07_d08_contexts(
        pcm.value()->view(),detector,{true,48000});
    const auto flat=p::derive_real_p04_d09_attack_rise(
        pcm.value()->view(),detector,ctx,true);
    QVERIFY(flat.exact_source_and_canonical_selection_current);
    QVERIFY(!flat.same_detector_event_population[0].
        attack_rise_time_ms);
    detector.canonical_events.clear();
    detector.canonical_events.push_back({2,480,480*2+960,0.7,50.0});
    const auto edge=p::derive_real_p04_d07_d08_contexts(
        pcm.value()->view(),detector,{true,48000});
    const auto missing=p::derive_real_p04_d09_attack_rise(
        pcm.value()->view(),detector,edge,true);
    QCOMPARE(missing.same_detector_event_population.size(),
             std::size_t{1});
    QVERIFY(!missing.same_detector_event_population[0].
        attack_rise_time_ms);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::NativeP04AttackRiseTest)
#include "test_l3_m06_p04_native_attack_rise.moc"
