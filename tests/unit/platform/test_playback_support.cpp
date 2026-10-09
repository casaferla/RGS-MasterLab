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
#include <cstdio>
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

    void clear_injected_error() noexcept
    {
        injectedError_.reset();
        state_ = OutputState::STOPPED;
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
    void controlPlaneTraversalSeekAndLoopSerials();
};

void PlaybackSupportTest::formatSelectionIsDeterministic()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: formatSelectionIsDeterministic\\n");
    std::fflush(stderr);
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
    std::fprintf(stderr, "DBG_SLOT_EXIT: formatSelectionIsDeterministic\\n");
    std::fflush(stderr);
}

void PlaybackSupportTest::adaptedTimelineUsesAbsoluteRateMapping()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: adaptedTimelineUsesAbsoluteRateMapping\\n");
    std::fflush(stderr);
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
    std::fprintf(stderr, "DBG_SLOT_EXIT: adaptedTimelineUsesAbsoluteRateMapping\\n");
    std::fflush(stderr);
}

void PlaybackSupportTest::explicitSeekLoopStateMatrix()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: explicitSeekLoopStateMatrix\\n");
    std::fflush(stderr);
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
    std::fprintf(stderr, "DBG_SLOT_EXIT: explicitSeekLoopStateMatrix\\n");
    std::fflush(stderr);
}

void PlaybackSupportTest::explicitSeekLoopTraversalEofAndSrcIdentity()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: explicitSeekLoopTraversalEofAndSrcIdentity\\n");
    std::fflush(stderr);
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
    std::fprintf(stderr, "DBG_SLOT_EXIT: explicitSeekLoopTraversalEofAndSrcIdentity\\n");
    std::fflush(stderr);
}

void PlaybackSupportTest::loopCommandIsPositionNeutralAcrossStates()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: loopCommandIsPositionNeutralAcrossStates\\n");
    std::fflush(stderr);
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
    std::fprintf(stderr, "DBG_SLOT_EXIT: loopCommandIsPositionNeutralAcrossStates\\n");
    std::fflush(stderr);
}

void PlaybackSupportTest::floatAndPcm16GoldenConversion()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: floatAndPcm16GoldenConversion\\n");
    std::fflush(stderr);
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
    std::fprintf(stderr, "DBG_SLOT_EXIT: floatAndPcm16GoldenConversion\\n");
    std::fflush(stderr);
}

void PlaybackSupportTest::conversionFailuresAndChunkInvariance()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: conversionFailuresAndChunkInvariance\\n");
    std::fflush(stderr);
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
    std::fprintf(stderr, "DBG_SLOT_EXIT: conversionFailuresAndChunkInvariance\\n");
    std::fflush(stderr);
}

void PlaybackSupportTest::stateMachineAndBoundedPump()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: stateMachineAndBoundedPump\\n");
    std::fflush(stderr);
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
    std::fprintf(stderr, "DBG_SLOT_EXIT: stateMachineAndBoundedPump\\n");
    std::fflush(stderr);
}

