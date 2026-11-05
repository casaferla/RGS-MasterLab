#include "internal/true_peak_itu4x_channel_stream.hpp"
#include "internal/true_peak_chronological_buckets.hpp"

#include <rgsml/core/error.hpp>
#include <QTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {
using analysis::internal::TruePeakItu4xChannelStream;

// Independent test-only integer transcription: not imported from production.
constexpr std::array<std::array<int, 4>, 12> kLagMajor{{
    {14,-239,-155,-68}, {90,240,271,122},
    {-161,-424,-477,-218}, {272,730,832,390},
    {-487,-1364,-1641,-838}, {1125,3810,6388,7964},
    {7964,6388,3810,1125}, {-838,-1641,-1364,-487},
    {390,832,730,272}, {-218,-477,-424,-161},
    {122,271,240,90}, {-68,-155,-239,14}
}};

[[nodiscard]] double oracle_peak(std::span<const double> pcm)
{
    double peak = 0.0;
    // Independent full-support direct convolution, not FIR history replay.
    for (std::size_t i = 0; i < pcm.size() + 11; ++i) {
        for (std::size_t phase = 0; phase < 4; ++phase) {
            double sum = 0.0;
            for (std::size_t lag = 0; lag < 12; ++lag) {
                if (i >= lag && i - lag < pcm.size())
                    sum += pcm[i - lag] *
                           (static_cast<double>(kLagMajor[lag][phase]) / 8192.0);
            }
            peak = std::max(peak, std::abs(sum));
        }
    }
    return peak;
}

class TruePeakItu4xChannelStreamTest final : public QObject {
    Q_OBJECT
private slots:
    void signedPhaseIndexingAndFullTail();
    void directConvolutionAndChunkPartitionIdentity();
    void lifecycleAndLateFailureAtomicity();
    void chronologicalBucketSixRatesAndFIRParity();
    void chronologicalHoldAndGapNeverCertifyPartial();
    void chronologicalInvalidInputsAreAtomic();
    void chronologicalSeekAndUnknownHistoryDoNotInventValidBuckets();
};

void TruePeakItu4xChannelStreamTest::signedPhaseIndexingAndFullTail()
{
    TruePeakItu4xChannelStream s;
    auto impulse = s.push(1.0);
    QVERIFY(impulse);
    QCOMPARE(s.programme_frames(), std::uint64_t{1});
    for (std::size_t p = 0; p < 4; ++p)
        QCOMPARE(impulse.value()->signed_phases[p],
                 static_cast<double>(kLagMajor[0][p]) / 8192.0);
    QVERIFY(impulse.value()->signed_phases[1] < 0.0);
    auto complete = s.finalize();
    QVERIFY(complete);
    QCOMPARE(*complete.value(), 7964.0 / 8192.0);
    QCOMPARE(s.programme_frames(), std::uint64_t{1});
    QVERIFY(s.finalized());
    QVERIFY(!s.push(1.0));
    auto duplicate = s.finalize();
    QVERIFY(!duplicate);
    QCOMPARE(duplicate.error()->code(), core::ErrorCode::InvalidState);

    s.reset();
    auto plus = s.push(1.0);
    QVERIFY(plus);
    auto second = s.push(1.0);
    QVERIFY(second);
    for (std::size_t p = 0; p < 4; ++p)
        QCOMPARE(second.value()->signed_phases[p],
                 static_cast<double>(kLagMajor[0][p] + kLagMajor[1][p]) / 8192.0);
    auto pairPeak = s.finalize();
    QVERIFY(pairPeak);
    QCOMPARE(*pairPeak.value(), 10198.0 / 8192.0);
}

void TruePeakItu4xChannelStreamTest::directConvolutionAndChunkPartitionIdentity()
{
    const std::vector<std::vector<double>> cases{
        {0.0}, {1.0}, {-1.0}, {1.0,1.0}, {1.0,-1.0},
        {0.0,1.0,0.0,-1.0,2.0,-2.0,0.5,-0.25},
        std::vector<double>(48, 0.3), std::vector<double>(20, 0.0)
    };
    for (const auto& pcm : cases) {
        TruePeakItu4xChannelStream full;
        for (double x : pcm) QVERIFY(full.push(x));
        auto peak = full.finalize();
        QVERIFY(peak);
        QVERIFY(std::abs(*peak.value() - oracle_peak(pcm)) < 2e-15);

        // Explicit partition boundaries must never reset FIR history.
        for (std::size_t block : {std::size_t{1}, std::size_t{2},
                                  std::size_t{3}, std::size_t{11},
                                  std::size_t{13}, std::size_t{31}}) {
            TruePeakItu4xChannelStream chunks;
            for (std::size_t begin = 0; begin < pcm.size(); begin += block) {
                const auto end = std::min(pcm.size(), begin + block);
                for (std::size_t j = begin; j < end; ++j)
                    QVERIFY(chunks.push(pcm[j]));
            }
            auto segmentedPeak = chunks.finalize();
            QVERIFY(segmentedPeak);
            QCOMPARE(*segmentedPeak.value(), *peak.value());
            QCOMPARE(chunks.programme_frames(),
                     static_cast<std::uint64_t>(pcm.size()));
        }
    }
}

