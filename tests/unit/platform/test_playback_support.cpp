#include "playback_support.hpp"

#include "../../audio_golden/wav/golden_vectors.hpp"
#include "../audio/wav_test_support.hpp"

#include <rgsml/audio/audio_buffer.hpp>

#include <QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace wav_support;
using platform::windows::internal::DeviceSampleFormat;
using platform::windows::internal::IPlaybackOutput;
using platform::windows::internal::OutputState;
using platform::windows::internal::PlaybackEngine;

[[nodiscard]] audio::AudioBuffer make_buffer(
    std::int64_t rateValue,
    audio::ChannelLayout layout,
    std::int64_t frames)
{
    auto rate = core::SampleRate::create(rateValue);
    Q_ASSERT(rate);
    auto format = audio::AudioFormat::create(*rate.value(), layout);
    Q_ASSERT(format);
    auto count = core::FrameCount::create(frames);
    Q_ASSERT(count);
    auto buffer = audio::AudioBuffer::create(
        *format.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},
        *count.value());
    Q_ASSERT(buffer);
    return std::move(*buffer.value());
}

[[nodiscard]] std::uint16_t read_u16(
    const std::vector<std::byte>& bytes,
    std::size_t offset)
{
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset]))
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(bytes[offset + 1U]))
            << 8U);
}

class FakeOutput final : public IPlaybackOutput {
public:
    explicit FakeOutput(
        std::size_t capacity,
        std::size_t bytesPerFrame,
        std::size_t maximumWrite = std::numeric_limits<std::size_t>::max())
        : capacity_(capacity)
        , bytesPerFrame_(bytesPerFrame)
        , maximumWrite_(maximumWrite)
    {
    }

    [[nodiscard]] std::size_t writable_bytes() const noexcept override
    {
        return capacity_ - queue_.size();
    }

    [[nodiscard]] std::size_t queued_bytes() const noexcept override
    {
        return queue_.size();
    }

    [[nodiscard]] core::Result<std::size_t> enqueue(
        std::span<const std::byte> bytes) override
    {
        if (injectedError_) {
            return core::Result<std::size_t>::failure(*injectedError_);
        }
        const auto accepted = std::min(
            {bytes.size(), writable_bytes(), maximumWrite_});
        queue_.insert(queue_.end(), bytes.begin(), bytes.begin() + accepted);
        history_.insert(history_.end(), bytes.begin(), bytes.begin() + accepted);
        maximumObserved_ = std::max(maximumObserved_, queue_.size());
        return core::Result<std::size_t>::success(accepted);
    }

    void clear_queue() noexcept override
    {
        queue_.clear();
    }

    [[nodiscard]] core::Status start() override
    {
        ++startCalls;
        state_ = OutputState::ACTIVE;
        processedFrames_ = 0;
        return core::Status::success();
    }

    [[nodiscard]] core::Status suspend() override
    {
        ++suspendCalls;
        state_ = OutputState::SUSPENDED;
        return core::Status::success();
    }

    [[nodiscard]] core::Status resume() override
    {
        ++resumeCalls;
        state_ = OutputState::ACTIVE;
        return core::Status::success();
    }

    [[nodiscard]] core::Status stop() override
    {
        ++stopCalls;
        state_ = OutputState::STOPPED;
        queue_.clear();
        processedFrames_ = 0;
        return core::Status::success();
    }

    [[nodiscard]] std::int64_t processed_frames() const noexcept override
    {
        return processedFrames_;
    }

    [[nodiscard]] OutputState state() const noexcept override
    {
        return state_;
    }

    [[nodiscard]] std::optional<core::Error> error() const override
    {
        return injectedError_;
    }

    void consume_all()
    {
        processedFrames_ += static_cast<std::int64_t>(
            queue_.size() / bytesPerFrame_);
        queue_.clear();
        state_ = OutputState::IDLE;
    }

    void set_processed_frames(std::int64_t frames)
    {
        processedFrames_ = frames;
    }

