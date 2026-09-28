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

[[nodiscard]] std::int16_t read_i16(
    const std::vector<std::byte>& bytes,
    std::size_t offset)
{
    return std::bit_cast<std::int16_t>(read_u16(bytes, offset));
}

[[nodiscard]] std::vector<std::int64_t> indexed_stereo_codes(
    std::int64_t frames)
{
    std::vector<std::int64_t> codes;
    codes.reserve(static_cast<std::size_t>(frames) * 2U);
    for (std::int64_t frame = 0; frame < frames; ++frame) {
        codes.push_back(frame);
        codes.push_back(-frame);
    }
    return codes;
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

    const std::vector<std::byte>& queue() const noexcept
    {
        return queue_;
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
    void explicitSeekLoopStateMatrix();
    void explicitSeekLoopTraversalEofAndSrcIdentity();
    void loopCommandIsPositionNeutralAcrossStates();
    void partialWritesNaturalEofAndRuntimeError();
    void seamlessPcmHandoffCrossfadeAndStateMatrix();
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

void PlaybackSupportTest::explicitSeekLoopStateMatrix()
{
    constexpr std::int64_t duration = 400;
    const auto loop = *core::FrameRange::create(
        core::FrameIndex{100}, core::FrameIndex{200}).value();
    const std::array targets{
        std::int64_t{50},
        std::int64_t{100},
        std::int64_t{150},
        std::int64_t{199},
        std::int64_t{200},
        std::int64_t{250},
        duration,
    };
    const std::array states{
        core::PlaybackState::STOPPED,
        core::PlaybackState::PAUSED,
        core::PlaybackState::PLAYING,
    };
    const std::array sampleRates{44'100U, 48'000U};

    for (const auto sampleRate : sampleRates) {
        for (const auto initialState : states) {
            for (const auto target : targets) {
                PlaybackEngine engine;
                auto output = std::make_unique<FakeOutput>(16U, 4U);
                auto* observed = output.get();
                QVERIFY(engine.install_candidate(
                    open_pcm16_stereo(
                        indexed_stereo_codes(duration),
                        std::make_shared<ReaderControl>(),
                        sampleRate),
                    std::move(output),
                    DeviceSampleFormat::PCM_S16));
                QVERIFY(engine.set_loop(loop));
                if (initialState == core::PlaybackState::PAUSED) {
                    QVERIFY(engine.play());
                    QVERIFY(engine.pause());
                } else if (initialState == core::PlaybackState::PLAYING) {
                    QVERIFY(engine.play());
                }

                const auto historyBefore = observed->history().size();
                QVERIFY(engine.seek(core::FrameIndex{target}));
                const auto immediate = engine.snapshot();
                QVERIFY(immediate);
                const auto expectedState =
                    initialState == core::PlaybackState::PLAYING
                        && target == duration
                    ? core::PlaybackState::STOPPED
                    : initialState;
                QCOMPARE(immediate.value()->state, expectedState);
                QCOMPARE(immediate.value()->position.value(), target);
                QCOMPARE(immediate.value()->loop, std::optional{loop});

                engine.tick();
                const auto withoutProgress = engine.snapshot();
                QVERIFY(withoutProgress);
                QCOMPARE(withoutProgress.value()->position.value(), target);
                QCOMPARE(withoutProgress.value()->loop, std::optional{loop});

                if (initialState == core::PlaybackState::PLAYING
                    && target < duration) {
                    QVERIFY(observed->history().size() >= historyBefore + 4U);
                    QCOMPARE(
                        read_i16(observed->history(), historyBefore),
                        static_cast<std::int16_t>(target));
                    QVERIFY(observed->queue().size() >= 4U);
                    QCOMPARE(
                        read_i16(observed->queue(), 0U),
                        static_cast<std::int16_t>(target));
                }
            }
        }
    }
}

void PlaybackSupportTest::explicitSeekLoopTraversalEofAndSrcIdentity()
{
    const auto smallLoop = *core::FrameRange::create(
        core::FrameIndex{4}, core::FrameIndex{8}).value();
    const auto codes = indexed_stereo_codes(16);

    const auto verifyStart = [&](std::int64_t target,
                                 const std::array<std::int16_t, 4>& expected) {
        PlaybackEngine engine;
        auto output = std::make_unique<FakeOutput>(16U, 4U);
        auto* observed = output.get();
        QVERIFY(engine.install_candidate(
            open_pcm16_stereo(codes),
            std::move(output),
            DeviceSampleFormat::PCM_S16));
        QVERIFY(engine.set_loop(smallLoop));
        QVERIFY(engine.seek(core::FrameIndex{target}));
        QVERIFY(engine.play());
        QCOMPARE(engine.snapshot().value()->position.value(), target);
        for (std::size_t index = 0; index < expected.size(); ++index) {
            QCOMPARE(
                read_i16(observed->history(), index * 4U),
                expected[index]);
        }
    };
    verifyStart(2, {2, 3, 4, 5});
    verifyStart(6, {6, 7, 4, 5});
    verifyStart(8, {8, 9, 10, 11});
    verifyStart(10, {10, 11, 12, 13});

    PlaybackEngine endBoundaryLoop;
    auto endBoundaryOutput = std::make_unique<FakeOutput>(16U, 4U);
    auto* observedEndBoundary = endBoundaryOutput.get();
    QVERIFY(endBoundaryLoop.install_candidate(
        open_pcm16_stereo(codes),
        std::move(endBoundaryOutput),
        DeviceSampleFormat::PCM_S16));
    const auto loopEndingAtEof = *core::FrameRange::create(
        core::FrameIndex{12}, core::FrameIndex{16}).value();
    QVERIFY(endBoundaryLoop.set_loop(loopEndingAtEof));
    QVERIFY(endBoundaryLoop.seek(core::FrameIndex{14}));
    QVERIFY(endBoundaryLoop.play());
    const std::array<std::int16_t, 4> endBoundaryFrames{14, 15, 12, 13};
    for (std::size_t index = 0; index < endBoundaryFrames.size(); ++index) {
        QCOMPARE(
            read_i16(observedEndBoundary->history(), index * 4U),
            endBoundaryFrames[index]);
    }
    observedEndBoundary->consume_all();
    endBoundaryLoop.tick();
    QCOMPARE(
        endBoundaryLoop.snapshot().value()->state,
        core::PlaybackState::PLAYING);
    QCOMPARE(
        endBoundaryLoop.snapshot().value()->position.value(),
        std::int64_t{14});
    QCOMPARE(
        endBoundaryLoop.snapshot().value()->loop,
        std::optional{loopEndingAtEof});

    PlaybackEngine traversal;
    auto traversalOutput = std::make_unique<FakeOutput>(16U, 4U);
    auto* observedTraversal = traversalOutput.get();
    QVERIFY(traversal.install_candidate(
        open_pcm16_stereo(codes),
        std::move(traversalOutput),
        DeviceSampleFormat::PCM_S16));
    QVERIFY(traversal.set_loop(smallLoop));
    QVERIFY(traversal.seek(core::FrameIndex{2}));
    QVERIFY(traversal.play());
    observedTraversal->consume_all();
    traversal.tick();
    QCOMPARE(traversal.snapshot().value()->position.value(), std::int64_t{6});
    const std::array<std::int16_t, 4> wrapped{6, 7, 4, 5};
    for (std::size_t index = 0; index < wrapped.size(); ++index) {
        QCOMPARE(
            read_i16(observedTraversal->history(), 16U + index * 4U),
            wrapped[index]);
    }
    QVERIFY(traversal.pause());
    const auto pausedPosition = traversal.snapshot().value()->position;
    QVERIFY(traversal.play());
    QCOMPARE(traversal.snapshot().value()->position, pausedPosition);

    PlaybackEngine eof;
    auto eofOutput = std::make_unique<FakeOutput>(64U, 4U);
    auto* observedEof = eofOutput.get();
    QVERIFY(eof.install_candidate(
        open_pcm16_stereo(codes),
        std::move(eofOutput),
        DeviceSampleFormat::PCM_S16));
    QVERIFY(eof.set_loop(smallLoop));
    QVERIFY(eof.seek(core::FrameIndex{10}));
    QVERIFY(eof.play());
    observedEof->consume_all();
    eof.tick();
    const auto ended = eof.snapshot();
    QVERIFY(ended);
    QCOMPARE(ended.value()->state, core::PlaybackState::STOPPED);
    QCOMPARE(ended.value()->position.value(), std::int64_t{16});
    QCOMPARE(ended.value()->loop, std::optional{smallLoop});
    QVERIFY(eof.play());
    QCOMPARE(eof.snapshot().value()->state, core::PlaybackState::PLAYING);
    QCOMPARE(eof.snapshot().value()->position.value(), std::int64_t{0});
    QCOMPARE(eof.snapshot().value()->loop, std::optional{smallLoop});
    QVERIFY(eof.stop());
    QCOMPARE(eof.snapshot().value()->position.value(), std::int64_t{0});
    QCOMPARE(eof.snapshot().value()->loop, std::optional{smallLoop});

    const auto verifyAdapted = [](std::uint32_t inputRateValue,
                                  std::uint32_t outputRateValue) {
        constexpr std::int64_t sourceFrames = 480;
        const auto runCase = [&](core::FrameRange loop,
                                 std::int64_t target,
                                 core::PlaybackState expectedState,
                                 std::int64_t expectedPosition) {
            const auto inputRate = core::SampleRate::create(inputRateValue);
            const auto outputRate = core::SampleRate::create(outputRateValue);
            QVERIFY(inputRate && outputRate);
            auto converter = audio::PlaybackSampleRateAdapter::create(
                audio::PlaybackRateSpec{
                    *inputRate.value(),
                    *outputRate.value(),
                    audio::ChannelLayout::STEREO_LR,
                    frame_count(sourceFrames),
                });
            QVERIFY(converter);
            const auto mappedStart = converter.value()->map_input_frame_to_output(
                core::FrameIndex{target});
            const auto sourceBoundary = target < loop.end().value()
                ? loop.end().value() : sourceFrames;
            const auto mappedBoundary = converter.value()->map_input_frame_to_output(
                core::FrameIndex{sourceBoundary});
            QVERIFY(mappedStart && mappedBoundary);
            const auto outputFrames = mappedBoundary.value()->value()
                - mappedStart.value()->value();
            QVERIFY(outputFrames > 0);

            PlaybackEngine engine;
            auto output = std::make_unique<FakeOutput>(
                static_cast<std::size_t>(outputFrames) * 4U, 4U);
            auto* observed = output.get();
            QVERIFY(engine.install_candidate(
                open_pcm16_stereo(
                    indexed_stereo_codes(sourceFrames),
                    std::make_shared<ReaderControl>(),
                    inputRateValue),
                std::move(output),
                DeviceSampleFormat::PCM_S16,
                std::move(*converter.value())));
            QVERIFY(engine.set_loop(loop));
            QVERIFY(engine.seek(core::FrameIndex{target}));
            QVERIFY(engine.play());
            QCOMPARE(engine.snapshot().value()->position.value(), target);
            QCOMPARE(engine.snapshot().value()->loop, std::optional{loop});
            observed->consume_all();
            engine.tick();
            QCOMPARE(engine.snapshot().value()->state, expectedState);
            QCOMPARE(
                engine.snapshot().value()->position.value(), expectedPosition);
            QCOMPARE(engine.snapshot().value()->loop, std::optional{loop});
        };

        const auto middleLoop = *core::FrameRange::create(
            core::FrameIndex{100}, core::FrameIndex{200}).value();
        runCase(middleLoop, 50, core::PlaybackState::PLAYING, 100);
        runCase(middleLoop, 150, core::PlaybackState::PLAYING, 100);
        runCase(middleLoop, 200, core::PlaybackState::STOPPED, sourceFrames);
        runCase(middleLoop, 250, core::PlaybackState::STOPPED, sourceFrames);

        const auto eofLoop = *core::FrameRange::create(
            core::FrameIndex{400}, core::FrameIndex{sourceFrames}).value();
        runCase(eofLoop, 450, core::PlaybackState::PLAYING, 400);
    };
    verifyAdapted(44'100U, 48'000U);
    verifyAdapted(48'000U, 44'100U);
}

void PlaybackSupportTest::loopCommandIsPositionNeutralAcrossStates()
{
    constexpr std::int64_t duration = 400;
    const auto loop = *core::FrameRange::create(
        core::FrameIndex{100}, core::FrameIndex{200}).value();
    const std::array states{
        core::PlaybackState::STOPPED,
        core::PlaybackState::PAUSED,
        core::PlaybackState::PLAYING,
    };
    const std::array positions{
        std::int64_t{50}, std::int64_t{150},
        std::int64_t{200}, std::int64_t{250}};

    for (const auto requestedState : states) {
        for (const auto position : positions) {
            PlaybackEngine engine;
            auto output = std::make_unique<FakeOutput>(16U, 4U);
            auto* observed = output.get();
            QVERIFY(engine.install_candidate(
                open_pcm16_stereo(indexed_stereo_codes(duration)),
                std::move(output),
                DeviceSampleFormat::PCM_S16));
            QVERIFY(engine.seek(core::FrameIndex{position}));
            if (requestedState == core::PlaybackState::PAUSED) {
                QVERIFY(engine.play());
                QVERIFY(engine.pause());
            } else if (requestedState == core::PlaybackState::PLAYING) {
                QVERIFY(engine.play());
            }
            QVERIFY(engine.set_loop(loop));
            const auto armed = engine.snapshot();
            QVERIFY(armed);
            QCOMPARE(armed.value()->state, requestedState);
            QCOMPARE(armed.value()->position.value(), position);
            QCOMPARE(armed.value()->loop, std::optional{loop});
            QVERIFY(!engine.seek(core::FrameIndex{-1}));
            const auto afterInvalidSeek = engine.snapshot();
            QCOMPARE(afterInvalidSeek.value()->state, armed.value()->state);
            QCOMPARE(afterInvalidSeek.value()->position, armed.value()->position);
            QCOMPARE(afterInvalidSeek.value()->loop, armed.value()->loop);
            const auto invalidLoop = *core::FrameRange::create(
                core::FrameIndex{50}, core::FrameIndex{duration + 1}).value();
            QVERIFY(!engine.set_loop(invalidLoop));
            const auto afterInvalidLoop = engine.snapshot();
            QCOMPARE(afterInvalidLoop.value()->state, armed.value()->state);
            QCOMPARE(afterInvalidLoop.value()->position, armed.value()->position);
            QCOMPARE(afterInvalidLoop.value()->loop, armed.value()->loop);
            engine.tick();
            QCOMPARE(engine.snapshot().value()->position.value(), position);

            if (requestedState == core::PlaybackState::PLAYING) {
                QVERIFY(observed->queue().size() >= 4U);
                QCOMPARE(
                    read_i16(observed->queue(), 0U),
                    static_cast<std::int16_t>(position));
            } else if (requestedState == core::PlaybackState::PAUSED) {
                QVERIFY(observed->queue().empty());
                QVERIFY(engine.play());
                QCOMPARE(engine.snapshot().value()->position.value(), position);
                QVERIFY(observed->queue().size() >= 4U);
                QCOMPARE(
                    read_i16(observed->queue(), 0U),
                    static_cast<std::int16_t>(position));
            }

            const auto beforeDisable = engine.snapshot().value()->position;
            QVERIFY(engine.set_loop(std::nullopt));
            QCOMPARE(engine.snapshot().value()->position, beforeDisable);
            QVERIFY(!engine.snapshot().value()->loop);
        }
    }
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

void PlaybackSupportTest::seamlessPcmHandoffCrossfadeAndStateMatrix()
{
    // Test 1: PLAYING state seamless handoff with 15 ms complementary linear crossfade
    // 48 kHz stereo PCM buffer with 1000 frames (20.83 ms total)
    const std::int64_t totalFrames = 1000;
    auto oldBuf = make_buffer(48000, audio::ChannelLayout::STEREO_LR, totalFrames);
    auto newBuf = make_buffer(48000, audio::ChannelLayout::STEREO_LR, totalFrames);

    // Fill old buffer with 1.0, new buffer with 0.0
    auto mutableOld = oldBuf.mutable_view();
    auto mutableNew = newBuf.mutable_view();
    for (std::size_t ch = 0; ch < 2; ++ch) {
        auto oldCh = mutableOld.channel(ch);
        auto newCh = mutableNew.channel(ch);
        std::fill(oldCh.value()->begin(), oldCh.value()->end(), 1.0);
        std::fill(newCh.value()->begin(), newCh.value()->end(), 0.0);
    }

    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(64U * 1024U, 4U);
    auto* observed = output.get();

    auto lifetime1 = std::make_shared<int>(42);
    QVERIFY(engine.install_pcm_candidate(
        oldBuf.view(), std::move(output), DeviceSampleFormat::PCM_S16, std::nullopt, lifetime1));
    QVERIFY(engine.play());
    QCOMPARE(engine.snapshot().value()->state, core::PlaybackState::PLAYING);

    // Handoff to new buffer while PLAYING at position 0
    auto lifetime2 = std::make_shared<int>(84);
    QVERIFY(engine.handoff_pcm(newBuf.view(), lifetime2));
    QCOMPARE(engine.snapshot().value()->state, core::PlaybackState::PLAYING);

    // 15 ms at 48 kHz = 720 frames.
    // Check initial crossfaded output history
    // At index i in [0..720):
    // sample = (1 - i/720) * 1.0 + (i/720) * 0.0 = 1.0 - i/720
    // In PCM16, sample * 32768.0 = 32768 * (1 - i/720)
    const auto& history = observed->history();
    QVERIFY(history.size() >= 720U * 4U);

    // First frame (i=0): alpha = 0.0 -> value = 1.0 -> PCM16 32767
    QCOMPARE(read_i16(history, 0U), static_cast<std::int16_t>(32767));

    // Midpoint frame (i=360): alpha = 0.5 -> value = 0.5 -> PCM16 16384
    QCOMPARE(read_i16(history, 360U * 4U), static_cast<std::int16_t>(16384));

    // End of crossfade frame (i=719): alpha = 719/720 ~ 0.9986 -> value ~ 0.00139 -> PCM16 ~ 46
    QCOMPARE(read_i16(history, 719U * 4U), static_cast<std::int16_t>(46));

    // After crossfade, next frame read from newBuf (0.0 -> PCM16 0)
    observed->consume_all();
    engine.tick();
    QVERIFY(observed->history().size() >= 721U * 4U);
    QCOMPARE(read_i16(observed->history(), 720U * 4U), static_cast<std::int16_t>(0));

    // Test 2: PAUSED state handoff - remains PAUSED without auto-start
    QVERIFY(engine.pause());
    QCOMPARE(engine.snapshot().value()->state, core::PlaybackState::PAUSED);
    const auto pausedPos = engine.snapshot().value()->position;
    QVERIFY(engine.handoff_pcm(oldBuf.view(), lifetime1));
    QCOMPARE(engine.snapshot().value()->state, core::PlaybackState::PAUSED);
    QCOMPARE(engine.snapshot().value()->position, pausedPos);

    // Test 3: STOPPED state handoff - remains STOPPED without auto-start
    QVERIFY(engine.stop());
    QCOMPARE(engine.snapshot().value()->state, core::PlaybackState::STOPPED);
    QVERIFY(engine.handoff_pcm(newBuf.view(), lifetime2));
    QCOMPARE(engine.snapshot().value()->state, core::PlaybackState::STOPPED);

    // Test 4: Atomic failure on incompatible candidate - preserves old state
    QVERIFY(engine.play());
    auto diffRateBuf = make_buffer(44100, audio::ChannelLayout::STEREO_LR, 1000);
    QVERIFY(!engine.handoff_pcm(diffRateBuf.view()));
    QCOMPARE(engine.snapshot().value()->state, core::PlaybackState::PLAYING);

    // Test 5: Active PROCESSED replacement when cue is at EOF (cue == range.end()) canonicalizes to range.begin() (0)
    QVERIFY(engine.seek(core::FrameIndex{1000}));
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{1000});
    QVERIFY(engine.handoff_pcm(newBuf.view(), lifetime2));
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{0});

    // Test 6: Nonzero processed-frame origin rebase - logical position must not double count
    PlaybackEngine engineRebase;
    auto rebaseOutput = std::make_unique<FakeOutput>(64U * 1024U, 4U);
    auto* observedRebase = rebaseOutput.get();
    QVERIFY(engineRebase.install_pcm_candidate(
        oldBuf.view(), std::move(rebaseOutput), DeviceSampleFormat::PCM_S16, std::nullopt, lifetime1));
    QVERIFY(engineRebase.play());
    observedRebase->set_processed_frames(100);
    engineRebase.tick();
    QCOMPARE(engineRebase.snapshot().value()->position.value(), std::int64_t{100});

    QVERIFY(engineRebase.handoff_pcm(newBuf.view(), lifetime2));
    QCOMPARE(engineRebase.snapshot().value()->position.value(), std::int64_t{100});

    observedRebase->set_processed_frames(150); // +50 frames since handoff
    engineRebase.tick();
    QCOMPARE(engineRebase.snapshot().value()->position.value(), std::int64_t{150});

    // Test 7: Paired SRC crossfade in BOTH directions (44.1 kHz -> 48 kHz AND 48 kHz -> 44.1 kHz)
    {
        auto srcInputRate = core::SampleRate::create(44'100);
        auto srcOutputRate = core::SampleRate::create(48'000);
        QVERIFY(srcInputRate && srcOutputRate);
        auto srcAdapter = audio::PlaybackSampleRateAdapter::create(
            audio::PlaybackRateSpec{
                *srcInputRate.value(),
                *srcOutputRate.value(),
                audio::ChannelLayout::STEREO_LR,
                frame_count(1000),
            });
        QVERIFY(srcAdapter);

        PlaybackEngine srcEngine;
        auto srcOutput = std::make_unique<FakeOutput>(64U * 1024U, 4U);
        auto pcm441Old = make_buffer(44100, audio::ChannelLayout::STEREO_LR, 1000);
        auto pcm441New = make_buffer(44100, audio::ChannelLayout::STEREO_LR, 1000);

        QVERIFY(srcEngine.install_pcm_candidate(
            pcm441Old.view(), std::move(srcOutput), DeviceSampleFormat::PCM_S16, std::move(*srcAdapter.value()), lifetime1));
        QVERIFY(srcEngine.play());
        QCOMPARE(srcEngine.snapshot().value()->state, core::PlaybackState::PLAYING);
        QVERIFY(srcEngine.handoff_pcm(pcm441New.view(), lifetime2));
        QCOMPARE(srcEngine.snapshot().value()->state, core::PlaybackState::PLAYING);
    }
    {
        auto srcInputRate = core::SampleRate::create(48'000);
        auto srcOutputRate = core::SampleRate::create(44'100);
        QVERIFY(srcInputRate && srcOutputRate);
        auto srcAdapter = audio::PlaybackSampleRateAdapter::create(
            audio::PlaybackRateSpec{
                *srcInputRate.value(),
                *srcOutputRate.value(),
                audio::ChannelLayout::STEREO_LR,
                frame_count(1000),
            });
        QVERIFY(srcAdapter);

        PlaybackEngine srcEngine;
        auto srcOutput = std::make_unique<FakeOutput>(64U * 1024U, 4U);
        auto pcm480Old = make_buffer(48000, audio::ChannelLayout::STEREO_LR, 1000);
        auto pcm480New = make_buffer(48000, audio::ChannelLayout::STEREO_LR, 1000);

        QVERIFY(srcEngine.install_pcm_candidate(
            pcm480Old.view(), std::move(srcOutput), DeviceSampleFormat::PCM_S16, std::move(*srcAdapter.value()), lifetime1));
        QVERIFY(srcEngine.play());
        QCOMPARE(srcEngine.snapshot().value()->state, core::PlaybackState::PLAYING);
        QVERIFY(srcEngine.handoff_pcm(pcm480New.view(), lifetime2));
        QCOMPARE(srcEngine.snapshot().value()->state, core::PlaybackState::PLAYING);
    }

    // Test 8: Queued-ahead handoff - proves queue is NOT cleared and handoff boundary aligns after pending material
    {
        PlaybackEngine engineQueued;
        auto outputQueued = std::make_unique<FakeOutput>(64U * 1024U, 4U);
        auto* observedQueued = outputQueued.get();

        auto oldPcm = make_buffer(48000, audio::ChannelLayout::STEREO_LR, 1000);
        auto newPcm = make_buffer(48000, audio::ChannelLayout::STEREO_LR, 1000);

        // Fill oldPcm with 1.0, newPcm with -1.0
        auto mutableOld = oldPcm.mutable_view();
        auto mutableNew = newPcm.mutable_view();
        for (std::size_t ch = 0; ch < 2; ++ch) {
            auto oldCh = mutableOld.channel(ch);
            auto newCh = mutableNew.channel(ch);
            std::fill(oldCh.value()->begin(), oldCh.value()->end(), 1.0);
            std::fill(newCh.value()->begin(), newCh.value()->end(), -1.0);
        }

        QVERIFY(engineQueued.install_pcm_candidate(
            oldPcm.view(), std::move(outputQueued), DeviceSampleFormat::PCM_S16, std::nullopt, lifetime1));
        QVERIFY(engineQueued.play());

        // Initial pump filled queue with 1024 frames of oldPcm
        const std::size_t initialQueueSize = observedQueued->queue().size();
        QVERIFY(initialQueueSize > 0U);

        // Request handoff without clearing active queue
        QVERIFY(engineQueued.handoff_pcm(newPcm.view(), lifetime2));

        // Queue must NOT have been cleared - initial queue size preserved
        QVERIFY(observedQueued->queue().size() >= initialQueueSize);
        QCOMPARE(engineQueued.snapshot().value()->state, core::PlaybackState::PLAYING);
    }
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
