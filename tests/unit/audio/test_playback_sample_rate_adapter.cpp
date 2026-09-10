#include "wav_test_support.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>

#include <QTest>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace wav_support;

[[nodiscard]] core::SampleRate rate(std::int64_t value)
{
    auto result = core::SampleRate::create(value);
    Q_ASSERT(result);
    return *result.value();
}

[[nodiscard]] std::unique_ptr<audio::WavReader> open_f64_stereo(
    std::uint32_t sampleRate,
    std::span<const std::uint64_t> interleaved,
    const std::shared_ptr<ReaderControl>& control =
        std::make_shared<ReaderControl>())
{
    auto bytes = make_wav(
        3U, 64U, 2U, sampleRate, f64_payload(interleaved));
    auto reader = audio::WavReader::open(memory_reader(std::move(bytes), control));
    Q_ASSERT(reader);
    return std::move(*reader.value());
}

[[nodiscard]] audio::PlaybackSampleRateAdapter adapter(
    std::int64_t inputRate,
    std::int64_t outputRate,
    std::int64_t inputFrames)
{
    auto result = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{
            rate(inputRate),
            rate(outputRate),
            audio::ChannelLayout::STEREO_LR,
            frame_count(inputFrames),
        });
    Q_ASSERT(result);
    return std::move(*result.value());
}

[[nodiscard]] audio::AudioBuffer output_buffer(
    const audio::PlaybackSampleRateAdapter& converter,
    std::int64_t start,
    std::int64_t frames)
{
    auto format = audio::AudioFormat::create(
        converter.output_rate(), converter.channel_layout());
    Q_ASSERT(format);
    auto buffer = audio::AudioBuffer::create(
        *format.value(),
        audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{start},
        frame_count(frames));
    Q_ASSERT(buffer);
    return std::move(*buffer.value());
}

[[nodiscard]] std::vector<std::uint64_t> render_partitioned(
    audio::PlaybackSampleRateAdapter& converter,
    audio::WavReader& reader,
    std::span<const std::int64_t> partition,
    std::size_t channel)
{
    std::vector<std::uint64_t> result;
    std::int64_t start = 0;
    for (const auto frames : partition) {
        auto block = output_buffer(converter, start, frames);
        auto rendered = converter.read_frames(
            reader, core::FrameIndex{start}, block.mutable_view());
        Q_ASSERT(rendered);
        auto bits = channel_bits(block, channel);
        result.insert(result.end(), bits.begin(), bits.end());
        start += frames;
    }
    return result;
}

}  // namespace

class PlaybackSampleRateAdapterTest final : public QObject {
    Q_OBJECT

private slots:
    void policyIdentityAndFrameMap();
    void assetsAreFrozenAndSymmetric();
    void constantAndChunkInvariance();
    void reverseDirectionAndAbsoluteSeekInvariance();
    void oneFrameBoundaryAndStereoIndependence();
    void decodeWindowIsTrackDurationIndependent();
};

