#include "internal/l3_m06_p04_real_event_descriptors.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest/QTest>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace rgsml::tests {
namespace {
namespace p=rgsml::app::l3_automatch::m05m06_real;
class P04RealDescriptorsTest final:public QObject {
    Q_OBJECT
private slots:
    void realConstantPcmGivesZeroContrastAndZeroCrest();
    void missingEdgeContextNeverDeletesCanonicalDetectorEvent();
};
void P04RealDescriptorsTest::
realConstantPcmGivesZeroContrastAndZeroCrest() {
    const auto fs=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    const auto n=core::FrameCount::create(48000*3);
    auto pcm=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*n.value());
    QVERIFY(pcm);
    auto channel=pcm.value()->mutable_view().channel(0);
    QVERIFY(channel);
    for(auto& v:*channel.value())v=0.2;
    p::P04DetectedNativePopulation events;
    events.reason=p::P04SelectionReason::ready;
    events.canonical_events.push_back({
        300,300*240,2*300*240+960,0.5,50.0});
    const auto observed=p::derive_real_p04_d07_d08_contexts(
        pcm.value()->view(),events,{true,48000});
    QVERIFY(observed.source_valid);
    QCOMPARE(observed.complete_detector_population.size(),std::size_t{1});
    const auto& event=observed.complete_detector_population.front();
    QVERIFY(event.early_minus_sustain_rms_db);
    QVERIFY(event.peak_minus_local_rms_db);
    QVERIFY(std::abs(*event.early_minus_sustain_rms_db)<1e-10);
    QVERIFY(std::abs(*event.peak_minus_local_rms_db)<1e-10);
    QVERIFY(!observed.authorizes_reference_grade());
}
void P04RealDescriptorsTest::
missingEdgeContextNeverDeletesCanonicalDetectorEvent() {
    const auto fs=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    const auto n=core::FrameCount::create(48000);
    auto pcm=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*n.value());
    QVERIFY(pcm);
    p::P04DetectedNativePopulation events;
    events.reason=p::P04SelectionReason::ready;
    events.canonical_events.push_back({2,480,480*2+960,0.2,8.0});
    const auto s=p::derive_real_p04_d07_d08_contexts(
        pcm.value()->view(),events,{true,48000});
    QVERIFY(s.source_valid);
    QCOMPARE(s.complete_detector_population.size(),std::size_t{1});
    QVERIFY(!s.complete_detector_population[0].
        early_minus_sustain_rms_db);
    QVERIFY(!s.complete_detector_population[0].
        peak_minus_local_rms_db);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::P04RealDescriptorsTest)
#include "test_l3_m06_p04_real_event_descriptors.moc"
