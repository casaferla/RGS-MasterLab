#include <rgsml/audio/waveform_summary.hpp>
#include <rgsml/audio/wav_reader.hpp>

#include "internal/waveform_summary_builder.hpp"
#include "wav_test_support.hpp"

#include <QTest>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace wav_support;

[[nodiscard]] std::unique_ptr<audio::WavReader> open_f64(
    const std::vector<std::uint64_t>& interleaved,
    std::uint16_t channels = 1U)
{
    const auto bytes = make_wav(
        3U,
        64U,
        channels,
        48'000U,
        f64_payload(interleaved));
    auto opened = audio::WavReader::open(
        memory_reader(bytes, std::make_shared<ReaderControl>()));
    Q_ASSERT(opened);
    return std::move(*opened.value());
}

[[nodiscard]] std::uint64_t bits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

void compare_summaries(
    const audio::WaveformSummary& left,
    const audio::WaveformSummary& right)
{
    QCOMPARE(left.sample_rate(), right.sample_rate());
    QCOMPARE(left.channel_count(), right.channel_count());
    QCOMPARE(left.source_frame_count(), right.source_frame_count());
    QCOMPARE(left.level_count(), right.level_count());
    QCOMPARE(left.payload_bytes(), right.payload_bytes());
    for (std::size_t levelIndex = 0; levelIndex < left.level_count(); ++levelIndex) {
        const auto leftLevel = left.level(levelIndex);
        const auto rightLevel = right.level(levelIndex);
        QVERIFY(leftLevel);
        QVERIFY(rightLevel);
        QCOMPARE(
            leftLevel.value()->frames_per_bucket(),
            rightLevel.value()->frames_per_bucket());
        QCOMPARE(
            leftLevel.value()->bucket_count(),
            rightLevel.value()->bucket_count());
        for (std::size_t channel = 0; channel < left.channel_count(); ++channel) {
            for (std::int64_t bucket = 0;
                 bucket < leftLevel.value()->bucket_count().value();
                 ++bucket) {
                const auto leftPeak = leftLevel.value()->peak(
                    channel, core::FrameIndex{bucket});
                const auto rightPeak = rightLevel.value()->peak(
                    channel, core::FrameIndex{bucket});
                QVERIFY(leftPeak);
                QVERIFY(rightPeak);
                QCOMPARE(bits(leftPeak.value()->minimum), bits(rightPeak.value()->minimum));
                QCOMPARE(bits(leftPeak.value()->maximum), bits(rightPeak.value()->maximum));
            }
        }
    }
}

}  // namespace

class WaveformSummaryTest final : public QObject {
    Q_OBJECT

private slots:
    void emptySourceHasNoSyntheticLevel();
    void constantsAndSmallPyramid();
    void signedZeroSubnormalAndOverrangeAreExact();
    void decodePartitionsAreBitIdentical();
    void cancellationAndBoundsAreDeterministic();
};