void TruePeakItu4xChannelStreamTest::lifecycleAndLateFailureAtomicity()
{
    TruePeakItu4xChannelStream s;
    auto empty = s.finalize();
    QVERIFY(!empty);
    QCOMPARE(empty.error()->code(), core::ErrorCode::InvalidState);
    QVERIFY(s.push(0.0));
    auto silence = s.finalize();
    QVERIFY(silence);
    QCOMPARE(*silence.value(), 0.0);

    s.reset();
    QVERIFY(s.push(2.0)); // finite > 1 PCM is legal
    const double before = s.linear_peak_so_far();
    auto bad = s.push(std::numeric_limits<double>::quiet_NaN());
    QVERIFY(!bad);
    QCOMPARE(bad.error()->code(), core::ErrorCode::InvalidAudioSample);
    QCOMPARE(s.programme_frames(), std::uint64_t{1});
    QCOMPARE(s.linear_peak_so_far(), before);
    auto inf = s.push(std::numeric_limits<double>::infinity());
    QVERIFY(!inf);
    QCOMPARE(inf.error()->code(), core::ErrorCode::InvalidAudioSample);
    QCOMPARE(s.programme_frames(), std::uint64_t{1});
    QCOMPARE(s.linear_peak_so_far(), before);

    // Failure does not silently finalize or change an accepted frame.
    s.reset();
    QVERIFY(s.push(1.0));
    const double first = s.linear_peak_so_far();
    QVERIFY(!s.push(-std::numeric_limits<double>::infinity()));
    QCOMPARE(s.linear_peak_so_far(), first);
    QCOMPARE(s.programme_frames(), std::uint64_t{1});
    QVERIFY(s.finalize());
}


void TruePeakItu4xChannelStreamTest::chronologicalBucketSixRatesAndFIRParity()
{
    using analysis::internal::ChronologicalTruePeakBuckets;
    for (const std::int64_t hz : {44100,48000,88200,96000,176400,192000}) {
        for (const std::size_t channels : {std::size_t{1}, std::size_t{2}}) {
            auto maker = ChronologicalTruePeakBuckets::create(hz, channels);
            QVERIFY(maker);
            auto meter = std::move(*maker.value());
            QCOMPARE(meter.frames_per_bucket(), hz / 100);
            const auto frames = meter.frames_per_bucket() * 10;
            std::vector<double> independent(static_cast<std::size_t>(frames), 0.0);
            const auto impulse = frames - 25;
            independent[static_cast<std::size_t>(impulse)] = 0.875;
            for (std::int64_t i = 0; i < frames; ++i) {
                const std::array<double, 2> current{
                    independent[static_cast<std::size_t>(i)],
                    -0.5 * independent[static_cast<std::size_t>(i)]};
                QVERIFY(meter.consume_frame(
                    std::span<const double>{current.data(), channels}, true));
            }
            const auto seen = meter.snapshot();
            QVERIFY(seen.recent100msComplete);
            QVERIFY(!seen.shortHold1sComplete);
            QCOMPARE(seen.consumedSourceFrames, frames);
            QCOMPARE(seen.lastCompleteBucketEnd, frames);
            QVERIFY(std::abs(seen.recentLinearPeak[0] - oracle_peak(independent)) < 2e-15);
            if (channels == 2) {
                QVERIFY(std::abs(seen.recentLinearPeak[1]
                    - 0.5 * oracle_peak(independent)) < 2e-15);
            } else {
                QCOMPARE(seen.recentLinearPeak[1], 0.0);
            }
        }
    }
}