    void inject_error(core::Error error)
    {
        injectedError_ = std::move(error);
        state_ = OutputState::ERROR;
    }

    const std::vector<std::byte>& history() const noexcept
    {
        return history_;
    }

    std::size_t maximum_observed() const noexcept
    {
        return maximumObserved_;
    }

    int startCalls{0};
    int suspendCalls{0};
    int resumeCalls{0};
    int stopCalls{0};

private:
    std::size_t capacity_;
    std::size_t bytesPerFrame_;
    std::size_t maximumWrite_;
    std::vector<std::byte> queue_;
    std::vector<std::byte> history_;
    std::size_t maximumObserved_{0U};
    std::int64_t processedFrames_{0};
    OutputState state_{OutputState::STOPPED};
    std::optional<core::Error> injectedError_;
};

[[nodiscard]] std::unique_ptr<audio::WavReader> open_pcm16_stereo(
    const std::vector<std::int64_t>& codes,
    std::shared_ptr<ReaderControl> control = std::make_shared<ReaderControl>(),
    std::uint32_t sampleRate = 48'000U)
{
    auto bytes = make_wav(
        1U, 16U, 2U, sampleRate, pcm_payload(codes, 16U));
    auto reader = audio::WavReader::open(memory_reader(std::move(bytes), control));
    Q_ASSERT(reader);
    return std::move(*reader.value());
}

}  // namespace

class PlaybackSupportTest final : public QObject {
    Q_OBJECT

private slots:
    void formatSelectionIsDeterministic();
    void floatAndPcm16GoldenConversion();
    void conversionFailuresAndChunkInvariance();
    void stateMachineAndBoundedPump();
    void adaptedTimelineUsesAbsoluteRateMapping();
    void partialWritesNaturalEofAndRuntimeError();
};