void WaveformSummaryTest::emptySourceHasNoSyntheticLevel()
{
    using namespace wav_support;
    const auto bytes = make_wav(1U, 16U, 1U, 44'100U, Bytes{});
    auto reader = audio::WavReader::open(
        memory_reader(bytes, std::make_shared<ReaderControl>()));
    QVERIFY(reader);
    auto result = audio::build_waveform_summary(**reader.value());
    QVERIFY(result);
    QCOMPARE(result.value()->source_frame_count().value(), std::int64_t{0});
    QCOMPARE(result.value()->level_count(), std::size_t{0});
    QCOMPARE(result.value()->payload_bytes(), std::size_t{0});
    QVERIFY(!result.value()->level(0U));
}

void WaveformSummaryTest::constantsAndSmallPyramid()
{
    QCOMPARE(
        audio::kWaveformSummaryAlgorithmId,
        std::string_view{"rgsml.waveform.summary.minmax-pyramid"});
    QCOMPARE(audio::kWaveformSummaryAlgorithmVersion, std::string_view{"1.0.0"});
    QCOMPARE(audio::WaveformSummary::kMaximumBaseBucketsPerChannel, std::size_t{65'536});
    QCOMPARE(audio::WaveformSummary::kMaximumUiRangesPerChannel, std::size_t{4'096});
    QCOMPARE(audio::WaveformSummary::kMaximumDecodeBlockFrames, std::size_t{4'096});

    const std::vector<std::uint64_t> samples{
        bits(-0.75), bits(0.25), bits(-0.5), bits(1.0), bits(0.125)};
    auto reader = open_f64(samples);
    auto result = audio::build_waveform_summary(*reader);
    QVERIFY(result);
    const auto& summary = *result.value();
    QCOMPARE(summary.sample_rate().value(), std::int64_t{48'000});
    QCOMPARE(summary.channel_count(), std::size_t{1});
    QCOMPARE(summary.source_frame_count().value(), std::int64_t{5});
    QCOMPARE(summary.level_count(), std::size_t{4});

    const auto base = summary.level(0U);
    QVERIFY(base);
    QCOMPARE(base.value()->frames_per_bucket().value(), std::int64_t{1});
    QCOMPARE(base.value()->bucket_count().value(), std::int64_t{5});
    const auto last = base.value()->peak(0U, core::FrameIndex{4});
    QVERIFY(last);
    QCOMPARE(bits(last.value()->minimum), bits(0.125));
    QCOMPARE(bits(last.value()->maximum), bits(0.125));

    const auto top = summary.level(summary.level_count() - 1U);
    QVERIFY(top);
    QCOMPARE(top.value()->bucket_count().value(), std::int64_t{1});
    const auto global = top.value()->peak(0U, core::FrameIndex{0});
    QVERIFY(global);
    QCOMPARE(bits(global.value()->minimum), bits(-0.75));
    QCOMPARE(bits(global.value()->maximum), bits(1.0));
    QVERIFY(!top.value()->peak(1U, core::FrameIndex{0}));
    QVERIFY(!top.value()->peak(0U, core::FrameIndex{1}));
}

void WaveformSummaryTest::signedZeroSubnormalAndOverrangeAreExact()
{
    std::vector<std::uint64_t> interleaved(65'537U * 2U, bits(0.0));
    interleaved[0] = bits(0.0);
    interleaved[2] = bits(-0.0);
    interleaved[1] = std::uint64_t{1U};
    interleaved[3] = bits(1.25);
    interleaved[4] = bits(-1.5);
    interleaved[5] = bits(-0.0);

    auto reader = open_f64(interleaved, 2U);
    auto result = audio::build_waveform_summary(*reader);
    QVERIFY(result);
    const auto& summary = *result.value();
    const auto base = summary.level(0U);
    QVERIFY(base);
    QCOMPARE(base.value()->frames_per_bucket().value(), std::int64_t{2});
    QCOMPARE(base.value()->bucket_count().value(), std::int64_t{32'769});

    const auto leftZero = base.value()->peak(0U, core::FrameIndex{0});
    QVERIFY(leftZero);
    QCOMPARE(bits(leftZero.value()->minimum), bits(-0.0));
    QCOMPARE(bits(leftZero.value()->maximum), bits(0.0));
    const auto rightFirst = base.value()->peak(1U, core::FrameIndex{0});
    QVERIFY(rightFirst);
    QCOMPARE(bits(rightFirst.value()->minimum), std::uint64_t{1U});
    QCOMPARE(bits(rightFirst.value()->maximum), bits(1.25));
    const auto leftSecond = base.value()->peak(0U, core::FrameIndex{1});
    QVERIFY(leftSecond);
    QCOMPARE(bits(leftSecond.value()->minimum), bits(-1.5));

    QVERIFY(summary.payload_bytes() <= 4'194'272U);
}

void WaveformSummaryTest::decodePartitionsAreBitIdentical()
{
    std::vector<std::uint64_t> samples;
    samples.reserve(70'003U);
    for (std::size_t index = 0; index < 70'003U; ++index) {
        const double value = static_cast<double>(static_cast<int>(index % 31U) - 15) / 16.0;
        samples.push_back(bits(value));
    }

    auto referenceReader = open_f64(samples);
    auto reference = audio::build_waveform_summary_with_block_limit(
        *referenceReader, 4'096U);
    QVERIFY(reference);
    for (const auto partition : std::array<std::size_t, 7>{1U, 2U, 7U, 255U, 4'095U, 4'096U, 997U}) {
        auto reader = open_f64(samples);
        auto candidate = audio::build_waveform_summary_with_block_limit(
            *reader, partition);
        QVERIFY(candidate);
        compare_summaries(*reference.value(), *candidate.value());
    }
}

void WaveformSummaryTest::cancellationAndBoundsAreDeterministic()
{
    std::stop_source cancellation;
    cancellation.request_stop();
    auto reader = open_f64(std::vector<std::uint64_t>{bits(0.0)});
    auto cancelled = audio::build_waveform_summary(*reader, cancellation.get_token());
    QVERIFY(!cancelled);
    QCOMPARE(cancelled.error()->code(), core::ErrorCode::InvalidState);

    auto invalidBlockReader = open_f64(std::vector<std::uint64_t>{bits(0.0)});
    auto invalidBlock = audio::build_waveform_summary_with_block_limit(
        *invalidBlockReader, 4'097U);
    QVERIFY(!invalidBlock);
    QCOMPARE(invalidBlock.error()->code(), core::ErrorCode::InvalidArgument);
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::WaveformSummaryTest)

#include "test_waveform_summary.moc"
