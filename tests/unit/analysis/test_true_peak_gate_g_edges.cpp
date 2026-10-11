#include "internal/true_peak_full_resource.hpp"
#include "internal/true_peak_itu4x_channel_stream.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/frame_time.hpp>

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
using analysis::internal::measure_unqualified_true_peak_full_resource;
using analysis::internal::TruePeakItu4xChannelStream;

// Independent signed integer transcription of ITU BS.1770-5 Annex 2,
// lag-major rather than importing the B1a production bank.
constexpr std::array<std::array<int, 4>, 12> kOfficialLagMajor{{
    {14,-239,-155,-68}, {90,240,271,122},
    {-161,-424,-477,-218}, {272,730,832,390},
    {-487,-1364,-1641,-838}, {1125,3810,6388,7964},
    {7964,6388,3810,1125}, {-838,-1641,-1364,-487},
    {390,832,730,272}, {-218,-477,-424,-161},
    {122,271,240,90}, {-68,-155,-239,14}
}};

[[nodiscard]] core::Result<audio::AudioBuffer> make_audio(
    std::int64_t rateHz, audio::ChannelLayout layout,
    std::span<const double> l, std::span<const double> r = {})
{
    auto rate = core::SampleRate::create(rateHz);
    if (!rate) return core::Result<audio::AudioBuffer>::failure(*rate.error());
    auto format = audio::AudioFormat::create(*rate.value(), layout);
    if (!format) return core::Result<audio::AudioBuffer>::failure(*format.error());
    auto frames = core::FrameCount::create(static_cast<std::int64_t>(l.size()));
    if (!frames) return core::Result<audio::AudioBuffer>::failure(*frames.error());
    if (layout == audio::ChannelLayout::STEREO_LR && l.size() != r.size()) {
        return core::Result<audio::AudioBuffer>::failure(
            core::Error{core::ErrorCode::InvalidArgument, "Fixture length mismatch."});
    }
    auto owned = audio::AudioBuffer::create(
        *format.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, *frames.value());
    if (!owned) return owned;
    auto w = owned.value()->mutable_view();
    auto left = w.channel(0);
    if (!left) return core::Result<audio::AudioBuffer>::failure(*left.error());
    std::copy(l.begin(), l.end(), left.value()->begin());
    if (layout == audio::ChannelLayout::STEREO_LR) {
        auto right = w.channel(1);
        if (!right) return core::Result<audio::AudioBuffer>::failure(*right.error());
        std::copy(r.begin(), r.end(), right.value()->begin());
    }
    return owned;
}

// Independent integer convolution for dyadic PCM x[n]=z[n]/256.
// Every numerator fits int64 here and every output is exactly representable
// in binary64, so no floating point tolerance hides an indexing defect.
[[nodiscard]] std::int64_t integer_peak_numerator(
    std::span<const std::int32_t> z)
{
    std::int64_t peak = 0;
    for (std::size_t i = 0; i < z.size() + 11; ++i) {
        for (std::size_t p = 0; p < 4; ++p) {
            std::int64_t sum = 0;
            for (std::size_t lag = 0; lag < 12; ++lag) {
                if (i >= lag && i - lag < z.size())
                    sum += static_cast<std::int64_t>(
                        kOfficialLagMajor[lag][p]) * z[i - lag];
            }
            peak = std::max(peak, sum < 0 ? -sum : sum);
        }
    }
    return peak;
}

class TruePeakGateGEdgePrequalTest final : public QObject {
    Q_OBJECT
private slots:
    void allBoundaryImpulsesAndPolarity();
    void independentDyadicIntegerConvolutionAndStereo();
    void subnormalAndFiniteOverflowRemainTruthful();
};

void TruePeakGateGEdgePrequalTest::allBoundaryImpulsesAndPolarity()
{
    // First/last impulse must keep ALL 48 high-rate samples, especially the
    // four outputs from the eleventh analyzer-only EOS zero input frame.
    constexpr std::array<std::size_t, 11> lengths{
        1,2,3,10,11,12,13,31,32,33,257};
    for (const auto n : lengths) {
        for (const auto pos : {std::size_t{0},std::size_t{1},
                               n / 2,n - 2,n - 1}) {
            if (pos >= n) continue;
            for (const double amp : {-2.0,-1.0,1.0,2.0}) {
                std::vector<double> x(n, 0.0);
                x[pos] = amp;
                auto buf = make_audio(48000, audio::ChannelLayout::MONO_C, x);
                QVERIFY(buf);
                auto tp = measure_unqualified_true_peak_full_resource(*buf.value());
                QVERIFY(tp);
                QCOMPARE(tp.value()->global_linear_peak,
                         std::abs(amp) * (7964.0 / 8192.0));
                QCOMPARE(tp.value()->channel_count, std::size_t{1});
                auto untouched = buf.value()->view().channel(0);
                QVERIFY(untouched);
                QCOMPARE((*untouched.value())[pos], amp);
                QCOMPARE(buf.value()->view().frame_count().value(),
                         static_cast<std::int64_t>(n));
            }
        }
    }
}

