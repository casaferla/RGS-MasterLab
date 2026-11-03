#include <rgsml/analysis/true_peak_measurement.hpp>

#include "internal/true_peak_full_resource.hpp"

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
#include <string_view>
#include <type_traits>
#include <vector>

namespace rgsml::tests {
namespace {

using analysis::TruePeakMeasurement;
using analysis::TruePeakPeakChannel;
using analysis::TruePeakQualificationStatus;
using analysis::measure_true_peak_full_resource;
using analysis::internal::measure_unqualified_true_peak_full_resource;
using audio::ChannelLayout;

static_assert(!std::is_default_constructible_v<TruePeakMeasurement>);

[[nodiscard]] core::Result<audio::AudioBuffer> make_pcm(
    std::int64_t hz, ChannelLayout layout,
    std::span<const double> left,
    std::span<const double> right = {})
{
    auto rate = core::SampleRate::create(hz);
    if (!rate) return core::Result<audio::AudioBuffer>::failure(*rate.error());
    auto fmt = audio::AudioFormat::create(*rate.value(), layout);
    if (!fmt) return core::Result<audio::AudioBuffer>::failure(*fmt.error());
    auto frames = core::FrameCount::create(static_cast<std::int64_t>(left.size()));
    if (!frames) return core::Result<audio::AudioBuffer>::failure(*frames.error());
    if (layout == ChannelLayout::STEREO_LR && left.size() != right.size()) {
        return core::Result<audio::AudioBuffer>::failure(
            core::Error{core::ErrorCode::InvalidArgument, "Unequal stereo lengths."});
    }
    auto pcm = audio::AudioBuffer::create(
        *fmt.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, *frames.value());
    if (!pcm) return pcm;
    auto view = pcm.value()->mutable_view();
    auto l = view.channel(0);
    if (!l) return core::Result<audio::AudioBuffer>::failure(*l.error());
    std::copy(left.begin(), left.end(), l.value()->begin());
    if (layout == ChannelLayout::STEREO_LR) {
        auto rr = view.channel(1);
        if (!rr) return core::Result<audio::AudioBuffer>::failure(*rr.error());
        std::copy(right.begin(), right.end(), rr.value()->begin());
    }
    return pcm;
}

class TruePeakPublicResultTest final : public QObject {
    Q_OBJECT
private slots:
    void sixRatesBothLayoutsParityAndIdentity();
    void exactWinnerTieAndSilence();
    void invalidSourceAndNoQualificationPromotion();
};

void TruePeakPublicResultTest::sixRatesBothLayoutsParityAndIdentity()
{
    constexpr std::array<std::int64_t, 6> rates{
        44100, 48000, 88200, 96000, 176400, 192000};
    const std::vector<double> l{0.25, 1.0, 0.0, -0.125};
    const std::vector<double> r{-0.5, -2.0, 0.125, 0.75};
    for (std::int64_t hz : rates) {
        for (bool stereo : {false, true}) {
            auto pcm = make_pcm(hz, stereo ? ChannelLayout::STEREO_LR
                                           : ChannelLayout::MONO_C,
                                l, stereo ? std::span<const double>{r}
                                          : std::span<const double>{});
            QVERIFY(pcm);
            auto expected = measure_unqualified_true_peak_full_resource(*pcm.value());
            auto got = measure_true_peak_full_resource(*pcm.value());
            QVERIFY(expected && got);
            const auto& m = *got.value();
            QCOMPARE(m.sample_rate_hz(), hz);
            QCOMPARE(m.channel_count(), stereo ? std::size_t{2} : std::size_t{1});
            QVERIFY(m.algorithm_version() == std::string_view{"1.0.0"});
            QVERIFY(m.true_peak_policy_id()
                    == std::string_view{"rgsml.analysis.true-peak.itu4x/1.0.0"});
            QVERIFY(m.result_schema_id()
                    == std::string_view{"rgsml.analysis.true-peak.result/1.0.0"});
            QVERIFY(m.filter_asset_sha256() == std::string_view{
                "D1DF3373D3C5A82A3E4CFE3576BDDF97E153F3281FF9287554E024FA6CB0D316"});
            QCOMPARE(m.qualification_status(),
                     TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
            QCOMPARE(m.global_linear_peak(), expected.value()->global_linear_peak);
            QVERIFY(m.global_db_tp() == expected.value()->global_db_tp);
            QCOMPARE(m.is_exact_silence(), expected.value()->is_exact_silence);
            QCOMPARE(m.per_channel_linear_peak().size(), m.channel_count());
            QCOMPARE(m.per_channel_db_tp().size(), m.channel_count());
            for (std::size_t ch = 0; ch < m.channel_count(); ++ch) {
                QCOMPARE(m.per_channel_linear_peak()[ch],
                         expected.value()->per_channel_linear_peak[ch]);
                QVERIFY(m.per_channel_db_tp()[ch]
                        == expected.value()->per_channel_db_tp[ch]);
            }
            QCOMPARE(m.global_peak_channel(),
                     stereo ? TruePeakPeakChannel::RIGHT : TruePeakPeakChannel::MONO_C);
            auto source = pcm.value()->view().channel(0);
            QVERIFY(source);
            QCOMPARE((*source.value())[1], 1.0);
            if (stereo) {
                auto right = pcm.value()->view().channel(1);
                QVERIFY(right);
                QCOMPARE((*right.value())[1], -2.0);
            }
        }
    }
}

void TruePeakPublicResultTest::exactWinnerTieAndSilence()
{
    const std::vector<double> one{1.0};
    const std::vector<double> two{2.0};
    const std::vector<double> zero{0.0, -0.0, 0.0};
    auto left = make_pcm(48000, ChannelLayout::STEREO_LR, two, one);
    QVERIFY(left);
    auto a = measure_true_peak_full_resource(*left.value());
    QVERIFY(a);
    QCOMPARE(a.value()->global_peak_channel(), TruePeakPeakChannel::LEFT);
    auto tie = make_pcm(48000, ChannelLayout::STEREO_LR, one, one);
    QVERIFY(tie);
    auto b = measure_true_peak_full_resource(*tie.value());
    QVERIFY(b);
    QCOMPARE(b.value()->global_peak_channel(), TruePeakPeakChannel::TIE);
    auto silence = make_pcm(48000, ChannelLayout::STEREO_LR, zero, zero);
    QVERIFY(silence);
    auto c = measure_true_peak_full_resource(*silence.value());
    QVERIFY(c);
    QCOMPARE(c.value()->global_peak_channel(), TruePeakPeakChannel::NONE);
    QCOMPARE(c.value()->global_linear_peak(), 0.0);
    QVERIFY(c.value()->is_exact_silence());
    QVERIFY(!c.value()->global_db_tp().has_value());
    for (const auto& d : c.value()->per_channel_db_tp()) QVERIFY(!d.has_value());
    auto mono = make_pcm(96000, ChannelLayout::MONO_C, one);
    QVERIFY(mono);
    auto d = measure_true_peak_full_resource(*mono.value());
    QVERIFY(d);
    QCOMPARE(d.value()->global_peak_channel(), TruePeakPeakChannel::MONO_C);
    QCOMPARE(d.value()->per_channel_linear_peak().size(), std::size_t{1});
}

void TruePeakPublicResultTest::invalidSourceAndNoQualificationPromotion()
{
    const std::vector<double> one{1.0};
    const std::vector<double> empty;
    auto unsupported = make_pcm(32000, ChannelLayout::MONO_C, one);
    QVERIFY(unsupported);
    auto unsupportedResult = measure_true_peak_full_resource(*unsupported.value());
    QVERIFY(!unsupportedResult);
    QCOMPARE(unsupportedResult.error()->code(), core::ErrorCode::UnsupportedOperation);
    auto noFrames = make_pcm(48000, ChannelLayout::MONO_C, empty);
    QVERIFY(noFrames);
    auto emptyResult = measure_true_peak_full_resource(*noFrames.value());
    QVERIFY(!emptyResult);
    QCOMPARE(emptyResult.error()->code(), core::ErrorCode::InvalidState);
    const std::vector<double> bad{
        0.25, -2.0, std::numeric_limits<double>::quiet_NaN()};
    auto invalid = make_pcm(48000, ChannelLayout::MONO_C, bad);
    QVERIFY(invalid);
    auto badResult = measure_true_peak_full_resource(*invalid.value());
    QVERIFY(!badResult);
    QCOMPARE(badResult.error()->code(), core::ErrorCode::InvalidAudioSample);
    auto pristine = invalid.value()->view().channel(0);
    QVERIFY(pristine);
    QCOMPARE((*pristine.value())[1], -2.0);
    QVERIFY(std::isnan((*pristine.value())[2]));
    auto high = make_pcm(96000, ChannelLayout::MONO_C, std::vector<double>{2.0});
    QVERIFY(high);
    auto measured = measure_true_peak_full_resource(*high.value());
    QVERIFY(measured);
    QVERIFY(measured.value()->global_linear_peak() > 1.0);
    QVERIFY(measured.value()->global_db_tp().has_value());
    QVERIFY(*measured.value()->global_db_tp() > 0.0);
    QCOMPARE(measured.value()->qualification_status(),
             TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakPublicResultTest)
#include "test_true_peak_measurement.moc"
