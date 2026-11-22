#include "internal/l3_m06_p04_real_product_observation.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest/QTest>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
class P04RealProductObservationTest final:public QObject {
    Q_OBJECT
private slots:
    void actualPCMRetainsSingleCanonicalEventPopulationAndNullMasks();
    void missingEpochAndActiveScopeCannotClaimReferenceGrade();
};
void P04RealProductObservationTest::
actualPCMRetainsSingleCanonicalEventPopulationAndNullMasks() {
    const auto fs=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    const auto fc=core::FrameCount::create(48000);
    auto buffer=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*fc.value());
    QVERIFY(buffer);
    auto data=buffer.value()->mutable_view().channel(0);
    QVERIFY(data);
    for(auto& v:*data.value())v=0.0;
    const auto measured=real::measure_product_p04_d07_d09(
        buffer.value()->view(),48000,true);
    QCOMPARE(measured.reason,real::P04ProductObservationReason::ready);
    QVERIFY(measured.actual_flux.canonical.size()>10U);
    QCOMPARE(measured.actual_flux.canonical.size(),
             measured.detector.canonical_flux_valid_frames+1U);
    QVERIFY(measured.detector.canonical_events.empty());
    QVERIFY(measured.actual_d07_d08_d09_events.
        same_detector_event_population.empty());
    for(const auto& d:measured.original_d07_d08_d09_type7.
            d07_d08_d09) {
        QCOMPARE(d.canonical_events,std::uint64_t{0});
        QVERIFY(!d.reference_grade());
    }
    QVERIFY(!measured.authorizes_m10_or_publish());
}
void P04RealProductObservationTest::
missingEpochAndActiveScopeCannotClaimReferenceGrade() {
    const auto fs=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::MONO_C);
    const auto fc=core::FrameCount::create(4096);
    auto buffer=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*fc.value());
    QVERIFY(buffer);
    const auto result=real::measure_product_p04_d07_d09(
        buffer.value()->view(),4096,false);
    QCOMPARE(result.reason,
             real::P04ProductObservationReason::
                 missing_current_phase6_owner);
    const auto excessive=real::measure_product_p04_d07_d09(
        buffer.value()->view(),100000,true);
    QCOMPARE(excessive.reason,
             real::P04ProductObservationReason::
                 missing_current_phase6_owner);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::P04RealProductObservationTest)
#include "test_l3_m06_p04_real_product_observation.moc"