void PlaybackSupportTest::formatSelectionIsDeterministic()
{
    auto rate = core::SampleRate::create(48000);
    QVERIFY(rate);
    auto format = audio::AudioFormat::create(
        *rate.value(), audio::ChannelLayout::STEREO_LR);
    QVERIFY(format);

    auto both = platform::windows::internal::select_device_format(
        *format.value(), true, true, true, true);
    QVERIFY(both);
    QCOMPARE(both.value()->sampleFormat, DeviceSampleFormat::IEEE_F32);
    QCOMPARE(both.value()->sampleRateHz, 48000);
    QCOMPARE(both.value()->channelCount, std::size_t{2});
    QVERIFY(!both.value()->srcApplied);

    auto fallback = platform::windows::internal::select_device_format(
        *format.value(), false, true, true, true);
    QVERIFY(fallback);
    QCOMPARE(fallback.value()->sampleFormat, DeviceSampleFormat::PCM_S16);
    QVERIFY(!fallback.value()->srcApplied);

    auto pairedFloat = platform::windows::internal::select_device_format(
        *format.value(), false, false, true, true);
    QVERIFY(pairedFloat);
    QCOMPARE(pairedFloat.value()->sampleFormat, DeviceSampleFormat::IEEE_F32);
    QCOMPARE(pairedFloat.value()->sampleRateHz, 44'100);
    QVERIFY(pairedFloat.value()->srcApplied);

    auto pairedPcm = platform::windows::internal::select_device_format(
        *format.value(), false, false, false, true);
    QVERIFY(pairedPcm);
    QCOMPARE(pairedPcm.value()->sampleFormat, DeviceSampleFormat::PCM_S16);
    QVERIFY(pairedPcm.value()->srcApplied);

    auto unsupported = platform::windows::internal::select_device_format(
        *format.value(), false, false, false, false);
    QVERIFY(!unsupported);
    QCOMPARE(unsupported.error()->code(), core::ErrorCode::UnsupportedOperation);
}

void PlaybackSupportTest::adaptedTimelineUsesAbsoluteRateMapping()
{
    std::vector<std::int64_t> codes;
    codes.reserve(441U * 2U);
    for (std::int64_t frame = 0; frame < 441; ++frame) {
        codes.push_back(frame % 100);
        codes.push_back(-(frame % 100));
    }
    auto inputRate = core::SampleRate::create(44'100);
    auto outputRate = core::SampleRate::create(48'000);
    QVERIFY(inputRate);
    QVERIFY(outputRate);
    auto converter = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{
            *inputRate.value(),
            *outputRate.value(),
            audio::ChannelLayout::STEREO_LR,
            frame_count(441),
        });
    QVERIFY(converter);

    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(64U * 1024U, 4U);
    auto* observed = output.get();
    QVERIFY(engine.install_candidate(
        open_pcm16_stereo(codes, std::make_shared<ReaderControl>(), 44'100U),
        std::move(output),
        DeviceSampleFormat::PCM_S16,
        std::move(*converter.value())));
    QVERIFY(engine.play());
    observed->set_processed_frames(160);
    engine.tick();
    auto snapshot = engine.snapshot();
    QVERIFY(snapshot);
    QCOMPARE(snapshot.value()->position.value(), std::int64_t{147});
    QCOMPARE(snapshot.value()->duration->value(), std::int64_t{441});

    QVERIFY(engine.seek(core::FrameIndex{220}));
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{220});
    QVERIFY(engine.stop());
}

void PlaybackSupportTest::floatAndPcm16GoldenConversion()
{
    auto floatBuffer = make_buffer(
        48000, audio::ChannelLayout::STEREO_LR, 3);
    auto mutableFloat = floatBuffer.mutable_view();
    auto left = mutableFloat.channel(0);
    auto right = mutableFloat.channel(1);
    QVERIFY(left);
    QVERIFY(right);
    (*left.value())[0] = -0.0;
    (*left.value())[1] = 1.0;
    (*left.value())[2] = -1.0;
    (*right.value())[0] = 0.0;
    (*right.value())[1] = 0.5;
    (*right.value())[2] = -0.5;
    const std::array<std::uint64_t, 6> before{
        std::bit_cast<std::uint64_t>((*left.value())[0]),
        std::bit_cast<std::uint64_t>((*left.value())[1]),
        std::bit_cast<std::uint64_t>((*left.value())[2]),
        std::bit_cast<std::uint64_t>((*right.value())[0]),
        std::bit_cast<std::uint64_t>((*right.value())[1]),
        std::bit_cast<std::uint64_t>((*right.value())[2]),
    };

    auto floatBytes = platform::windows::internal::encode_device_block(
        floatBuffer.view(), DeviceSampleFormat::IEEE_F32);
    QVERIFY(floatBytes);
    const std::array<std::uint32_t, 6> expected{
        0x80000000U, 0x00000000U,
        0x3f800000U, 0x3f000000U,
        0xbf800000U, 0xbf000000U,
    };
    QCOMPARE(floatBytes.value()->size(), expected.size() * sizeof(std::uint32_t));
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto offset = index * 4U;
        const std::uint32_t actual =
            static_cast<std::uint32_t>(
                std::to_integer<std::uint8_t>((*floatBytes.value())[offset]))
            | (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>((*floatBytes.value())[offset + 1U]))
                << 8U)
            | (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>((*floatBytes.value())[offset + 2U]))
                << 16U)
            | (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>((*floatBytes.value())[offset + 3U]))
                << 24U);
        QCOMPARE(actual, expected[index]);
    }
    QCOMPARE(
        std::bit_cast<std::uint64_t>((*left.value())[0]), before[0]);
    QCOMPARE(
        std::bit_cast<std::uint64_t>((*right.value())[0]), before[3]);

    auto pcmBuffer = make_buffer(48000, audio::ChannelLayout::MONO_C, 6);
    auto pcmPlane = pcmBuffer.mutable_view().channel(0);
    QVERIFY(pcmPlane);
    const std::array<double, 6> samples{
        -1.0,
        1.0,
        0.5 / 32768.0,
        1.5 / 32768.0,
        -0.5 / 32768.0,
        -1.5 / 32768.0,
    };
    std::copy(samples.begin(), samples.end(), pcmPlane.value()->begin());
    auto pcmBytes = platform::windows::internal::encode_device_block(
        pcmBuffer.view(), DeviceSampleFormat::PCM_S16);
    QVERIFY(pcmBytes);
    const std::array<std::int16_t, 6> expectedPcm{
        -32768, 32767, 0, 2, 0, -2};
    for (std::size_t index = 0; index < expectedPcm.size(); ++index) {
        QCOMPARE(
            std::bit_cast<std::int16_t>(read_u16(*pcmBytes.value(), index * 2U)),
            expectedPcm[index]);
    }
}