void TruePeakGateGEdgePrequalTest::independentDyadicIntegerConvolutionAndStereo()
{
    for (const std::size_t n : {std::size_t{33}, std::size_t{257}}) {
        std::vector<std::int32_t> zl(n), zr(n);
        std::vector<double> l(n), r(n);
        std::uint32_t stateL = 0x4D313600U, stateR = 0x4D313601U;
        for (std::size_t i = 0; i < n; ++i) {
            stateL = 1664525U * stateL + 1013904223U;
            stateR = 1664525U * stateR + 1013904223U;
            zl[i] = static_cast<std::int32_t>((stateL >> 24) & 0xffU) - 128;
            zr[i] = static_cast<std::int32_t>((stateR >> 24) & 0xffU) - 128;
            l[i] = static_cast<double>(zl[i]) / 256.0;
            r[i] = static_cast<double>(zr[i]) / 256.0;
        }
        const double pl = static_cast<double>(integer_peak_numerator(zl))
                        / (8192.0 * 256.0);
        const double pr = static_cast<double>(integer_peak_numerator(zr))
                        / (8192.0 * 256.0);
        for (auto sampleRate : {44100, 192000}) {
            for (int swap = 0; swap != 2; ++swap) {
                auto buf = make_audio(sampleRate, audio::ChannelLayout::STEREO_LR,
                                      swap ? std::span<const double>(r) : std::span<const double>(l),
                                      swap ? std::span<const double>(l) : std::span<const double>(r));
                QVERIFY(buf);
                auto tp = measure_unqualified_true_peak_full_resource(*buf.value());
                QVERIFY(tp);
                QCOMPARE(tp.value()->per_channel_linear_peak[swap], pl);
                QCOMPARE(tp.value()->per_channel_linear_peak[1 - swap], pr);
                QCOMPARE(tp.value()->global_linear_peak, std::max(pl, pr));
                QCOMPARE(tp.value()->is_exact_silence, false);
            }
        }
    }
}

void TruePeakGateGEdgePrequalTest::subnormalAndFiniteOverflowRemainTruthful()
{
    const double tiny = std::numeric_limits<double>::denorm_min();
    QVERIFY(tiny > 0.0);
    const std::array<double, 1> subnormal{tiny};
    auto buf = make_audio(48000, audio::ChannelLayout::MONO_C, subnormal);
    QVERIFY(buf);
    auto tp = measure_unqualified_true_peak_full_resource(*buf.value());
    QVERIFY(tp);
    // This input is non-silent after rounding. A flush-to-zero path is invalid.
    QCOMPARE(tp.value()->global_linear_peak, tiny);
    QVERIFY(tp.value()->global_db_tp.has_value());
    QVERIFY(std::isfinite(*tp.value()->global_db_tp));

    const double m = std::numeric_limits<double>::max();
    std::array<double, 12> high{};
    // At frame i=11, phase 1's sum of abs tap weights is 16571/8192 >1,
    // so sign-aligned finite DBL_MAX samples MUST overflow categorically.
    for (std::size_t lag = 0; lag < high.size(); ++lag) {
        const int c = kOfficialLagMajor[lag][1];
        high[11 - lag] = c < 0 ? -m : m;
    }
    auto overflowing = make_audio(
        48000, audio::ChannelLayout::MONO_C, high);
    QVERIFY(overflowing);
    auto failed = measure_unqualified_true_peak_full_resource(
        *overflowing.value());
    QVERIFY(!failed);
    QCOMPARE(failed.error()->code(), core::ErrorCode::InvalidAudioSample);
    auto pristine = overflowing.value()->view().channel(0);
    QVERIFY(pristine);
    for (std::size_t i = 0; i < high.size(); ++i)
        QCOMPARE((*pristine.value())[i], high[i]);

    // A rejected extreme sample must not alter history, accepted count, or
    // the last valid peak. The already-accepted DBL_MAX history may STILL
    // overflow when the next input is zero: this is not failed recovery.
    // Compare both success and categorical failure against a snapshot taken
    // strictly BEFORE the rejected sample, rather than requiring success.
    TruePeakItu4xChannelStream s;
    bool sawOverflow = false;
    for (double sample : high) {
        const auto before = s;
        auto step = s.push(sample);
        if (step) continue;
        sawOverflow = true;
        QCOMPARE(step.error()->code(), core::ErrorCode::InvalidAudioSample);
        QCOMPARE(s.programme_frames(), before.programme_frames());
        QCOMPARE(s.linear_peak_so_far(), before.linear_peak_so_far());
        QCOMPARE(s.finalized(), before.finalized());

        // Multiple probes start from independent cloned snapshots. Even if
        // their validly retained extreme PCM causes a second overflow, the
        // two histories must produce IDENTICAL outcomes and accepted state.
        for (double probe : {0.0, 1.0, -1.0}) {
            auto actual = s;
            auto expectedAfterFailure = before;
            auto following = actual.push(probe);
            auto expected = expectedAfterFailure.push(probe);
            QCOMPARE(following.has_value(), expected.has_value());
            if (following) {
                for (std::size_t phase = 0; phase < 4; ++phase)
                    QCOMPARE(following.value()->signed_phases[phase],
                             expected.value()->signed_phases[phase]);
            } else {
                QCOMPARE(following.error()->code(), expected.error()->code());
            }
            QCOMPARE(actual.programme_frames(),
                     expectedAfterFailure.programme_frames());
            QCOMPARE(actual.linear_peak_so_far(),
                     expectedAfterFailure.linear_peak_so_far());
            QCOMPARE(actual.finalized(), expectedAfterFailure.finalized());
        }

        // Reset is the explicit operation that removes the extreme history.
        s.reset();
        QCOMPARE(s.programme_frames(), std::uint64_t{0});
        QCOMPARE(s.linear_peak_so_far(), 0.0);
        QVERIFY(s.push(0.0));
        auto freshSilence = s.finalize();
        QVERIFY(freshSilence);
        QCOMPARE(*freshSilence.value(), 0.0);
        break;
    }
    QVERIFY(sawOverflow);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakGateGEdgePrequalTest)
#include "test_true_peak_gate_g_edges.moc"