void PlaybackSupportTest::seamlessPcmHandoffCrossfadeAndStateMatrix()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: seamlessPcmHandoffCrossfadeAndStateMatrix\\n");
    std::fflush(stderr);
    qInfo() << "DBG_PLAYBACK_SUPPORT: seamless:start";
    // Test 1: PLAYING replacement uses the deterministic future queued boundary,
    // not the stale/current cue, then emits a 15 ms complementary linear crossfade.
    constexpr std::int64_t totalFrames = 5000;
    constexpr std::size_t bytesPerFrame = 4U;
    constexpr std::size_t queuedCapacityFrames = 512U;
    constexpr std::size_t handoffBoundaryFrames = 1024U;
    constexpr std::size_t xfadeFrames48k = 720U;

    auto oldBuf = make_buffer(
        48000, audio::ChannelLayout::STEREO_LR, totalFrames);
    auto newBuf = make_buffer(
        48000, audio::ChannelLayout::STEREO_LR, totalFrames);

    auto mutableOld = oldBuf.mutable_view();
    auto mutableNew = newBuf.mutable_view();
    for (std::size_t ch = 0; ch < 2; ++ch) {
        auto oldCh = mutableOld.channel(ch);
        auto newCh = mutableNew.channel(ch);
        std::fill(oldCh.value()->begin(), oldCh.value()->end(), 1.0);
        std::fill(newCh.value()->begin(), newCh.value()->end(), 0.0);
    }

    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(
        queuedCapacityFrames * bytesPerFrame, bytesPerFrame);
    auto* observed = output.get();

    auto lifetime1 = std::make_shared<int>(42);
    const core::RealizationId oldRealizationId{1};
    const core::RealizationId newRealizationId{2};
    QVERIFY(engine.install_pcm_candidate(
        oldBuf.view(),
        std::move(output),
        DeviceSampleFormat::PCM_S16,
        std::nullopt,
        lifetime1,
        oldRealizationId));
    auto initialRealization = engine.snapshot().value()->audibleRealization;
    QCOMPARE(initialRealization.phase, core::AudibleHandoffPhase::NEW);
    QCOMPARE(initialRealization.realizationId, std::optional{oldRealizationId});
    QVERIFY(engine.play());
    QCOMPARE(
        engine.snapshot().value()->state,
        core::PlaybackState::PLAYING);
    QCOMPARE(
        observed->history().size(),
        queuedCapacityFrames * bytesPerFrame);

    const int stopCallsBeforeHandoff = observed->stopCalls;
    auto lifetime2 = std::make_shared<int>(84);
    QVERIFY(engine.handoff_pcm(newBuf.view(), lifetime2, newRealizationId));
    QCOMPARE(
        engine.snapshot().value()->state,
        core::PlaybackState::PLAYING);
    auto beforeBoundary = engine.snapshot().value()->audibleRealization;
    QCOMPARE(beforeBoundary.phase, core::AudibleHandoffPhase::OLD);
    QCOMPARE(beforeBoundary.realizationId, std::optional{oldRealizationId});
    observed->set_processed_frames(
        static_cast<std::int64_t>(handoffBoundaryFrames));
    auto atBoundary = engine.snapshot().value()->audibleRealization;
    QCOMPARE(atBoundary.phase, core::AudibleHandoffPhase::TRANSITION);
    QVERIFY(!atBoundary.realizationId.has_value());
    observed->set_processed_frames(
        static_cast<std::int64_t>(
            handoffBoundaryFrames + xfadeFrames48k - 1U));
    auto lastTransition = engine.snapshot().value()->audibleRealization;
    QCOMPARE(lastTransition.phase, core::AudibleHandoffPhase::TRANSITION);
    observed->set_processed_frames(
        static_cast<std::int64_t>(
            handoffBoundaryFrames + xfadeFrames48k));
    auto afterBoundary = engine.snapshot().value()->audibleRealization;
    QCOMPARE(afterBoundary.phase, core::AudibleHandoffPhase::NEW);
    QCOMPARE(afterBoundary.realizationId, std::optional{newRealizationId});
    observed->set_processed_frames(0);
    QCOMPARE(observed->stopCalls, stopCallsBeforeHandoff);

    const auto requiredHistoryBytes =
        (handoffBoundaryFrames + xfadeFrames48k + 1U) * bytesPerFrame;
    for (int iteration = 0;
         iteration < 8
             && observed->history().size() < requiredHistoryBytes;
         ++iteration) {
        observed->consume_all();
        engine.tick();
    }

    const auto& history = observed->history();
    QVERIFY(history.size() >= requiredHistoryBytes);

    // All material before the future boundary remains old realization audio.
    QCOMPARE(
        read_i16(history, (handoffBoundaryFrames - 1U) * bytesPerFrame),
        static_cast<std::int16_t>(32767));

    // Crossfade starts exactly at the queued future boundary.
    QCOMPARE(
        read_i16(history, handoffBoundaryFrames * bytesPerFrame),
        static_cast<std::int16_t>(32767));
    QCOMPARE(
        read_i16(
            history,
            (handoffBoundaryFrames + 360U) * bytesPerFrame),
        static_cast<std::int16_t>(16384));
    QCOMPARE(
        read_i16(
            history,
            (handoffBoundaryFrames + 719U) * bytesPerFrame),
        static_cast<std::int16_t>(46));

    // The first post-crossfade frame is new-only.
    QCOMPARE(
        read_i16(
            history,
            (handoffBoundaryFrames + xfadeFrames48k) * bytesPerFrame),
        static_cast<std::int16_t>(0));

    // Test 2: PAUSED replacement remains PAUSED without auto-start.
    QVERIFY(engine.pause());
    QCOMPARE(
        engine.snapshot().value()->state,
        core::PlaybackState::PAUSED);
    const auto pausedPos = engine.snapshot().value()->position;
    QVERIFY(engine.handoff_pcm(oldBuf.view(), lifetime1));
    QCOMPARE(
        engine.snapshot().value()->state,
        core::PlaybackState::PAUSED);
    QCOMPARE(engine.snapshot().value()->position, pausedPos);

    qInfo() << "DBG_PLAYBACK_SUPPORT: seamless:before-2A";
    // Test 2A: a PAUSED replacement must discard queued old audio before
    // the new realization is published. Resume/replay must therefore begin
    // with new-only audio, never old queued material under a NEW identity.
    {
        PlaybackEngine pausedReplacement;
        auto pausedOutput = std::make_unique<FakeOutput>(
            queuedCapacityFrames * bytesPerFrame, bytesPerFrame);
        auto* observedPaused = pausedOutput.get();
        QVERIFY(pausedReplacement.install_pcm_candidate(
            oldBuf.view(),
            std::move(pausedOutput),
            DeviceSampleFormat::PCM_S16,
            std::nullopt,
            lifetime1,
            oldRealizationId));
        QVERIFY(pausedReplacement.play());
        QVERIFY(observedPaused->queued_bytes() > 0U);
        QVERIFY(pausedReplacement.pause());

        const auto historyBeforeReplacement =
            observedPaused->history().size();
        QVERIFY(pausedReplacement.handoff_pcm(
            newBuf.view(), lifetime2, newRealizationId));
        QCOMPARE(
            pausedReplacement.snapshot().value()->state,
            core::PlaybackState::PAUSED);
        QCOMPARE(observedPaused->queued_bytes(), std::size_t{0});
        const auto pausedReplacementIdentity =
            pausedReplacement.snapshot().value()->audibleRealization;
        QCOMPARE(
            pausedReplacementIdentity.phase,
            core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            pausedReplacementIdentity.realizationId,
            std::optional{newRealizationId});

        QVERIFY(pausedReplacement.play());
        QVERIFY(observedPaused->history().size() > historyBeforeReplacement);
        QCOMPARE(
            read_i16(
                observedPaused->history(),
                historyBeforeReplacement),
            static_cast<std::int16_t>(0));
        const auto resumedIdentity =
            pausedReplacement.snapshot().value()->audibleRealization;
        QCOMPARE(resumedIdentity.phase, core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            resumedIdentity.realizationId,
            std::optional{newRealizationId});
    }

    // Test 2B: a late output enqueue failure during PLAYING handoff must fail
    // closed. No candidate identity or candidate audio may survive the failed
    // handoff; replay resumes from the previously accepted realization.
    {
        PlaybackEngine atomicFailure;
        auto atomicOutput = std::make_unique<FakeOutput>(
            queuedCapacityFrames * bytesPerFrame, bytesPerFrame);
        auto* observedAtomic = atomicOutput.get();
        QVERIFY(atomicFailure.install_pcm_candidate(
            oldBuf.view(),
            std::move(atomicOutput),
            DeviceSampleFormat::PCM_S16,
            std::nullopt,
            lifetime1,
            oldRealizationId));
        QVERIFY(atomicFailure.play());

        observedAtomic->consume_all();
        observedAtomic->inject_error(core::Error{
            core::ErrorCode::IoFailure,
            "Injected late handoff enqueue failure."});

        QVERIFY(!atomicFailure.handoff_pcm(
            newBuf.view(), lifetime2, newRealizationId));
        const auto failedSnapshot = atomicFailure.snapshot();
        QVERIFY(failedSnapshot);
        QCOMPARE(
            failedSnapshot.value()->state,
            core::PlaybackState::STOPPED);
        QCOMPARE(
            failedSnapshot.value()->audibleRealization.phase,
            core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            failedSnapshot.value()->audibleRealization.realizationId,
            std::optional{oldRealizationId});
        QCOMPARE(observedAtomic->queued_bytes(), std::size_t{0});

        observedAtomic->clear_injected_error();
        const auto historyBeforeReplay = observedAtomic->history().size();
        QVERIFY(atomicFailure.play());
        QVERIFY(observedAtomic->history().size() > historyBeforeReplay);
        QCOMPARE(
            read_i16(
                observedAtomic->history(),
                historyBeforeReplay),
            static_cast<std::int16_t>(32767));
        const auto replayIdentity =
            atomicFailure.snapshot().value()->audibleRealization;
        QCOMPARE(replayIdentity.phase, core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            replayIdentity.realizationId,
            std::optional{oldRealizationId});
    }

    qInfo() << "DBG_PLAYBACK_SUPPORT: seamless:before-2C";
    // Test 2C: pausing during an already-pending handoff freezes the
    // audible OLD/TRANSITION phase; resume continues the same progression.
    {
        PlaybackEngine pendingPause;
        auto pendingOutput = std::make_unique<FakeOutput>(
            queuedCapacityFrames * bytesPerFrame, bytesPerFrame);
        auto* observedPending = pendingOutput.get();
        QVERIFY(pendingPause.install_pcm_candidate(
            oldBuf.view(),
            std::move(pendingOutput),
            DeviceSampleFormat::PCM_S16,
            std::nullopt,
            lifetime1,
            oldRealizationId));
        QVERIFY(pendingPause.play());
        QVERIFY(pendingPause.handoff_pcm(
            newBuf.view(), lifetime2, newRealizationId));

        observedPending->set_processed_frames(500);
        QVERIFY(pendingPause.pause());
        const auto pausedOld =
            pendingPause.snapshot().value()->audibleRealization;
        QCOMPARE(pausedOld.phase, core::AudibleHandoffPhase::OLD);
        QCOMPARE(
            pausedOld.realizationId,
            std::optional{oldRealizationId});
        QCOMPARE(
            pendingPause.snapshot().value()->audibleRealization,
            pausedOld);

        QVERIFY(pendingPause.play());
        observedPending->set_processed_frames(
            static_cast<std::int64_t>(handoffBoundaryFrames + 100U));
        QVERIFY(pendingPause.pause());
        const auto pausedTransition =
            pendingPause.snapshot().value()->audibleRealization;
        QCOMPARE(
            pausedTransition.phase,
            core::AudibleHandoffPhase::TRANSITION);
        QVERIFY(!pausedTransition.realizationId.has_value());
        QCOMPARE(
            pendingPause.snapshot().value()->audibleRealization,
            pausedTransition);

        QVERIFY(pendingPause.play());
        observedPending->set_processed_frames(
            static_cast<std::int64_t>(
                handoffBoundaryFrames + xfadeFrames48k));
        const auto resumedNew =
            pendingPause.snapshot().value()->audibleRealization;
        QCOMPARE(resumedNew.phase, core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            resumedNew.realizationId,
            std::optional{newRealizationId});
    }

    qInfo() << "DBG_PLAYBACK_SUPPORT: seamless:before-2D";
    // Test 2D: seeking while a handoff is pending discards the old queued
    // chronology. Playback restarts from the accepted new realization.
    {
        PlaybackEngine pendingSeek;
        auto seekOutput = std::make_unique<FakeOutput>(
            queuedCapacityFrames * bytesPerFrame, bytesPerFrame);
        auto* observedSeek = seekOutput.get();
        QVERIFY(pendingSeek.install_pcm_candidate(
            oldBuf.view(),
            std::move(seekOutput),
            DeviceSampleFormat::PCM_S16,
            std::nullopt,
            lifetime1,
            oldRealizationId));
        QVERIFY(pendingSeek.play());
        QVERIFY(pendingSeek.handoff_pcm(
            newBuf.view(), lifetime2, newRealizationId));
        QCOMPARE(
            pendingSeek.snapshot().value()->audibleRealization.phase,
            core::AudibleHandoffPhase::OLD);

        const auto historyBeforeSeek = observedSeek->history().size();
        QVERIFY(pendingSeek.seek(core::FrameIndex{1500}));
        QCOMPARE(
            pendingSeek.snapshot().value()->state,
            core::PlaybackState::PLAYING);
        const auto afterSeek =
            pendingSeek.snapshot().value()->audibleRealization;
        QCOMPARE(afterSeek.phase, core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            afterSeek.realizationId,
            std::optional{newRealizationId});
        QVERIFY(observedSeek->history().size() > historyBeforeSeek);
        QCOMPARE(
            read_i16(observedSeek->history(), historyBeforeSeek),
            static_cast<std::int16_t>(0));
    }

    qInfo() << "DBG_PLAYBACK_SUPPORT: seamless:before-2E";
    // Test 2E: zero- and short-crossfade boundaries have deterministic
    // identity semantics: zero skips TRANSITION, short remains TRANSITION
    // for exactly the bounded output-frame interval.
    const auto verifyShortCrossfadeIdentity = [&](
        std::int64_t loopEndFrame,
        std::int64_t expectedTransitionFrames) {
        std::fprintf(stderr,
            "DBG_LAMBDA_ENTER: short-crossfade loopEnd=%lld transition=%lld\\n",
            static_cast<long long>(loopEndFrame),
            static_cast<long long>(expectedTransitionFrames));
        std::fflush(stderr);
        PlaybackEngine shortEngine;
        auto shortOutput = std::make_unique<FakeOutput>(
            queuedCapacityFrames * bytesPerFrame, bytesPerFrame);
        auto* observedShort = shortOutput.get();
        QVERIFY(shortEngine.install_pcm_candidate(
            oldBuf.view(),
            std::move(shortOutput),
            DeviceSampleFormat::PCM_S16,
            std::nullopt,
            lifetime1,
            oldRealizationId));
        auto shortLoop = core::FrameRange::create(
            core::FrameIndex{0},
            core::FrameIndex{loopEndFrame});
        QVERIFY(shortLoop);
        QVERIFY(shortEngine.set_loop(*shortLoop.value()));
        constexpr std::int64_t deviceProcessedBaseline = 5000;
        observedShort->set_processed_frames(deviceProcessedBaseline);
        QVERIFY(shortEngine.play());
        QVERIFY(shortEngine.handoff_pcm(
            newBuf.view(), lifetime2, newRealizationId));

        observedShort->set_processed_frames(
            deviceProcessedBaseline
            + static_cast<std::int64_t>(handoffBoundaryFrames - 1U));
        const auto beforeShort =
            shortEngine.snapshot().value()->audibleRealization;
        QCOMPARE(beforeShort.phase, core::AudibleHandoffPhase::OLD);
        QCOMPARE(
            beforeShort.realizationId,
            std::optional{oldRealizationId});

        observedShort->set_processed_frames(
            deviceProcessedBaseline
            + static_cast<std::int64_t>(handoffBoundaryFrames));
        const auto atShortBoundary =
            shortEngine.snapshot().value()->audibleRealization;
        if (expectedTransitionFrames == 0) {
            QCOMPARE(
                atShortBoundary.phase,
                core::AudibleHandoffPhase::NEW);
            QCOMPARE(
                atShortBoundary.realizationId,
                std::optional{newRealizationId});
            QCOMPARE(
                atShortBoundary.handoffEndFrame,
                std::optional<std::int64_t>{0});
            std::fprintf(stderr,
                "DBG_LAMBDA_EXIT: short-crossfade loopEnd=%lld transition=%lld\\n",
                static_cast<long long>(loopEndFrame),
                static_cast<long long>(expectedTransitionFrames));
            std::fflush(stderr);
            return;
        }

        QCOMPARE(
            atShortBoundary.phase,
            core::AudibleHandoffPhase::TRANSITION);
        QVERIFY(!atShortBoundary.realizationId.has_value());

        observedShort->set_processed_frames(
            deviceProcessedBaseline
            + static_cast<std::int64_t>(
                handoffBoundaryFrames
                + static_cast<std::size_t>(expectedTransitionFrames - 1)));
        QCOMPARE(
            shortEngine.snapshot().value()->audibleRealization.phase,
            core::AudibleHandoffPhase::TRANSITION);

        observedShort->set_processed_frames(
            deviceProcessedBaseline
            + static_cast<std::int64_t>(
                handoffBoundaryFrames
                + static_cast<std::size_t>(expectedTransitionFrames)));
        const auto afterShort =
            shortEngine.snapshot().value()->audibleRealization;
        QCOMPARE(afterShort.phase, core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            afterShort.realizationId,
            std::optional{newRealizationId});
        QCOMPARE(
            afterShort.handoffEndFrame,
            std::optional<std::int64_t>{0});
        std::fprintf(stderr,
            "DBG_LAMBDA_EXIT: short-crossfade loopEnd=%lld transition=%lld\\n",
            static_cast<long long>(loopEndFrame),
            static_cast<long long>(expectedTransitionFrames));
        std::fflush(stderr);
    };

    qInfo() << "DBG_PLAYBACK_SUPPORT: seamless:2E-zero";
    verifyShortCrossfadeIdentity(
        static_cast<std::int64_t>(handoffBoundaryFrames), 0);
    qInfo() << "DBG_PLAYBACK_SUPPORT: seamless:2E-short";
    verifyShortCrossfadeIdentity(
        static_cast<std::int64_t>(handoffBoundaryFrames + 6U), 6);

    // Test 3: STOPPED replacement remains STOPPED without auto-start.
    QVERIFY(engine.stop());
    QCOMPARE(
        engine.snapshot().value()->state,
        core::PlaybackState::STOPPED);
    QVERIFY(engine.handoff_pcm(newBuf.view(), lifetime2));
    QCOMPARE(
        engine.snapshot().value()->state,
        core::PlaybackState::STOPPED);

    // Test 4: Atomic failure on incompatible candidate preserves old state.
    QVERIFY(engine.play());
    auto diffRateBuf =
        make_buffer(44100, audio::ChannelLayout::STEREO_LR, totalFrames);
    const auto realizationBeforeFailedHandoff =
        engine.snapshot().value()->audibleRealization;
    QVERIFY(!engine.handoff_pcm(
        diffRateBuf.view(), nullptr, core::RealizationId{99}));
    QCOMPARE(
        engine.snapshot().value()->state,
        core::PlaybackState::PLAYING);
    QCOMPARE(
        engine.snapshot().value()->audibleRealization,
        realizationBeforeFailedHandoff);

    // Test 5: A non-playing EOF cue canonicalizes to range.begin() without autoplay.
    QVERIFY(engine.seek(core::FrameIndex{totalFrames}));
    QCOMPARE(
        engine.snapshot().value()->position.value(),
        totalFrames);
    QCOMPARE(
        engine.snapshot().value()->state,
        core::PlaybackState::STOPPED);
    QVERIFY(engine.handoff_pcm(newBuf.view(), lifetime2));
    QCOMPARE(
        engine.snapshot().value()->position.value(),
        std::int64_t{0});
    QCOMPARE(
        engine.snapshot().value()->state,
        core::PlaybackState::STOPPED);

    // Test 6: Cumulative backend processed frames remain rebased correctly.
    PlaybackEngine engineRebase;
    auto rebaseOutput =
        std::make_unique<FakeOutput>(64U * 1024U, bytesPerFrame);
    auto* observedRebase = rebaseOutput.get();
    QVERIFY(engineRebase.install_pcm_candidate(
        oldBuf.view(),
        std::move(rebaseOutput),
        DeviceSampleFormat::PCM_S16,
        std::nullopt,
        lifetime1));
    QVERIFY(engineRebase.play());
    observedRebase->set_processed_frames(100);
    engineRebase.tick();
    QCOMPARE(
        engineRebase.snapshot().value()->position.value(),
        std::int64_t{100});

    QVERIFY(engineRebase.handoff_pcm(newBuf.view(), lifetime2));
    QCOMPARE(
        engineRebase.snapshot().value()->position.value(),
        std::int64_t{100});

    observedRebase->set_processed_frames(150);
    engineRebase.tick();
    QCOMPARE(
        engineRebase.snapshot().value()->position.value(),
        std::int64_t{150});

    // Test 7: paired-rate SRC crossfades are generated in OUTPUT_RATE frames
    // in both supported directions and do not stop/restart the output.
    const auto verifySrcCrossfade = [&](
        std::uint32_t inputRateValue,
        std::uint32_t outputRateValue,
        std::size_t expectedXfadeFrames) {
        std::fprintf(stderr,
            "DBG_LAMBDA_ENTER: src-crossfade input=%u output=%u xfade=%zu\\n",
            inputRateValue, outputRateValue, expectedXfadeFrames);
        std::fflush(stderr);
        auto srcInputRate = core::SampleRate::create(inputRateValue);
        auto srcOutputRate = core::SampleRate::create(outputRateValue);
        QVERIFY(srcInputRate && srcOutputRate);
        auto srcAdapter = audio::PlaybackSampleRateAdapter::create(
            audio::PlaybackRateSpec{
                *srcInputRate.value(),
                *srcOutputRate.value(),
                audio::ChannelLayout::STEREO_LR,
                frame_count(totalFrames),
            });
        QVERIFY(srcAdapter);

        PlaybackEngine srcEngine;
        auto srcOutput = std::make_unique<FakeOutput>(
            queuedCapacityFrames * bytesPerFrame, bytesPerFrame);
        auto* observedSrc = srcOutput.get();
        auto srcOld = make_buffer(
            inputRateValue,
            audio::ChannelLayout::STEREO_LR,
            totalFrames);
        auto srcNew = make_buffer(
            inputRateValue,
            audio::ChannelLayout::STEREO_LR,
            totalFrames);

        auto srcMutableOld = srcOld.mutable_view();
        auto srcMutableNew = srcNew.mutable_view();
        for (std::size_t ch = 0; ch < 2; ++ch) {
            auto oldCh = srcMutableOld.channel(ch);
            auto newCh = srcMutableNew.channel(ch);
            // Keep SRC qualification below full scale: FIR passband ripple can
            // cross unity by a sub-LSB amount, while PCM16 audition correctly
            // rejects canonical samples outside [-1.0, +1.0].
            std::fill(oldCh.value()->begin(), oldCh.value()->end(), 0.5);
            std::fill(newCh.value()->begin(), newCh.value()->end(), 0.0);
        }

        QVERIFY(srcEngine.install_pcm_candidate(
            srcOld.view(),
            std::move(srcOutput),
            DeviceSampleFormat::PCM_S16,
            std::move(*srcAdapter.value()),
            lifetime1,
            oldRealizationId));
        QVERIFY(srcEngine.play());
        QCOMPARE(
            srcEngine.snapshot().value()->state,
            core::PlaybackState::PLAYING);
        const int stopCallsBeforeSrcHandoff = observedSrc->stopCalls;
        QVERIFY(srcEngine.handoff_pcm(
            srcNew.view(), lifetime2, newRealizationId));
        QCOMPARE(
            srcEngine.snapshot().value()->state,
            core::PlaybackState::PLAYING);
        QCOMPARE(
            observedSrc->stopCalls,
            stopCallsBeforeSrcHandoff);

        const auto srcBeforeBoundary =
            srcEngine.snapshot().value()->audibleRealization;
        QCOMPARE(srcBeforeBoundary.phase, core::AudibleHandoffPhase::OLD);
        QCOMPARE(
            srcBeforeBoundary.realizationId,
            std::optional{oldRealizationId});
        observedSrc->set_processed_frames(
            static_cast<std::int64_t>(handoffBoundaryFrames));
        QCOMPARE(
            srcEngine.snapshot().value()->audibleRealization.phase,
            core::AudibleHandoffPhase::TRANSITION);
        observedSrc->set_processed_frames(
            static_cast<std::int64_t>(
                handoffBoundaryFrames + expectedXfadeFrames));
        const auto srcAfterBoundary =
            srcEngine.snapshot().value()->audibleRealization;
        QCOMPARE(srcAfterBoundary.phase, core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            srcAfterBoundary.realizationId,
            std::optional{newRealizationId});
        observedSrc->set_processed_frames(0);

        const auto requiredBytes =
            (handoffBoundaryFrames + expectedXfadeFrames + 1U)
            * bytesPerFrame;
        for (int iteration = 0;
             iteration < 8
                 && observedSrc->history().size() < requiredBytes;
             ++iteration) {
            observedSrc->consume_all();
            srcEngine.tick();
        }
        const auto& srcHistory = observedSrc->history();
        QVERIFY(srcHistory.size() >= requiredBytes);
        const auto oldBeforeBoundary = read_i16(
            srcHistory,
            (handoffBoundaryFrames - 1U) * bytesPerFrame);
        const auto oldAtBoundary = read_i16(
            srcHistory,
            handoffBoundaryFrames * bytesPerFrame);
        const auto midpoint = read_i16(
            srcHistory,
            (handoffBoundaryFrames + expectedXfadeFrames / 2U)
                * bytesPerFrame);
        QVERIFY(std::abs(static_cast<int>(oldBeforeBoundary) - 16384) <= 1);
        QVERIFY(std::abs(static_cast<int>(oldAtBoundary) - 16384) <= 1);
        QVERIFY(std::abs(static_cast<int>(midpoint) - 8192) <= 1);
        const auto lastCrossfadeSample = read_i16(
            srcHistory,
            (handoffBoundaryFrames + expectedXfadeFrames - 1U)
                * bytesPerFrame);
        QVERIFY(lastCrossfadeSample > 0);
        QVERIFY(lastCrossfadeSample < 100);
        QCOMPARE(
            read_i16(
                srcHistory,
                (handoffBoundaryFrames + expectedXfadeFrames)
                    * bytesPerFrame),
            static_cast<std::int16_t>(0));
        std::fprintf(stderr,
            "DBG_LAMBDA_EXIT: src-crossfade input=%u output=%u xfade=%zu\\n",
            inputRateValue, outputRateValue, expectedXfadeFrames);
        std::fflush(stderr);
    };

    qInfo() << "DBG_PLAYBACK_SUPPORT: seamless:src-441-480";
    verifySrcCrossfade(44'100U, 48'000U, 720U);
    qInfo() << "DBG_PLAYBACK_SUPPORT: seamless:src-480-441";
    verifySrcCrossfade(48'000U, 44'100U, 662U);

    // Identity-less PCM remains explicitly unavailable.
    {
        PlaybackEngine unavailableEngine;
        auto unavailableOutput = std::make_unique<FakeOutput>(
            queuedCapacityFrames * bytesPerFrame, bytesPerFrame);
        QVERIFY(unavailableEngine.install_pcm_candidate(
            oldBuf.view(),
            std::move(unavailableOutput),
            DeviceSampleFormat::PCM_S16,
            std::nullopt,
            lifetime1));
        const auto unavailable =
            unavailableEngine.snapshot().value()->audibleRealization;
        QCOMPARE(
            unavailable.phase,
            core::AudibleHandoffPhase::UNAVAILABLE);
        QVERIFY(!unavailable.realizationId.has_value());
    }

    // Test 9: an active Loop Region survives a PLAYING handoff. The
    // future-boundary crossfade occurs before the loop end, then playback
    // traverses the armed loop using only the new realization without a stop.
    {
        constexpr std::int64_t loopEndFrame = 3000;
        PlaybackEngine loopEngine;
        auto loopOutput = std::make_unique<FakeOutput>(
            queuedCapacityFrames * bytesPerFrame, bytesPerFrame);
        auto* observedLoop = loopOutput.get();

        auto loopOld = make_buffer(
            48000, audio::ChannelLayout::STEREO_LR, totalFrames);
        auto loopNew = make_buffer(
            48000, audio::ChannelLayout::STEREO_LR, totalFrames);
        auto loopMutableOld = loopOld.mutable_view();
        auto loopMutableNew = loopNew.mutable_view();
        for (std::size_t ch = 0; ch < 2; ++ch) {
            auto oldCh = loopMutableOld.channel(ch);
            auto newCh = loopMutableNew.channel(ch);
            std::fill(oldCh.value()->begin(), oldCh.value()->end(), 0.5);
            std::fill(newCh.value()->begin(), newCh.value()->end(), -0.5);
        }

        QVERIFY(loopEngine.install_pcm_candidate(
            loopOld.view(),
            std::move(loopOutput),
            DeviceSampleFormat::PCM_S16,
            std::nullopt,
            lifetime1,
            oldRealizationId));
        auto loopRange = core::FrameRange::create(
            core::FrameIndex{0},
            core::FrameIndex{loopEndFrame});
        QVERIFY(loopRange);
        QVERIFY(loopEngine.set_loop(*loopRange.value()));
        QVERIFY(loopEngine.play());

        const int stopCallsBeforeLoopHandoff = observedLoop->stopCalls;
        QVERIFY(loopEngine.handoff_pcm(
            loopNew.view(), lifetime2, newRealizationId));
        const auto loopBeforeBoundary =
            loopEngine.snapshot().value()->audibleRealization;
        QCOMPARE(loopBeforeBoundary.phase, core::AudibleHandoffPhase::OLD);
        QCOMPARE(
            loopBeforeBoundary.realizationId,
            std::optional{oldRealizationId});
        observedLoop->set_processed_frames(
            static_cast<std::int64_t>(handoffBoundaryFrames));
        QCOMPARE(
            loopEngine.snapshot().value()->audibleRealization.phase,
            core::AudibleHandoffPhase::TRANSITION);
        observedLoop->set_processed_frames(
            static_cast<std::int64_t>(
                handoffBoundaryFrames + xfadeFrames48k));
        const auto loopAfterBoundary =
            loopEngine.snapshot().value()->audibleRealization;
        QCOMPARE(loopAfterBoundary.phase, core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            loopAfterBoundary.realizationId,
            std::optional{newRealizationId});
        observedLoop->set_processed_frames(0);

        auto loopSnapshot = loopEngine.snapshot();
        QVERIFY(loopSnapshot);
        QCOMPARE(
            loopSnapshot.value()->state,
            core::PlaybackState::PLAYING);
        QVERIFY(loopSnapshot.value()->loop.has_value());
        QCOMPARE(
            loopSnapshot.value()->loop->begin().value(),
            std::int64_t{0});
        QCOMPARE(
            loopSnapshot.value()->loop->end().value(),
            loopEndFrame);
        QCOMPARE(
            observedLoop->stopCalls,
            stopCallsBeforeLoopHandoff);

        const auto requiredLoopHistoryBytes =
            (static_cast<std::size_t>(loopEndFrame) + 1U)
            * bytesPerFrame;
        for (int iteration = 0;
             iteration < 12
                 && observedLoop->history().size()
                     < requiredLoopHistoryBytes;
             ++iteration) {
            observedLoop->consume_all();
            loopEngine.tick();
        }

        const auto& loopHistory = observedLoop->history();
        QVERIFY(loopHistory.size() >= requiredLoopHistoryBytes);
        QVERIFY(
            std::abs(
                static_cast<int>(
                    read_i16(
                        loopHistory,
                        (handoffBoundaryFrames - 1U)
                            * bytesPerFrame))
                - 16384)
            <= 1);
        QVERIFY(
            std::abs(
                static_cast<int>(
                    read_i16(
                        loopHistory,
                        handoffBoundaryFrames * bytesPerFrame))
                - 16384)
            <= 1);
        QVERIFY(
            std::abs(
                static_cast<int>(
                    read_i16(
                        loopHistory,
                        (handoffBoundaryFrames + xfadeFrames48k)
                            * bytesPerFrame))
                + 16384)
            <= 1);

        // Linear output frame loopEndFrame is the first frame after the
        // traversal boundary, so it must be the new realization at loop begin.
        QVERIFY(
            std::abs(
                static_cast<int>(
                    read_i16(
                        loopHistory,
                        static_cast<std::size_t>(loopEndFrame)
                            * bytesPerFrame))
                + 16384)
            <= 1);
        QCOMPARE(
            observedLoop->stopCalls,
            stopCallsBeforeLoopHandoff);
        QCOMPARE(
            loopEngine.snapshot().value()->state,
            core::PlaybackState::PLAYING);
        const auto loopPostTraversal =
            loopEngine.snapshot().value()->audibleRealization;
        QCOMPARE(loopPostTraversal.phase, core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            loopPostTraversal.realizationId,
            std::optional{newRealizationId});
    }

    // Test 8: if all remaining old audio is already committed through natural
    // EOF, replacement must not append a frame-zero crossfade or replay.
    {
        constexpr std::int64_t shortFrames = 1000;
        PlaybackEngine engineQueued;
        auto outputQueued =
            std::make_unique<FakeOutput>(64U * 1024U, bytesPerFrame);
        auto* observedQueued = outputQueued.get();

        auto oldPcm = make_buffer(
            48000, audio::ChannelLayout::STEREO_LR, shortFrames);
        auto newPcm = make_buffer(
            48000, audio::ChannelLayout::STEREO_LR, shortFrames);

        auto queuedMutableOld = oldPcm.mutable_view();
        auto queuedMutableNew = newPcm.mutable_view();
        for (std::size_t ch = 0; ch < 2; ++ch) {
            auto oldCh = queuedMutableOld.channel(ch);
            auto newCh = queuedMutableNew.channel(ch);
            std::fill(oldCh.value()->begin(), oldCh.value()->end(), 1.0);
            std::fill(newCh.value()->begin(), newCh.value()->end(), -1.0);
        }

        QVERIFY(engineQueued.install_pcm_candidate(
            oldPcm.view(),
            std::move(outputQueued),
            DeviceSampleFormat::PCM_S16,
            std::nullopt,
            lifetime1,
            oldRealizationId));
        QVERIFY(engineQueued.play());
        const int stopCallsBeforeQueuedHandoff =
            observedQueued->stopCalls;

        const std::size_t initialQueueSize =
            observedQueued->queue().size();
        const std::size_t initialHistorySize =
            observedQueued->history().size();
        QCOMPARE(
            initialQueueSize,
            static_cast<std::size_t>(shortFrames) * bytesPerFrame);

        QVERIFY(engineQueued.handoff_pcm(
            newPcm.view(), lifetime2, newRealizationId));
        QCOMPARE(
            engineQueued.snapshot().value()->state,
            core::PlaybackState::PLAYING);
        QCOMPARE(
            engineQueued.snapshot().value()->audibleRealization.phase,
            core::AudibleHandoffPhase::OLD);
        QCOMPARE(
            engineQueued.snapshot().value()->audibleRealization.realizationId,
            std::optional{oldRealizationId});
        QCOMPARE(observedQueued->queue().size(), initialQueueSize);
        QCOMPARE(observedQueued->history().size(), initialHistorySize);
        QCOMPARE(
            observedQueued->stopCalls,
            stopCallsBeforeQueuedHandoff);

        observedQueued->consume_all();
        engineQueued.tick();
        QCOMPARE(
            engineQueued.snapshot().value()->state,
            core::PlaybackState::STOPPED);
        QCOMPARE(
            engineQueued.snapshot().value()->position.value(),
            shortFrames);
        QCOMPARE(observedQueued->history().size(), initialHistorySize);
        QCOMPARE(
            engineQueued.snapshot().value()->audibleRealization.phase,
            core::AudibleHandoffPhase::OLD);

        // The new realization is authoritative only for the next explicit play.
        QVERIFY(engineQueued.play());
        QCOMPARE(
            engineQueued.snapshot().value()->audibleRealization.phase,
            core::AudibleHandoffPhase::NEW);
        QCOMPARE(
            engineQueued.snapshot().value()->audibleRealization.realizationId,
            std::optional{newRealizationId});
        QVERIFY(
            observedQueued->history().size()
            >= initialHistorySize + bytesPerFrame);
        QCOMPARE(
            read_i16(observedQueued->history(), initialHistorySize),
            static_cast<std::int16_t>(-32768));
    }
    std::fprintf(stderr, "DBG_SLOT_EXIT: seamlessPcmHandoffCrossfadeAndStateMatrix\\n");
    std::fflush(stderr);
}

