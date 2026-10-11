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
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace rgsml::tests {
namespace {

using analysis::internal::measure_unqualified_true_peak_full_resource;
using audio::ChannelLayout;

// Test-only factory uses the real binary64 owning resource, NOT an arbitrary
// AudioBufferView subrange. It preserves PCM bytes, including signed zero.
[[nodiscard]] core::Result<audio::AudioBuffer> make_pcm(
    std::int64_t hz, ChannelLayout layout,
    std::span<const double> left, std::span<const double> right = {})
{
    auto rate = core::SampleRate::create(hz);
    if (!rate) return core::Result<audio::AudioBuffer>::failure(*rate.error());
    auto fmt = audio::AudioFormat::create(*rate.value(), layout);
    if (!fmt) return core::Result<audio::AudioBuffer>::failure(*fmt.error());
    auto count = core::FrameCount::create(static_cast<std::int64_t>(left.size()));
    if (!count) return core::Result<audio::AudioBuffer>::failure(*count.error());
    if (layout == ChannelLayout::STEREO_LR && left.size() != right.size()) {
        return core::Result<audio::AudioBuffer>::failure(
            core::Error{core::ErrorCode::InvalidArgument, "Unequal test channel lengths."});
    }
    auto buffer = audio::AudioBuffer::create(
        *fmt.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, *count.value());
    if (!buffer) return buffer;
    auto writable = buffer.value()->mutable_view();
    auto l = writable.channel(0);
    if (!l) return core::Result<audio::AudioBuffer>::failure(*l.error());
    std::copy(left.begin(), left.end(), l.value()->begin());
    if (layout == ChannelLayout::STEREO_LR) {
        auto r = writable.channel(1);
        if (!r) return core::Result<audio::AudioBuffer>::failure(*r.error());
        std::copy(right.begin(), right.end(), r.value()->begin());
    }
    return buffer;
}

class TruePeakFullResourceTest final : public QObject {
    Q_OBJECT
private slots:
    void monoSixRatesImpulseAndTail();
    void stereoIndependentChannelsAndSwaps();
    void silenceNullDbAndDcNoCompensation();
    void rejectsLateNonfiniteAndUnsupportedWithoutSourceMutation();
    void fullResource4096ChunkBoundaryParity();
};

void TruePeakFullResourceTest::monoSixRatesImpulseAndTail()
{
    constexpr std::array<std::int64_t, 6> rates{
        44100, 48000, 88200, 96000, 176400, 192000};
    const std::vector<double> impulse{1.0};
    const std::vector<double> lastImpulse{0.0, 0.0, 0.0, 1.0};
    for (auto rate : rates) {
        for (const auto& pcm : {impulse, lastImpulse}) {
            auto buf = make_pcm(rate, ChannelLayout::MONO_C, pcm);
            QVERIFY(buf);
            auto evidence = measure_unqualified_true_peak_full_resource(*buf.value());
            QVERIFY(evidence);
            QCOMPARE(evidence.value()->sample_rate_hz, rate);
            QCOMPARE(evidence.value()->channel_count, std::size_t{1});
            QCOMPARE(evidence.value()->per_channel_linear_peak[0], 7964.0 / 8192.0);
            QCOMPARE(evidence.value()->global_linear_peak, 7964.0 / 8192.0);
            QVERIFY(!evidence.value()->is_exact_silence);
            QVERIFY(evidence.value()->per_channel_db_tp[0].has_value());
            QVERIFY(evidence.value()->global_db_tp.has_value());
            QCOMPARE(*evidence.value()->global_db_tp,
                     20.0 * std::log10(7964.0 / 8192.0));
        }
    }
}

void TruePeakFullResourceTest::stereoIndependentChannelsAndSwaps()
{
    const std::vector<double> left{1.0};
    const std::vector<double> right{-2.0};
    for (int swap = 0; swap != 2; ++swap) {
        auto buf = make_pcm(48000, ChannelLayout::STEREO_LR,
                            swap ? right : left, swap ? left : right);
        QVERIFY(buf);
        auto evidence = measure_unqualified_true_peak_full_resource(*buf.value());
        QVERIFY(evidence);
        QCOMPARE(evidence.value()->channel_count, std::size_t{2});
        QCOMPARE(evidence.value()->per_channel_linear_peak[swap], 7964.0 / 8192.0);
        QCOMPARE(evidence.value()->per_channel_linear_peak[1-swap],
                 2.0 * 7964.0 / 8192.0);
        QCOMPARE(evidence.value()->global_linear_peak, 2.0 * 7964.0 / 8192.0);
        QVERIFY(!evidence.value()->is_exact_silence);
        QCOMPARE(*evidence.value()->per_channel_db_tp[1-swap],
                 20.0 * std::log10(2.0 * 7964.0 / 8192.0));
    }
}

void TruePeakFullResourceTest::silenceNullDbAndDcNoCompensation()
{
    const std::vector<double> zero{0.0, -0.0, 0.0, 0.0};
    auto silent = make_pcm(44100, ChannelLayout::STEREO_LR, zero, zero);
    QVERIFY(silent);
    auto measured = measure_unqualified_true_peak_full_resource(*silent.value());
    QVERIFY(measured);
    QCOMPARE(measured.value()->global_linear_peak, 0.0);
    QVERIFY(measured.value()->is_exact_silence);
    QVERIFY(!measured.value()->per_channel_db_tp[0].has_value());
    QVERIFY(!measured.value()->per_channel_db_tp[1].has_value());
    QVERIFY(!measured.value()->global_db_tp.has_value());

    // A two-frame finite DC step includes zero-boundary FIR ringing: unlike
    // an infinite steady DC line, the peak is 10198/8192, not exactly 1.
    const std::vector<double> dc{1.0, 1.0};
    auto buffer = make_pcm(48000, ChannelLayout::MONO_C, dc);
    QVERIFY(buffer);
    auto evidence = measure_unqualified_true_peak_full_resource(*buffer.value());
    QVERIFY(evidence);
    QCOMPARE(evidence.value()->global_linear_peak, 10198.0 / 8192.0);
    QVERIFY(evidence.value()->global_linear_peak > 1.0);
    auto pristine = buffer.value()->view().channel(0);
    QVERIFY(pristine);
    QCOMPARE((*pristine.value())[0], 1.0);
    QCOMPARE((*pristine.value())[1], 1.0);
}

void TruePeakFullResourceTest::rejectsLateNonfiniteAndUnsupportedWithoutSourceMutation()
{
    const std::vector<double> good{0.1, 0.2, 0.3};
    const std::vector<double> bad{
        0.1, 0.2, std::numeric_limits<double>::quiet_NaN()};
    auto buffer = make_pcm(48000, ChannelLayout::STEREO_LR, good, bad);
    QVERIFY(buffer);
    auto attempted = measure_unqualified_true_peak_full_resource(*buffer.value());
    QVERIFY(!attempted);
    QCOMPARE(attempted.error()->code(), core::ErrorCode::InvalidAudioSample);
    auto left = buffer.value()->view().channel(0);
    auto right = buffer.value()->view().channel(1);
    QVERIFY(left && right);
    QCOMPARE((*left.value())[2], 0.3);
    QVERIFY(std::isnan((*right.value())[2]));

    const std::vector<double> one{1.0};
    auto unsupported = make_pcm(32000, ChannelLayout::MONO_C, one);
    QVERIFY(unsupported);
    auto rejected = measure_unqualified_true_peak_full_resource(*unsupported.value());
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(), core::ErrorCode::UnsupportedOperation);

    const std::vector<double> empty;
    auto noFrames = make_pcm(48000, ChannelLayout::MONO_C, empty);
    QVERIFY(noFrames);
    auto emptyStatus = measure_unqualified_true_peak_full_resource(*noFrames.value());
    QVERIFY(!emptyStatus);
    QCOMPARE(emptyStatus.error()->code(), core::ErrorCode::InvalidState);

    // Finite > 1 input and its positive dBTP are not silently clamped.
    const std::vector<double> high{2.0};
    auto large = make_pcm(96000, ChannelLayout::MONO_C, high);
    QVERIFY(large);
    auto elevated = measure_unqualified_true_peak_full_resource(*large.value());
    QVERIFY(elevated);
    QCOMPARE(elevated.value()->global_linear_peak, 2.0 * 7964.0 / 8192.0);
    QVERIFY(*elevated.value()->global_db_tp > 0.0);
}

void TruePeakFullResourceTest::fullResource4096ChunkBoundaryParity()
{
    using analysis::internal::TruePeakItu4xChannelStream;
    constexpr std::array<std::int64_t, 6> rates{
        44100, 48000, 88200, 96000, 176400, 192000
    };
    // Exercise the exact boundary where the full-resource adapter switches
    // chunk, plus adjacent frames and multiple-chunk finalization.
    constexpr std::array<std::size_t, 4> lengths{
        4095, 4096, 4097, 8193
    };
    for (std::int64_t hz : rates) {
        for (std::size_t n : lengths) {
            std::vector<double> left(n), right(n);
            std::uint32_t sl = 0x4D313600U, sr = 0x4D313601U;
            for (std::size_t i = 0; i < n; ++i) {
                sl = sl * 1664525U + 1013904223U;
                sr = sr * 1664525U + 1013904223U;
                left[i] = static_cast<double>(
                    static_cast<std::int32_t>((sl >> 24) & 0xffU) - 128) / 256.0;
                right[i] = static_cast<double>(
                    static_cast<std::int32_t>((sr >> 24) & 0xffU) - 128) / 256.0;
            }
            // A final-frame >1 finite PCM catches premature termination.
            left[n - 1] = 2.0;
            right[n - 1] = -1.5;

            for (int stereo = 0; stereo <= 1; ++stereo) {
                const auto layout = stereo
                    ? ChannelLayout::STEREO_LR : ChannelLayout::MONO_C;
                auto resource = make_pcm(hz, layout, left, stereo
                    ? std::span<const double>{right}
                    : std::span<const double>{});
                QVERIFY(resource);
                auto measured = measure_unqualified_true_peak_full_resource(
                    *resource.value());
                QVERIFY(measured);
                QCOMPARE(measured.value()->sample_rate_hz, hz);
                QCOMPARE(measured.value()->channel_count,
                         static_cast<std::size_t>(stereo ? 2 : 1));

                // Independent SCALAR history progression is a same-build
                // reference for the new per-channel 4096-frame chunk path.
                // An independent lag-major integer oracle already validates
                // those signed phase maxima in B1e's separate test target.
                double maximum = 0.0;
                for (std::size_t ch = 0; ch < measured.value()->channel_count;
                     ++ch) {
                    const auto& pcm = ch == 0 ? left : right;
                    TruePeakItu4xChannelStream scalar;
                    for (double x : pcm)
                        QVERIFY(scalar.push(x));
                    auto expectedPeak = scalar.finalize();
                    QVERIFY(expectedPeak);
                    QCOMPARE(measured.value()->per_channel_linear_peak[ch],
                             *expectedPeak.value());
                    QCOMPARE(*measured.value()->per_channel_db_tp[ch],
                             20.0 * std::log10(*expectedPeak.value()));
                    maximum = std::max(maximum, *expectedPeak.value());
                }
                QCOMPARE(measured.value()->global_linear_peak, maximum);
                QCOMPARE(*measured.value()->global_db_tp,
                         20.0 * std::log10(maximum));
                QVERIFY(!measured.value()->is_exact_silence);
                auto preservedL = resource.value()->view().channel(0);
                QVERIFY(preservedL);
                QCOMPARE((*preservedL.value())[n - 1], 2.0);
                if (stereo) {
                    auto preservedR = resource.value()->view().channel(1);
                    QVERIFY(preservedR);
                    QCOMPARE((*preservedR.value())[n - 1], -1.5);
                }
            }
        }
    }

    // Reject an invalid PCM near the second chunk, never expose partial
    // result evidence, and never alter the input resource.
    std::vector<double> left(4098, 0.125);
    std::vector<double> right(4098, -0.25);
    right[4097] = std::numeric_limits<double>::quiet_NaN();
    auto invalidResource = make_pcm(
        48000, ChannelLayout::STEREO_LR, left, right);
    QVERIFY(invalidResource);
    auto rejected = measure_unqualified_true_peak_full_resource(
        *invalidResource.value());
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(), core::ErrorCode::InvalidAudioSample);
    auto preserved = invalidResource.value()->view().channel(1);
    QVERIFY(preserved);
    QVERIFY(std::isnan((*preserved.value())[4097]));
    QCOMPARE((*preserved.value())[4096], -0.25);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakFullResourceTest)
#include "test_true_peak_full_resource.moc"
