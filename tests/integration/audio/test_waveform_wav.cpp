#include <rgsml/audio/waveform_summary.hpp>
#include <rgsml/audio/wav_reader.hpp>

#include "../../unit/audio/wav_test_support.hpp"

#include <QTest>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace rgsml::tests {

class WaveformWavIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptedFormatsAndContainers();
    void nonFiniteAndTruncatedInputFail();
};

void WaveformWavIntegrationTest::acceptedFormatsAndContainers()
{
    using namespace wav_support;
    struct Case final {
        std::uint16_t tag;
        std::uint16_t bits;
        bool rf64;
        Bytes payload;
    };
    const std::array<Case, 6> cases{{
        {1U, 16U, false, pcm_payload(std::array<std::int64_t, 4>{-32768, 32767, 0, 1}, 16U)},
        {1U, 24U, false, pcm_payload(std::array<std::int64_t, 4>{-8'388'608, 8'388'607, 0, 1}, 24U)},
        {1U, 32U, false, pcm_payload(std::array<std::int64_t, 4>{-2'147'483'648LL, 2'147'483'647LL, 0, 1}, 32U)},
        {3U, 32U, false, f32_payload(std::array<std::uint32_t, 4>{0xbf000000U, 0x3f000000U, 0U, 1U})},
        {3U, 64U, false, f64_payload(std::array<std::uint64_t, 4>{0xbfe0000000000000ULL, 0x3fe0000000000000ULL, 0U, 1U})},
        {3U, 64U, true, f64_payload(std::array<std::uint64_t, 4>{0x8000000000000000ULL, 0U, 1U, 0x3ff4000000000000ULL})},
    }};

    for (const auto& item : cases) {
        const auto bytes = make_wav(
            item.tag, item.bits, 2U, 48'000U, item.payload, false, item.rf64);
        auto control = std::make_shared<ReaderControl>();
        auto reader = audio::WavReader::open(memory_reader(bytes, control));
        QVERIFY(reader);
        auto summary = audio::build_waveform_summary(**reader.value());
        QVERIFY(summary);
        QCOMPARE(summary.value()->channel_count(), std::size_t{2});
        QCOMPARE(summary.value()->source_frame_count().value(), std::int64_t{2});
        QVERIFY(control->largestReadRequest <= 2'048U);
        QVERIFY((*reader.value())->close());
        QCOMPARE(control->closeCalls, std::size_t{1});
    }
}

void WaveformWavIntegrationTest::nonFiniteAndTruncatedInputFail()
{
    using namespace wav_support;
    const auto nonFiniteBytes = make_wav(
        3U,
        64U,
        1U,
        48'000U,
        f64_payload(std::array<std::uint64_t, 1>{0x7ff8000000000000ULL}));
    auto nonFiniteReader = audio::WavReader::open(
        memory_reader(nonFiniteBytes, std::make_shared<ReaderControl>()));
    QVERIFY(nonFiniteReader);
    auto nonFinite = audio::build_waveform_summary(**nonFiniteReader.value());
    QVERIFY(!nonFinite);
    QCOMPARE(nonFinite.error()->code(), core::ErrorCode::InvalidAudioSample);

    auto truncatedBytes = make_wav(
        1U,
        16U,
        1U,
        48'000U,
        pcm_payload(std::array<std::int64_t, 4>{0, 1, 2, 3}, 16U));
    truncatedBytes.pop_back();
    auto truncatedReader = audio::WavReader::open(
        memory_reader(truncatedBytes, std::make_shared<ReaderControl>()));
    QVERIFY(!truncatedReader);
    QCOMPARE(truncatedReader.error()->code(), core::ErrorCode::TruncatedAudioData);
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::WaveformWavIntegrationTest)

#include "test_waveform_wav.moc"