void PlaybackSupportTest::conversionFailuresAndChunkInvariance()
{
    auto buffer = make_buffer(44100, audio::ChannelLayout::MONO_C, 3);
    auto plane = buffer.mutable_view().channel(0);
    QVERIFY(plane);
    (*plane.value())[0] = -0.0;
    (*plane.value())[1] = 0.25;
    (*plane.value())[2] = -0.75;

    auto full = platform::windows::internal::encode_device_block(
        buffer.view(), DeviceSampleFormat::IEEE_F32);
    QVERIFY(full);
    auto first = buffer.view().subview(
        core::FrameIndex{0}, *core::FrameCount::create(1).value());
    auto tail = buffer.view().subview(
        core::FrameIndex{1}, *core::FrameCount::create(2).value());
    QVERIFY(first);
    QVERIFY(tail);
    auto firstBytes = platform::windows::internal::encode_device_block(
        *first.value(), DeviceSampleFormat::IEEE_F32);
    auto tailBytes = platform::windows::internal::encode_device_block(
        *tail.value(), DeviceSampleFormat::IEEE_F32);
    QVERIFY(firstBytes);
    QVERIFY(tailBytes);
    std::vector<std::byte> partitioned = *firstBytes.value();
    partitioned.insert(
        partitioned.end(), tailBytes.value()->begin(), tailBytes.value()->end());
    QCOMPARE(partitioned, *full.value());

    (*plane.value())[0] = 1.0001;
    auto clipped = platform::windows::internal::encode_device_block(
        buffer.view(), DeviceSampleFormat::PCM_S16);
    QVERIFY(!clipped);
    QCOMPARE(clipped.error()->code(), core::ErrorCode::InvalidAudioSample);

    (*plane.value())[0] = std::numeric_limits<double>::infinity();
    auto nonFinite = platform::windows::internal::encode_device_block(
        buffer.view(), DeviceSampleFormat::IEEE_F32);
    QVERIFY(!nonFinite);
    QCOMPARE(nonFinite.error()->code(), core::ErrorCode::InvalidAudioSample);

    (*plane.value())[0] = std::numeric_limits<double>::max();
    auto floatOverflow = platform::windows::internal::encode_device_block(
        buffer.view(), DeviceSampleFormat::IEEE_F32);
    QVERIFY(!floatOverflow);
    QCOMPARE(floatOverflow.error()->code(), core::ErrorCode::InvalidAudioSample);
}

