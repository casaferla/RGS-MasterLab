#include "internal/true_peak_itu4x_channel_stream.hpp"

#include <rgsml/core/error.hpp>
#include <QTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace rgsml::tests {
namespace {

using analysis::internal::TruePeakItu4xChannelStream;

// Test-only independent lag-major integer transcription, not the production
// binary64 coefficient asset or its FIR/history reduction implementation.
constexpr std::array<std::array<int, 4>, 12> kOracleLagMajor{{
    {14,-239,-155,-68}, {90,240,271,122},
    {-161,-424,-477,-218}, {272,730,832,390},
    {-487,-1364,-1641,-838}, {1125,3810,6388,7964},
    {7964,6388,3810,1125}, {-838,-1641,-1364,-487},
    {390,832,730,272}, {-218,-477,-424,-161},
    {122,271,240,90}, {-68,-155,-239,14}
}};

[[nodiscard]] double independent_dyadic_peak(
    std::span<const std::int32_t> numerators)
{
    std::int64_t maximum = 0;
    for (std::size_t i = 0; i < numerators.size() + 11; ++i) {
        for (std::size_t phase = 0; phase < 4; ++phase) {
            std::int64_t signedSum = 0;
            for (std::size_t lag = 0; lag < 12; ++lag) {
                if (i >= lag && i - lag < numerators.size())
                    signedSum +=
                        static_cast<std::int64_t>(kOracleLagMajor[lag][phase])
                        * numerators[i - lag];
            }
            maximum = std::max(maximum,
                               signedSum < 0 ? -signedSum : signedSum);
        }
    }
    // Every fixture sample is exactly z/256. Every resulting peak is an
    // exactly representable binary64 multiple of 1/(8192*256).
    return static_cast<double>(maximum) / (8192.0 * 256.0);
}

[[nodiscard]] std::span<const double> sub(
    const std::vector<double>& v, std::size_t start, std::size_t count)
{
    return std::span<const double>{v.data() + start, count};
}

class TruePeakChunkInvarianceTest final : public QObject {
    Q_OBJECT
private slots:
    void independentOracleCanonicalPartitionsAndEmptyChunks();
    void lateChunkFailureRollbackAndReset();
};

void TruePeakChunkInvarianceTest::independentOracleCanonicalPartitionsAndEmptyChunks()
{
    constexpr std::array<std::size_t, 18> lengths{
        1,2,3,7,10,11,12,13,31,32,33,64,255,256,257,4095,4096,4097};
    constexpr std::array<std::size_t, 18> fixedBlocks{
        1,2,3,7,10,11,12,13,31,32,33,64,255,256,257,4095,4096,4097};
    constexpr std::array<std::size_t, 8> ragged{
        1,7,2,11,3,12,5,31};

    for (const std::size_t count : lengths) {
        for (int pattern = 0; pattern != 3; ++pattern) {
            std::vector<std::int32_t> z(count);
            std::vector<double> pcm(count);
            std::uint32_t state = 0x4D313600U;
            for (std::size_t i = 0; i < count; ++i) {
                state = state * 1664525U + 1013904223U;
                z[i] = static_cast<std::int32_t>((state >> 24) & 0xffU) - 128;
                if (pattern != 0) z[i] = 0;
            }
            if (pattern == 1) z[0] = -256;
            if (pattern == 2) z[count-1] = 512;
            for (std::size_t i = 0; i < count; ++i)
                pcm[i] = static_cast<double>(z[i]) / 256.0;

            TruePeakItu4xChannelStream samplewise;
            for (const double v : pcm)
                QVERIFY(samplewise.push(v));
            const double beforeTail = samplewise.linear_peak_so_far();
            const auto samplePeak = samplewise.finalize();
            QVERIFY(samplePeak);
            QCOMPARE(*samplePeak.value(),
                independent_dyadic_peak(std::span<const std::int32_t>{
                    z.data(), z.size()}));
            QCOMPARE(samplewise.programme_frames(),
                     static_cast<std::uint64_t>(count));

            for (const std::size_t block : fixedBlocks) {
                TruePeakItu4xChannelStream chunked;
                for (std::size_t begin = 0; begin < count; begin += block) {
                    const std::size_t end = std::min(begin + block, count);
                    auto beforeEmptyCount = chunked.programme_frames();
                    QVERIFY(chunked.push_chunk(std::span<const double>{}));
                    QCOMPARE(chunked.programme_frames(), beforeEmptyCount);
                    QVERIFY(chunked.push_chunk(sub(pcm, begin, end-begin)));
                }
                QCOMPARE(chunked.programme_frames(),
                         static_cast<std::uint64_t>(count));
                QCOMPARE(chunked.linear_peak_so_far(), beforeTail);
                auto result = chunked.finalize();
                QVERIFY(result);
                QCOMPARE(*result.value(), *samplePeak.value());
            }

            TruePeakItu4xChannelStream irregular;
            std::size_t begin = 0;
            std::size_t index = 0;
            while (begin < count) {
                const auto segment = std::min(ragged[index % ragged.size()],
                                              count - begin);
                QVERIFY(irregular.push_chunk(sub(pcm, begin, segment)));
                begin += segment;
                ++index;
            }
            auto irregularPeak = irregular.finalize();
            QVERIFY(irregularPeak);
            QCOMPARE(*irregularPeak.value(), *samplePeak.value());

            // Exhaustive two-way split for the small boundary-vector sizes.
            if (count <= 33) {
                for (std::size_t cut = 0; cut <= count; ++cut) {
                    TruePeakItu4xChannelStream split;
                    QVERIFY(split.push_chunk(sub(pcm, 0, cut)));
                    QVERIFY(split.push_chunk(sub(pcm, cut, count-cut)));
                    auto splitPeak = split.finalize();
                    QVERIFY(splitPeak);
                    QCOMPARE(*splitPeak.value(), *samplePeak.value());
                }
            }
        }
    }
}

void TruePeakChunkInvarianceTest::lateChunkFailureRollbackAndReset()
{
    const std::array<double, 3> initial{0.125,-0.25,0.375};
    const std::array<double, 4> invalid{
        0.125,0.25,std::numeric_limits<double>::quiet_NaN(),0.5};
    const std::array<double, 2> continuation{-0.125,0.25};

    TruePeakItu4xChannelStream probe;
    QVERIFY(probe.push_chunk(initial));
    const auto before = probe;
    auto rejected = probe.push_chunk(invalid);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(), core::ErrorCode::InvalidAudioSample);
    QCOMPARE(probe.programme_frames(), before.programme_frames());
    QCOMPARE(probe.linear_peak_so_far(), before.linear_peak_so_far());
    QCOMPARE(probe.finalized(), before.finalized());