void PlaybackSupportTest::partialWritesNaturalEofAndRuntimeError()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: partialWritesNaturalEofAndRuntimeError\\n");
    std::fflush(stderr);
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
    std::fprintf(stderr, "DBG_SLOT_EXIT: partialWritesNaturalEofAndRuntimeError\\n");
    std::fflush(stderr);
}

void PlaybackSupportTest::controlPlaneTraversalSeekAndLoopSerials()
{
    std::fprintf(stderr, "DBG_SLOT_ENTER: controlPlaneTraversalSeekAndLoopSerials\\n");
    std::fflush(stderr);
    qInfo() << "DBG_PLAYBACK_SUPPORT: control-plane:start";
    PlaybackEngine engine;
    auto initialSnap = engine.snapshot();
    QVERIFY(initialSnap);
    QCOMPARE(initialSnap.value()->traversalSerial, std::uint64_t{0});
    QCOMPARE(initialSnap.value()->seekSerial, std::uint64_t{0});
    QCOMPARE(initialSnap.value()->loopWrapCount, std::uint64_t{0});

    const auto codes = indexed_stereo_codes(400);
    auto output = std::make_unique<FakeOutput>(64U * 1024U, 4U);
    auto* observedOutput = output.get();

    QVERIFY(engine.install_candidate(
        open_pcm16_stereo(codes),
        std::move(output),
        DeviceSampleFormat::PCM_S16));

    // Prepare / install alone MUST NOT start traversal or change seek/loop serials
    auto preparedSnap = engine.snapshot();
    QVERIFY(preparedSnap);
    QCOMPARE(preparedSnap.value()->traversalSerial, std::uint64_t{0});
    QCOMPARE(preparedSnap.value()->seekSerial, std::uint64_t{0});
    QCOMPARE(preparedSnap.value()->loopWrapCount, std::uint64_t{0});

    qInfo() << "DBG_PLAYBACK_SUPPORT: control-plane:before-play1";
    // 1st PLAY starts first traversal (traversalSerial = 1)
    QVERIFY(engine.play());
    auto play1Snap = engine.snapshot();
    QVERIFY(play1Snap);
    QCOMPARE(play1Snap.value()->traversalSerial, std::uint64_t{1});
    QCOMPARE(play1Snap.value()->seekSerial, std::uint64_t{0});

    // PAUSE -> RESUME MUST NOT increment traversalSerial
    QVERIFY(engine.pause());
    QCOMPARE(engine.snapshot().value()->traversalSerial, std::uint64_t{1});
    QVERIFY(engine.play());
    QCOMPARE(engine.snapshot().value()->traversalSerial, std::uint64_t{1});

    // STOP -> PLAY establishes a new traversal (traversalSerial = 2)
    QVERIFY(engine.stop());
    QCOMPARE(engine.snapshot().value()->traversalSerial, std::uint64_t{1});
    QVERIFY(engine.play());
    QCOMPARE(engine.snapshot().value()->traversalSerial, std::uint64_t{2});

    qInfo() << "DBG_PLAYBACK_SUPPORT: control-plane:before-handoff";
    // Active realization handoff MUST NOT increment traversalSerial
    const auto buf2 = make_buffer(48000, audio::ChannelLayout::STEREO_LR, 400);
    const core::RealizationId rid2{99};
    QVERIFY(engine.handoff_pcm(buf2.view(), nullptr, rid2));
    QCOMPARE(engine.snapshot().value()->traversalSerial, std::uint64_t{2});

    qInfo() << "DBG_PLAYBACK_SUPPORT: control-plane:before-seek";
    // Successful SEEK increments seekSerial exactly once without falsely creating a traversal event
    QVERIFY(engine.seek(core::FrameIndex{100}));
    auto seekSnap = engine.snapshot();
    QVERIFY(seekSnap);
    QCOMPARE(seekSnap.value()->seekSerial, std::uint64_t{1});
    QCOMPARE(seekSnap.value()->traversalSerial, std::uint64_t{2});

    // Failed SEEK changes no serial
    QVERIFY(!engine.seek(core::FrameIndex{-50}));
    auto failSeekSnap = engine.snapshot();
    QCOMPARE(failSeekSnap.value()->seekSerial, std::uint64_t{1});
    QCOMPARE(failSeekSnap.value()->traversalSerial, std::uint64_t{2});

    qInfo() << "DBG_PLAYBACK_SUPPORT: control-plane:before-loop";
    // set_loop internal restart does NOT falsely create a new traversal
    const auto loop = *core::FrameRange::create(
        core::FrameIndex{50}, core::FrameIndex{150}).value();
    QVERIFY(engine.set_loop(loop));
    auto loopSnap = engine.snapshot();
    QCOMPARE(loopSnap.value()->traversalSerial, std::uint64_t{2});
    QCOMPARE(loopSnap.value()->seekSerial, std::uint64_t{1});

    // loopWrapCount changes only when actual processed output crosses a loop boundary
    QCOMPARE(engine.snapshot().value()->loopWrapCount, std::uint64_t{0});

    qInfo() << "DBG_PLAYBACK_SUPPORT: control-plane:before-wrap";
    // Simulate hardware processed frames crossing loop boundary (length 100 frames)
    observedOutput->set_processed_frames(200);
    auto wrapSnap = engine.snapshot();
    QCOMPARE(wrapSnap.value()->loopWrapCount, std::uint64_t{2});
    std::fprintf(stderr, "DBG_SLOT_EXIT: controlPlaneTraversalSeekAndLoopSerials\\n");
    std::fflush(stderr);
}

}  // namespace rgsml::tests

int main(int argc, char** argv)
{
    rgsml::tests::PlaybackSupportTest testObject;
    const int result = QTest::qExec(&testObject, argc, argv);
    std::fprintf(stderr, "DBG_QEXEC_RESULT: %d\\n", result);
    std::fflush(stderr);
    return result;
}

#include "test_playback_support.moc"