void PlaybackSupportTest::stateMachineAndBoundedPump()
{
    PlaybackEngine engine;
    auto initial = engine.snapshot();
    QVERIFY(initial);
    QCOMPARE(initial.value()->state, core::PlaybackState::NO_SOURCE);
    QCOMPARE(initial.value()->position.value(), std::int64_t{0});
    QVERIFY(!initial.value()->duration);
    QVERIFY(!engine.play());
    QVERIFY(!engine.pause());
    QVERIFY(!engine.stop());
    QVERIFY(engine.clear());

    std::vector<std::int64_t> codes;
    codes.reserve(8192U);
    for (std::int64_t frame = 0; frame < 4096; ++frame) {
        codes.push_back(frame % 32768);
        codes.push_back(-(frame % 32768));
    }
    auto control = std::make_shared<ReaderControl>();
    auto output = std::make_unique<FakeOutput>(4096U, 4U);
    auto* observed = output.get();
    QVERIFY(engine.install_candidate(
        open_pcm16_stereo(codes, control),
        std::move(output),
        DeviceSampleFormat::PCM_S16));
    auto prepared = engine.snapshot();
    QVERIFY(prepared);
    QCOMPARE(prepared.value()->state, core::PlaybackState::STOPPED);
    QCOMPARE(prepared.value()->duration->value(), std::int64_t{4096});
    QVERIFY(engine.play());
    QVERIFY(observed->queued_bytes() <= 4096U);
    QVERIFY(observed->maximum_observed() <= 4096U);
    QVERIFY(control->largestReadRequest <= 1024U);
    QCOMPARE(observed->startCalls, 1);

    observed->set_processed_frames(100);
    engine.tick();
    auto playing = engine.snapshot();
    QVERIFY(playing);
    QCOMPARE(playing.value()->position.value(), std::int64_t{100});
    QVERIFY(engine.pause());
    QCOMPARE(observed->suspendCalls, 1);
    auto paused = engine.snapshot();
    QCOMPARE(paused.value()->state, core::PlaybackState::PAUSED);
    QVERIFY(engine.play());
    QCOMPARE(observed->resumeCalls, 1);
    QVERIFY(engine.stop());
    auto stopped = engine.snapshot();
    QCOMPARE(stopped.value()->state, core::PlaybackState::STOPPED);
    QCOMPARE(stopped.value()->position.value(), std::int64_t{0});
    QVERIFY(engine.seek(core::FrameIndex{10}));
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{10});
    QVERIFY(!engine.seek(core::FrameIndex{-1}));
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{10});

    auto loop = core::FrameRange::create(
        core::FrameIndex{10}, core::FrameIndex{11});
    QVERIFY(loop);
    QVERIFY(engine.set_loop(*loop.value()));
    QCOMPARE(engine.snapshot().value()->loop, std::optional{*loop.value()});
    QVERIFY(engine.set_loop(std::nullopt));
    QVERIFY(!engine.snapshot().value()->loop);
    QVERIFY(engine.clear());
    QCOMPARE(engine.snapshot().value()->state, core::PlaybackState::NO_SOURCE);
}

void PlaybackSupportTest::partialWritesNaturalEofAndRuntimeError()
{
    const std::vector<std::int64_t> codes{
        -32768, 32767,
        -16384, 16384,
        -8192, 8192,
        0, 1,
    };
    auto readerForExpected = open_pcm16_stereo(codes);
    auto decoded = make_destination(readerForExpected->info(), 0, 4);
    QVERIFY(decoded);
    QVERIFY(readerForExpected->read_frames(
        core::FrameIndex{0}, decoded.value()->mutable_view()));
    auto expected = platform::windows::internal::encode_device_block(
        decoded.value()->view(), DeviceSampleFormat::PCM_S16);
    QVERIFY(expected);

    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(8U, 4U, 3U);
    auto* observed = output.get();
    QVERIFY(engine.install_candidate(
        open_pcm16_stereo(codes),
        std::move(output),
        DeviceSampleFormat::PCM_S16));
    QVERIFY(engine.play());
    for (int iteration = 0; iteration < 16; ++iteration) {
        observed->consume_all();
        engine.tick();
        auto state = engine.snapshot();
        if (state && state.value()->state == core::PlaybackState::STOPPED) {
            break;
        }
    }
    QCOMPARE(observed->history(), *expected.value());
    auto ended = engine.snapshot();
    QVERIFY(ended);
    QCOMPARE(ended.value()->state, core::PlaybackState::STOPPED);
    QCOMPARE(ended.value()->position.value(), std::int64_t{4});
    QVERIFY(engine.play());
    QCOMPARE(observed->startCalls, 2);

    observed->inject_error(core::Error{
        core::ErrorCode::IoFailure,
        "Injected device loss."});
    engine.tick();
    auto failed = engine.snapshot();
    QVERIFY(!failed);
    QCOMPARE(failed.error()->code(), core::ErrorCode::IoFailure);
    QVERIFY(engine.clear());
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::PlaybackSupportTest)

#include "test_playback_support.moc"