void PlaybackSampleRateAdapterTest::policyIdentityAndFrameMap()
{
    auto identity = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{
            rate(44'100),
            rate(44'100),
            audio::ChannelLayout::STEREO_LR,
            frame_count(441),
        });
    QVERIFY(!identity);
    QCOMPARE(identity.error()->code(), core::ErrorCode::UnsupportedOperation);
    QCOMPARE(
        QString::fromStdString(identity.error()->message()),
        QStringLiteral("PLAYBACK_RATE_PAIR_UNSUPPORTED_V1"));

    auto unsupported = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{
            rate(96'000),
            rate(48'000),
            audio::ChannelLayout::STEREO_LR,
            frame_count(960),
        });
    QVERIFY(!unsupported);
    QCOMPARE(unsupported.error()->code(), core::ErrorCode::UnsupportedOperation);

    auto forward = adapter(44'100, 48'000, 441);
    QCOMPARE(forward.interpolation_factor(), std::int64_t{160});
    QCOMPARE(forward.decimation_factor(), std::int64_t{147});
    QCOMPARE(forward.output_frame_count().value(), std::int64_t{480});
    QCOMPARE(
        forward.map_input_frame_to_output(core::FrameIndex{1}).value()->value(),
        std::int64_t{2});
    QCOMPARE(
        forward.map_output_frame_to_input_cursor(core::FrameIndex{160})
            .value()
            ->value(),
        std::int64_t{147});

    auto interval = core::FrameRange::create(
        core::FrameIndex{10}, core::FrameIndex{20});
    QVERIFY(interval);
    auto mapped = forward.map_input_range_to_output(*interval.value());
    QVERIFY(mapped);
    QCOMPARE(mapped.value()->begin().value(), std::int64_t{11});
    QCOMPARE(mapped.value()->end().value(), std::int64_t{22});

    auto reverse = adapter(48'000, 44'100, 480);
    QCOMPARE(reverse.interpolation_factor(), std::int64_t{147});
    QCOMPARE(reverse.decimation_factor(), std::int64_t{160});
    QCOMPARE(reverse.output_frame_count().value(), std::int64_t{441});
}

void PlaybackSampleRateAdapterTest::assetsAreFrozenAndSymmetric()
{
    auto forward = adapter(44'100, 48'000, 10);
    QCOMPARE(forward.kernel_size(), std::size_t{30'721});
    QCOMPARE(forward.group_delay_high_rate_frames(), std::int64_t{15'360});
    QCOMPARE(
        QString::fromUtf8(forward.kernel_sha256()),
        QStringLiteral(
            "ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4"));
    for (std::size_t index = 0; index < forward.kernel_size(); ++index) {
        QCOMPARE(
            forward.coefficient_bits(index),
            forward.coefficient_bits(forward.kernel_size() - 1U - index));
    }

    auto reverse = adapter(48'000, 44'100, 10);
    QCOMPARE(reverse.kernel_size(), std::size_t{28'225});
    QCOMPARE(reverse.group_delay_high_rate_frames(), std::int64_t{14'112});
    QCOMPARE(
        QString::fromUtf8(reverse.kernel_sha256()),
        QStringLiteral(
            "37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22"));
    for (std::size_t index = 0; index < reverse.kernel_size(); ++index) {
        QCOMPARE(
            reverse.coefficient_bits(index),
            reverse.coefficient_bits(reverse.kernel_size() - 1U - index));
    }
}

void PlaybackSampleRateAdapterTest::constantAndChunkInvariance()
{
    constexpr std::int64_t kInputFrames = 900;
    std::vector<std::uint64_t> interleaved;
    interleaved.reserve(static_cast<std::size_t>(kInputFrames) * 2U);
    for (std::int64_t frame = 0; frame < kInputFrames; ++frame) {
        interleaved.push_back(std::bit_cast<std::uint64_t>(0.25));
        interleaved.push_back(std::bit_cast<std::uint64_t>(-0.5));
    }

    auto control = std::make_shared<ReaderControl>();
    auto wholeReader = open_f64_stereo(44'100, interleaved, control);
    auto converter = adapter(44'100, 48'000, kInputFrames);
    const auto outputFrames = converter.output_frame_count().value();
    QVERIFY(outputFrames <= audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    auto whole = output_buffer(converter, 0, outputFrames);
    QVERIFY(converter.read_frames(
        *wholeReader, core::FrameIndex{0}, whole.mutable_view()));
    const auto wholeLeft = channel_bits(whole, 0);
    const auto wholeRight = channel_bits(whole, 1);

    const std::vector<std::int64_t> partition{
        1, 17, 257, outputFrames - 275};
    auto partitionReader = open_f64_stereo(44'100, interleaved);
    const auto partitionLeft = render_partitioned(
        converter, *partitionReader, partition, 0);
    auto secondPartitionReader = open_f64_stereo(44'100, interleaved);
    const auto partitionRight = render_partitioned(
        converter, *secondPartitionReader, partition, 1);
    QCOMPARE(partitionLeft, wholeLeft);
    QCOMPARE(partitionRight, wholeRight);
    QVERIFY(control->largestReadRequest <= 4096U);

    auto left = whole.view().channel(0);
    auto right = whole.view().channel(1);
    QVERIFY(left);
    QVERIFY(right);
    for (std::size_t frame = 0; frame < left.value()->size(); ++frame) {
        QVERIFY(std::abs((*left.value())[frame] - 0.25) < 2.0e-6);
        QVERIFY(std::abs((*right.value())[frame] + 0.5) < 4.0e-6);
    }
}

void PlaybackSampleRateAdapterTest::oneFrameBoundaryAndStereoIndependence()
{
    const std::vector<std::uint64_t> oneFrame{
        std::bit_cast<std::uint64_t>(0.125),
        std::bit_cast<std::uint64_t>(-0.25),
    };
    auto oneReader = open_f64_stereo(48'000, oneFrame);
    auto reverse = adapter(48'000, 44'100, 1);
    QCOMPARE(reverse.output_frame_count().value(), std::int64_t{1});
    auto oneOutput = output_buffer(reverse, 0, 1);
    QVERIFY(reverse.read_frames(
        *oneReader, core::FrameIndex{0}, oneOutput.mutable_view()));
    auto left = oneOutput.view().channel(0);
    auto right = oneOutput.view().channel(1);
    QVERIFY(left);
    QVERIFY(right);
    QVERIFY(std::abs((*left.value())[0] - 0.125) < 2.0e-6);
    QVERIFY(std::abs((*right.value())[0] + 0.25) < 2.0e-6);

    std::vector<std::uint64_t> impulse(64U * 2U, UINT64_C(0));
    impulse[20U] = std::bit_cast<std::uint64_t>(1.0);
    auto impulseReader = open_f64_stereo(44'100, impulse);
    auto forward = adapter(44'100, 48'000, 64);
    auto impulseOutput = output_buffer(
        forward, 0, forward.output_frame_count().value());
    QVERIFY(forward.read_frames(
        *impulseReader, core::FrameIndex{0}, impulseOutput.mutable_view()));
    auto silentRight = impulseOutput.view().channel(1);
    QVERIFY(silentRight);
    for (const auto sample : *silentRight.value()) {
        QCOMPARE(std::bit_cast<std::uint64_t>(sample), UINT64_C(0));
    }
}

void PlaybackSampleRateAdapterTest::reverseDirectionAndAbsoluteSeekInvariance()
{
    constexpr std::int64_t kInputFrames = 1'000;
    std::vector<std::uint64_t> interleaved;
    interleaved.reserve(static_cast<std::size_t>(kInputFrames) * 2U);
    for (std::int64_t frame = 0; frame < kInputFrames; ++frame) {
        const double left = static_cast<double>((frame % 101) - 50) / 100.0;
        const double right = static_cast<double>((frame % 79) - 39) / 100.0;
        interleaved.push_back(std::bit_cast<std::uint64_t>(left));
        interleaved.push_back(std::bit_cast<std::uint64_t>(right));
    }
    auto converter = adapter(48'000, 44'100, kInputFrames);
    const auto outputFrames = converter.output_frame_count().value();
    auto wholeReader = open_f64_stereo(48'000, interleaved);
    auto whole = output_buffer(converter, 0, outputFrames);
    QVERIFY(converter.read_frames(
        *wholeReader, core::FrameIndex{0}, whole.mutable_view()));
    const auto wholeLeft = channel_bits(whole, 0);
    const auto wholeRight = channel_bits(whole, 1);

    const std::vector<std::int64_t> partition{
        3, 127, 509, outputFrames - 639};
    auto leftReader = open_f64_stereo(48'000, interleaved);
    auto rightReader = open_f64_stereo(48'000, interleaved);
    QCOMPARE(
        render_partitioned(converter, *leftReader, partition, 0),
        wholeLeft);
    QCOMPARE(
        render_partitioned(converter, *rightReader, partition, 1),
        wholeRight);

    constexpr std::int64_t kSeekStart = 317;
    constexpr std::int64_t kSeekFrames = 91;
    auto seekReader = open_f64_stereo(48'000, interleaved);
    auto seekBlock = output_buffer(converter, kSeekStart, kSeekFrames);
    QVERIFY(converter.read_frames(
        *seekReader,
        core::FrameIndex{kSeekStart},
        seekBlock.mutable_view()));
    const auto seekLeft = channel_bits(seekBlock, 0);
    const auto seekRight = channel_bits(seekBlock, 1);
    QCOMPARE(
        seekLeft,
        std::vector<std::uint64_t>(
            wholeLeft.begin() + kSeekStart,
            wholeLeft.begin() + kSeekStart + kSeekFrames));
    QCOMPARE(
        seekRight,
        std::vector<std::uint64_t>(
            wholeRight.begin() + kSeekStart,
            wholeRight.begin() + kSeekStart + kSeekFrames));
}

void PlaybackSampleRateAdapterTest::decodeWindowIsTrackDurationIndependent()
{
    constexpr std::int64_t kInputFrames = 100'000;
    std::vector<std::uint64_t> interleaved(
        static_cast<std::size_t>(kInputFrames) * 2U,
        std::bit_cast<std::uint64_t>(0.0));
    auto control = std::make_shared<ReaderControl>();
    auto reader = open_f64_stereo(44'100, interleaved, control);
    auto converter = adapter(44'100, 48'000, kInputFrames);
    constexpr std::int64_t kOutputStart = 50'000;
    auto block = output_buffer(
        converter,
        kOutputStart,
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    QVERIFY(converter.read_frames(
        *reader, core::FrameIndex{kOutputStart}, block.mutable_view()));
    QVERIFY(control->totalBytesRead < 50'000U);
    QVERIFY(control->largestReadRequest <= 4096U);

    auto oversized = output_buffer(
        converter,
        0,
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames + 1);
    auto rejected = converter.read_frames(
        *reader, core::FrameIndex{0}, oversized.mutable_view());
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(), core::ErrorCode::InvalidArgument);
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::PlaybackSampleRateAdapterTest)

#include "test_playback_sample_rate_adapter.moc"
