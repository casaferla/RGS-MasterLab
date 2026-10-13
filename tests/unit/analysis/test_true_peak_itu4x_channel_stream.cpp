#include "internal/true_peak_itu4x_channel_stream.hpp"

#include <rgsml/core/error.hpp>
#include <QTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
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

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakItu4xChannelStreamTest)
#include "test_true_peak_itu4x_channel_stream.moc"