void TruePeakItu4xChannelStreamTest::chronologicalHoldAndGapNeverCertifyPartial()
{
    using analysis::internal::ChronologicalTruePeakBuckets;
    auto created = ChronologicalTruePeakBuckets::create(48000, 1);
    QVERIFY(created);
    auto meter = std::move(*created.value());
    const std::array<double, 1> spike{0.95};
    const std::array<double, 1> silence{0.0};
    QVERIFY(meter.consume_frame(spike, true));
    for (int frame = 1; frame < 48000; ++frame)
        QVERIFY(meter.consume_frame(silence, true));
    const auto oneSecond = meter.snapshot();
    QVERIFY(oneSecond.recent100msComplete);
    QVERIFY(oneSecond.shortHold1sComplete);
    QCOMPARE(oneSecond.recentLinearPeak[0], 0.0);
    QVERIFY(oneSecond.shortHoldLinearPeak[0] > 0.9);

    // One consumed but UNPROVEN frame poisons the current bucket and the
    // 11-frame FIR support after a discontinuity; missing is never silence.
    QVERIFY(meter.consume_gap());
    for (int i = 1; i < 480; ++i)
        QVERIFY(meter.consume_frame(silence, true));
    const auto gap = meter.snapshot();
    QVERIFY(!gap.recent100msComplete);
    QVERIFY(!gap.shortHold1sComplete);
    QCOMPARE(gap.recentLinearPeak[0], 0.0);
    QCOMPARE(gap.shortHoldLinearPeak[0], 0.0);
    for (int i = 0; i < 480 * 100; ++i)
        QVERIFY(meter.consume_frame(silence, true));
    const auto recovered = meter.snapshot();
    QVERIFY(recovered.recent100msComplete);
    QVERIFY(recovered.shortHold1sComplete);
    QCOMPARE(recovered.recentLinearPeak[0], 0.0);
    QCOMPARE(recovered.shortHoldLinearPeak[0], 0.0);
}

void TruePeakItu4xChannelStreamTest::chronologicalInvalidInputsAreAtomic()
{
    using analysis::internal::ChronologicalTruePeakBuckets;
    QVERIFY(!ChronologicalTruePeakBuckets::create(32000, 2));
    QVERIFY(!ChronologicalTruePeakBuckets::create(48000, 3));
    QVERIFY(!ChronologicalTruePeakBuckets::create(48000, 1, -1));
    auto created = ChronologicalTruePeakBuckets::create(48000, 2);
    QVERIFY(created);
    auto meter = std::move(*created.value());
    const std::array<double, 2> good{0.2, -0.2};
    QVERIFY(meter.consume_frame(good, true));
    const auto before = meter.snapshot();
    const std::array<double, 2> badRight{
        0.4, std::numeric_limits<double>::quiet_NaN()};
    QVERIFY(!meter.consume_frame(badRight, true));
    QCOMPARE(meter.consumed_source_frames(), before.consumedSourceFrames);
    const std::array<double, 1> shortFrame{0.1};
    QVERIFY(!meter.consume_frame(shortFrame, true));
    QCOMPARE(meter.consumed_source_frames(), before.consumedSourceFrames);
    for (int i = 1; i < 480 * 10; ++i)
        QVERIFY(meter.consume_frame(good, true));
    const auto after = meter.snapshot();
    QVERIFY(after.recent100msComplete);
    QVERIFY(after.recentLinearPeak[0] > 0.0);
}


void TruePeakItu4xChannelStreamTest::chronologicalSeekAndUnknownHistoryDoNotInventValidBuckets()
{
    using analysis::internal::ChronologicalTruePeakBuckets;
    // A seek to frame 480 lacks the eleven previous source samples.
    // The first bucket is intrinsically incomplete; after 11 proven samples,
    // only *subsequent complete* buckets may become candidate-available.
    auto created = ChronologicalTruePeakBuckets::create(48000, 1, 480);
    QVERIFY(created);
    auto meter = std::move(*created.value());
    const std::array<double, 1> silence{0.0};
    for (int n = 0; n < 480 * 11; ++n)
        QVERIFY(meter.consume_frame(silence, true));
    const auto first = meter.snapshot();
    QCOMPARE(first.consumedSourceFrames, std::int64_t{5760});
    QCOMPARE(first.lastCompleteBucketEnd, std::int64_t{5760});
    QVERIFY(first.recent100msComplete);
    QVERIFY(!first.shortHold1sComplete);
    QCOMPARE(first.recentLinearPeak[0], 0.0);

    // An unproven frame in an otherwise silent signal must destroy certainty,
    // rather than being silently interpreted as an exact digital zero.
    QVERIFY(meter.consume_gap());
    for (int n = 1; n < 480; ++n)
        QVERIFY(meter.consume_frame(silence, true));
    const auto lost = meter.snapshot();
    QVERIFY(!lost.recent100msComplete);
    QVERIFY(!lost.shortHold1sComplete);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakItu4xChannelStreamTest)
#include "test_true_peak_itu4x_channel_stream.moc"
