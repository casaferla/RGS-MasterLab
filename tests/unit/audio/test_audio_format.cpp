#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>

#include <QtTest/QTest>

#include <array>
#include <cstdint>

namespace rgsml::tests {
namespace {

using namespace rgsml::audio;
using namespace rgsml::core;

class AudioFormatTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptedRatesAndLayouts();
    void invalidValues();
    void exactTimebase();
};

void AudioFormatTest::acceptedRatesAndLayouts()
{
    constexpr std::array<std::int64_t, 4> rates{44100, 48000, 96000, 192000};
    for (const auto rateValue : rates) {
        auto rate = SampleRate::create(rateValue);
        QVERIFY(rate.value() != nullptr);
        auto mono = AudioFormat::create(*rate.value(), ChannelLayout::MONO_C);
        auto stereo = AudioFormat::create(*rate.value(), ChannelLayout::STEREO_LR);
        QVERIFY(mono.value() != nullptr);
        QVERIFY(stereo.value() != nullptr);
        QCOMPARE(mono.value()->sample_rate().value(), rateValue);
        QCOMPARE(mono.value()->channel_count(), std::size_t{1});
        QCOMPARE(*mono.value()->channel_at(0).value(), AudioChannel::C);
        QCOMPARE(stereo.value()->channel_count(), std::size_t{2});
        QCOMPARE(*stereo.value()->channel_at(0).value(), AudioChannel::L);
        QCOMPARE(*stereo.value()->channel_at(1).value(), AudioChannel::R);
    }
}

void AudioFormatTest::invalidValues()
{
    auto zeroRate = SampleRate::create(0);
    QVERIFY(zeroRate.error() != nullptr);
    QCOMPARE(zeroRate.error()->code(), ErrorCode::InvalidArgument);

    auto rate = SampleRate::create(48000);
    QVERIFY(rate.value() != nullptr);
    auto invalidLayout = AudioFormat::create(
        *rate.value(), static_cast<ChannelLayout>(99));
    QVERIFY(invalidLayout.error() != nullptr);
    QCOMPARE(invalidLayout.error()->code(), ErrorCode::UnsupportedAudioLayout);

    auto format = AudioFormat::create(*rate.value(), ChannelLayout::MONO_C);
    QVERIFY(format.value() != nullptr);
    auto invalidChannel = format.value()->channel_at(1);
    QVERIFY(invalidChannel.error() != nullptr);
    QCOMPARE(invalidChannel.error()->code(), ErrorCode::OutOfRange);

    auto invalidDomain = AudioTimebase::create(
        *rate.value(), static_cast<FrameDomainId>(99));
    QVERIFY(invalidDomain.error() != nullptr);
    QCOMPARE(invalidDomain.error()->code(), ErrorCode::InvalidArgument);
}

void AudioFormatTest::exactTimebase()
{
    auto rate = SampleRate::create(48000);
    QVERIFY(rate.value() != nullptr);
    auto sourceTimebase = AudioTimebase::create(
        *rate.value(), FrameDomainId::SOURCE_PROCESSING_RATE);
    auto outputTimebase = AudioTimebase::create(
        *rate.value(), FrameDomainId::OUTPUT_RATE);
    QVERIFY(sourceTimebase.value() != nullptr);
    QVERIFY(outputTimebase.value() != nullptr);
    QCOMPARE(
        sourceTimebase.value()->frame_domain_id(),
        FrameDomainId::SOURCE_PROCESSING_RATE);
    QCOMPARE(outputTimebase.value()->frame_domain_id(), FrameDomainId::OUTPUT_RATE);

    auto exactTime = frame_to_time(FrameIndex{24000}, *rate.value());
    QVERIFY(exactTime.value() != nullptr);
    QCOMPARE(exactTime.value()->seconds().numerator(), std::int64_t{1});
    QCOMPARE(exactTime.value()->seconds().denominator(), std::int64_t{2});
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::AudioFormatTest)

#include "test_audio_format.moc"