    // A subsequent valid chunk must produce exactly the same signed state
    // and peak as the snapshot before the rejected multi-frame chunk.
    auto baseline = before;
    QVERIFY(baseline.push_chunk(continuation));
    QVERIFY(probe.push_chunk(continuation));
    QCOMPARE(probe.programme_frames(), baseline.programme_frames());
    QCOMPARE(probe.linear_peak_so_far(), baseline.linear_peak_so_far());
    const auto referencePeak = baseline.finalize();
    const auto observedPeak = probe.finalize();
    QVERIFY(referencePeak && observedPeak);
    QCOMPARE(*observedPeak.value(), *referencePeak.value());

    auto postFinalize = probe.push_chunk(std::span<const double>{});
    QVERIFY(!postFinalize);
    QCOMPARE(postFinalize.error()->code(), core::ErrorCode::InvalidState);

    // Reset is a fresh analysis resource; empty chunks are an internal
    // pre-finalize no-op, but do not legalize empty-resource finalization.
    probe.reset();
    QVERIFY(probe.push_chunk(std::span<const double>{}));
    auto empty = probe.finalize();
    QVERIFY(!empty);
    QCOMPARE(empty.error()->code(), core::ErrorCode::InvalidState);
    const std::array<double, 1> silence{0.0};
    QVERIFY(probe.push_chunk(silence));
    auto quiet = probe.finalize();
    QVERIFY(quiet);
    QCOMPARE(*quiet.value(), 0.0);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakChunkInvarianceTest)
#include "test_true_peak_chunk_invariance.moc"
