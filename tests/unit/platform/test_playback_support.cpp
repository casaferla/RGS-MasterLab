#include "playback_support.hpp"
#include "c2_consumed_prefix_projection.hpp"
#include "internal/true_peak_chronological_buckets.hpp"

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
    void hotTogglePreservesQueuedPcmAndSink();
    void hotTogglePreservesPendingHandoffProvenance();
    void partialWritesNaturalEofAndRuntimeError();
    void seamlessPcmHandoffCrossfadeAndStateMatrix();
    void controlPlaneTraversalSeekAndLoopSerials();
    void c2D1ProcessedWatermarkFailClosedAndGeneration();
    void c2D1UnknownTargetHandoffAndLoopAreNotCoverage();
    void c2D2AcceptedRunLedgerPartialWriteAndClock();
    void c2D2AcceptedRunLedgerOldOwnerResetAndFailClosed();
    void c2D2AcceptedRunLedgerOverflowIsSticky();
    void c2D2SrcAcceptedRunsAreNotExactSourceCoverage();
    void c2D3CrossfadeAppendedToOldPendingCannotInheritSource();
    void c2D3FreshCrossfadePendingCannotInheritStaleSource();
    void c2D4ConsumedPrefixNoFuturePartialWritesAndSeek();
    void c2D4ConsumedPrefixSrcAndHandoffExposeGaps();
    void c2D4ConsumedPrefixLongLoopGapIsConserved();
    void c2D4QueryIsObservationalAcrossLoopAndSeek();
    void c2D4PauseDoesNotAdvanceConsumedPrefix();
    void c2D5SinkWatermarkFeedsRealNumericalRecentOnlyAfterFullBucket();
    void c2D6WorkerProjectionPreservesConsumedWatermarkAndGap();
    void c2D6WorkerProjectionNeverCertifiesProcessedTarget();
    void c2D9RealSinkSeekMaintainsRelativeOutputAndAbsoluteSource();
    void c2D10LoopOffRecoversFreshSourceWithoutRestart();
    void c2D10Native44100IsNotInherentlyUnavailable();
    void c2D12HandoffRecoversConsumedPureNewSuffixWithoutRestart();
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
                // PAUSED retains already queued PCM; restarting would drop
                // buffered samples and recreate the platform audio sink.
                QVERIFY(!observed->queue().empty());
                QCOMPARE(observed->startCalls, 1);
                QVERIFY(engine.play());
                QCOMPARE(observed->resumeCalls, 1);
                QCOMPARE(observed->startCalls, 1);
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

void PlaybackSupportTest::hotTogglePreservesQueuedPcmAndSink()
{
    // The scheduler may have generated PCM beyond the sink's bounded byte
    // queue. Toggle must preserve both enqueued and pending PCM, even after
    // those samples would have crossed the newly armed Loop boundary.
    constexpr std::int64_t duration = 240;
    constexpr std::size_t bytesPerFrame = 4U;
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(16U * bytesPerFrame, bytesPerFrame);
    auto* observed = output.get();
    QVERIFY(engine.install_candidate(
        open_pcm16_stereo(indexed_stereo_codes(duration)),
        std::move(output), DeviceSampleFormat::PCM_S16));
    QVERIFY(engine.seek(core::FrameIndex{60}));
    QVERIFY(engine.play());
    const auto loop = *core::FrameRange::create(
        core::FrameIndex{40}, core::FrameIndex{80}).value();
    const int initialStarts = observed->startCalls;
    const int initialStops = observed->stopCalls;
    const auto originalHistory = observed->history();

    QVERIFY(engine.set_loop(loop));
    QCOMPARE(observed->startCalls, initialStarts);
    QCOMPARE(observed->stopCalls, initialStops);
    QCOMPARE(engine.snapshot().value()->state, core::PlaybackState::PLAYING);
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{60});
    QCOMPARE(observed->history(), originalHistory);
    // 180 pre-generated old frames remain first; only subsequent decoded
    // frames enter the new Loop. No PCM is flushed, skipped, or duplicated.
    constexpr std::size_t expectedFrames = 180U + 80U;
    for (int i = 0;
         i < 150 && observed->history().size() < expectedFrames * bytesPerFrame;
         ++i) {
        observed->consume_all();
        engine.tick();
    }
    QVERIFY(observed->history().size() >= expectedFrames * bytesPerFrame);
    const auto& history = observed->history();
    for (std::size_t frame = 0; frame < expectedFrames; ++frame) {
        const auto expected = frame < 180U
            ? static_cast<std::int16_t>(60 + frame)
            : static_cast<std::int16_t>(40 + ((frame - 180U) % 40U));
        QCOMPARE(read_i16(history, frame * bytesPerFrame), expected);
    }
    QCOMPARE(observed->startCalls, initialStarts);
    QCOMPARE(observed->stopCalls, initialStops);

    // Loop wrap telemetry reads the device counter directly, while the
    // snapshot's position_ is updated by the owner-thread tick. Simulate the
    // latter at each asserted audible instant (as the actual worker does).
    observed->set_processed_frames(179);
    engine.tick();
    QCOMPARE(engine.snapshot().value()->loopWrapCount, std::uint64_t{0});
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{239});
    observed->set_processed_frames(180);
    engine.tick();
    QCOMPARE(engine.snapshot().value()->loopWrapCount, std::uint64_t{0});
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{40});
    observed->set_processed_frames(220);
    engine.tick();
    QCOMPARE(engine.snapshot().value()->loopWrapCount, std::uint64_t{1});
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{40});

    // Already committed loop frames remain valid when disabling. Rapid
    // re-arming is allowed without any device restart or queue deletion.
    const auto queueBeforeDisable = observed->queue();
    QVERIFY(engine.set_loop(std::nullopt));
    QCOMPARE(observed->queue(), queueBeforeDisable);
    QCOMPARE(observed->startCalls, initialStarts);
    QCOMPARE(observed->stopCalls, initialStops);
    QVERIFY(engine.set_loop(loop));
    QVERIFY(engine.set_loop(std::nullopt));
    QCOMPARE(observed->startCalls, initialStarts);
    QCOMPARE(observed->stopCalls, initialStops);
    QCOMPARE(engine.snapshot().value()->traversalSerial, std::uint64_t{1});
    QCOMPARE(engine.snapshot().value()->seekSerial, std::uint64_t{1});

    // PAUSED toggles use the existing suspended sink and unchanged FIFO.
    QVERIFY(engine.pause());
    const auto pausedQueue = observed->queue();
    QVERIFY(engine.set_loop(loop));
    QCOMPARE(observed->queue(), pausedQueue);
    QCOMPARE(observed->startCalls, initialStarts);
    QCOMPARE(observed->stopCalls, initialStops);
    QVERIFY(engine.play());
    QCOMPARE(observed->resumeCalls, 1);
    QCOMPARE(observed->startCalls, initialStarts);
    QCOMPARE(observed->stopCalls, initialStops);

    // Reversing an uncommitted hot-toggle must also preserve the decoder
    // cursor, not just the sink. Otherwise ON/OFF can falsely jump to the
    // Loop start after an entirely prefetched Source.
    PlaybackEngine cancelled;
    auto cancelledOutput =
        std::make_unique<FakeOutput>(16U * bytesPerFrame, bytesPerFrame);
    auto* cancelledObserved = cancelledOutput.get();
    QVERIFY(cancelled.install_candidate(
        open_pcm16_stereo(indexed_stereo_codes(duration)),
        std::move(cancelledOutput), DeviceSampleFormat::PCM_S16));
    QVERIFY(cancelled.seek(core::FrameIndex{60}));
    QVERIFY(cancelled.play());
    const int cancelledStartBaseline = cancelledObserved->startCalls;
    const int cancelledStopBaseline = cancelledObserved->stopCalls;
    QVERIFY(cancelled.set_loop(loop));
    QVERIFY(cancelled.set_loop(std::nullopt));
    QCOMPARE(cancelledObserved->startCalls, cancelledStartBaseline);
    QCOMPARE(cancelledObserved->stopCalls, cancelledStopBaseline);
    for (int i = 0; i < 100
             && cancelled.snapshot().value()->state
                 == core::PlaybackState::PLAYING; ++i) {
        cancelledObserved->consume_all();
        cancelled.tick();
    }
    QCOMPARE(cancelled.snapshot().value()->state, core::PlaybackState::STOPPED);
    QCOMPARE(cancelledObserved->history().size(),
             static_cast<std::size_t>(duration - 60) * bytesPerFrame);
    QCOMPARE(read_i16(cancelledObserved->history(), 0U),
             static_cast<std::int16_t>(60));
}

void PlaybackSupportTest::hotTogglePreservesPendingHandoffProvenance()
{
    // A PCM handoff's earlier queued frames must retain their OLD identity
    // even when a subsequent Loop toggle changes the future decode policy.
    auto oldBuffer = make_buffer(48000, audio::ChannelLayout::STEREO_LR, 5000);
    auto newBuffer = make_buffer(48000, audio::ChannelLayout::STEREO_LR, 5000);
    constexpr core::RealizationId oldId{41};
    constexpr core::RealizationId newId{42};
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(4096U, 4U);
    auto* observed = output.get();
    QVERIFY(engine.install_pcm_candidate(
        oldBuffer.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr, oldId));
    const auto loop = *core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{3000}).value();
    QVERIFY(engine.set_loop(loop));
    QVERIFY(engine.play());
    QVERIFY(engine.handoff_pcm(newBuffer.view(), nullptr, newId));
    const auto before = engine.snapshot().value()->audibleRealization;
    QCOMPARE(before.phase, core::AudibleHandoffPhase::OLD);
    QCOMPARE(before.realizationId, std::optional{oldId});
    const auto starts = observed->startCalls;
    const auto stops = observed->stopCalls;
    QVERIFY(engine.set_loop(std::nullopt));
    const auto after = engine.snapshot().value()->audibleRealization;
    QCOMPARE(after.phase, core::AudibleHandoffPhase::OLD);
    QCOMPARE(after.realizationId, std::optional{oldId});
    QCOMPARE(observed->startCalls, starts);
    QCOMPARE(observed->stopCalls, stops);
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

    // Test 2E: zero- and short-crossfade boundaries have deterministic
    // identity semantics: zero skips TRANSITION, short remains TRANSITION
    // for exactly the bounded output-frame interval.
    const auto verifyShortCrossfadeIdentity = [&](
        std::int64_t loopEndFrame,
        std::int64_t expectedTransitionFrames) {
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
        QVERIFY(shortEngine.play());
        // FakeOutput::start() resets the backend processed-frame counter.
        // Capture the actual post-start epoch for the audible boundary.
        const auto deviceProcessedBaseline =
            observedShort->processed_frames();
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
    };

    verifyShortCrossfadeIdentity(
        static_cast<std::int64_t>(handoffBoundaryFrames), 0);
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
    };

    verifySrcCrossfade(44'100U, 48'000U, 720U);
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

void PlaybackSupportTest::controlPlaneTraversalSeekAndLoopSerials()
{
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

    // Active realization handoff MUST NOT increment traversalSerial
    const auto buf2 = make_buffer(48000, audio::ChannelLayout::STEREO_LR, 400);
    const core::RealizationId rid2{99};
    QVERIFY(engine.handoff_pcm(buf2.view(), nullptr, rid2));
    QCOMPARE(engine.snapshot().value()->traversalSerial, std::uint64_t{2});

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

    // A Loop hot-toggle neither restarts output nor creates a new traversal
    const auto loop = *core::FrameRange::create(
        core::FrameIndex{50}, core::FrameIndex{150}).value();
    QVERIFY(engine.set_loop(loop));
    auto loopSnap = engine.snapshot();
    QCOMPARE(loopSnap.value()->traversalSerial, std::uint64_t{2});
    QCOMPARE(loopSnap.value()->seekSerial, std::uint64_t{1});

    // loopWrapCount changes only when actual processed output crosses a loop boundary
    QCOMPARE(engine.snapshot().value()->loopWrapCount, std::uint64_t{0});

    // The seek at 100 prefills 300 old source frames before the toggle.
    // Hardware processing those frames must not count speculative Loop wraps.
    observedOutput->set_processed_frames(200);
    QCOMPARE(engine.snapshot().value()->loopWrapCount, std::uint64_t{0});
    observedOutput->set_processed_frames(300);
    QCOMPARE(engine.snapshot().value()->loopWrapCount, std::uint64_t{0});
    // A subsequent 200 processed frames in the new [50,150) region wrap twice.
    observedOutput->set_processed_frames(500);
    auto wrapSnap = engine.snapshot();
    QCOMPARE(wrapSnap.value()->loopWrapCount, std::uint64_t{2});
}

void PlaybackSupportTest::c2D1ProcessedWatermarkFailClosedAndGeneration()
{
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    PlaybackEngine engine;
    const auto empty = engine.c2_processed_coverage_probe();
    QCOMPARE(empty.generation, std::uint64_t{0});
    QCOMPARE(empty.reason, Reason::NOT_MONITORED);
    QVERIFY(!empty.completeThroughWatermark);

    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 16);
    auto output = std::make_unique<FakeOutput>(128U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(
        pcm.view(), std::move(output), DeviceSampleFormat::PCM_S16,
        std::nullopt, nullptr, core::RealizationId{401}));
    const auto ready = engine.c2_processed_coverage_probe();
    QCOMPARE(ready.reason, Reason::NOT_MONITORED);
    QVERIFY(ready.generation > empty.generation);

    QVERIFY(engine.play());
    const auto active = engine.c2_processed_coverage_probe();
    QCOMPARE(active.reason, Reason::MISSING_CHRONOLOGICAL_LEDGER);
    QCOMPARE(active.processedWatermarkExclusive, std::int64_t{0});
    QVERIFY(active.generatedOutputFrames >= 16);
    QVERIFY(!active.completeThroughWatermark);

    sink->consume_all();
    const auto consumed = engine.c2_processed_coverage_probe(active.generation);
    QCOMPARE(consumed.processedWatermarkExclusive, std::int64_t{16});
    QCOMPARE(consumed.reason, Reason::MISSING_CHRONOLOGICAL_LEDGER);
    QVERIFY(!consumed.completeThroughWatermark);

    sink->set_processed_frames(1000);
    const auto ahead = engine.c2_processed_coverage_probe();
    QCOMPARE(ahead.reason, Reason::CLOCK_AHEAD_OF_GENERATED);
    QVERIFY(!ahead.completeThroughWatermark);

    QVERIFY(engine.seek(core::FrameIndex{3}));
    const auto afterSeek = engine.c2_processed_coverage_probe();
    QVERIFY(afterSeek.generation > consumed.generation);
    const auto stale = engine.c2_processed_coverage_probe(consumed.generation);
    QCOMPARE(stale.reason, Reason::STALE_GENERATION);
    QVERIFY(!stale.completeThroughWatermark);
}

void PlaybackSupportTest::c2D1UnknownTargetHandoffAndLoopAreNotCoverage()
{
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 2000);
    auto another = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 2000);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(512U, 4U);
    QVERIFY(engine.install_pcm_candidate(
        pcm.view(), std::move(output), DeviceSampleFormat::PCM_S16,
        std::nullopt, nullptr, core::RealizationId{402}));
    QVERIFY(engine.play());
    QVERIFY(engine.handoff_pcm(another.view(), nullptr, core::RealizationId{403}));
    const auto handoff = engine.c2_processed_coverage_probe();
    QCOMPARE(handoff.reason, Reason::UNPROVEN_HANDOFF);
    QVERIFY(!handoff.completeThroughWatermark);

    QVERIFY(engine.stop());
    const auto afterStop = engine.c2_processed_coverage_probe();
    QCOMPARE(afterStop.reason, Reason::NOT_MONITORED);
    QVERIFY(afterStop.generation > handoff.generation);

    const auto loop = *core::FrameRange::create(
        core::FrameIndex{100}, core::FrameIndex{300}).value();
    QVERIFY(engine.set_loop(loop));
    QVERIFY(engine.seek(core::FrameIndex{100}));
    QVERIFY(engine.play());
    const auto looping = engine.c2_processed_coverage_probe();
    QCOMPARE(looping.reason, Reason::UNPROVEN_LOOP);
    QVERIFY(!looping.completeThroughWatermark);
}

void PlaybackSupportTest::c2D2AcceptedRunLedgerPartialWriteAndClock()
{
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    // A two-byte device write may complete only half a stereo PCM16 frame.
    // The observer must materialize only wholly accepted output frames.
    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 16);
    PlaybackEngine engine;
    auto out = std::make_unique<FakeOutput>(12U, 4U, 2U);
    auto* sink = out.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(out),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{501}));
    QVERIFY(engine.play());
    const auto initial = engine.c2_processed_coverage_probe();
    QCOMPARE(initial.acceptedOutputFrames, std::int64_t{3});
    QCOMPARE(initial.acceptedRunCount, std::size_t{1});
    QVERIFY(!initial.acceptedLedgerOverflow);
    QVERIFY(!initial.completeThroughWatermark);
    QCOMPARE(engine.c2_accepted_output_runs().size(), std::size_t{1});
    const auto first = engine.c2_accepted_output_runs().front();
    QCOMPARE(first.generation, initial.generation);
    QCOMPARE(first.outputBegin, std::int64_t{0});
    QCOMPARE(first.outputEnd, std::int64_t{3});
    QCOMPARE(first.sourceBegin, std::optional<std::int64_t>{0});
    QCOMPARE(first.sourceEnd, std::optional<std::int64_t>{3});
    QCOMPARE(first.realizationId, std::optional{core::RealizationId{501}});
    QVERIFY(first.exactOneToOne);

    sink->set_processed_frames(2);
    const auto partial = engine.c2_processed_coverage_probe();
    QCOMPARE(partial.processedWatermarkExclusive, std::int64_t{2});
    QCOMPARE(partial.reason, Reason::MISSING_CHRONOLOGICAL_LEDGER);
    QVERIFY(!partial.completeThroughWatermark);

    sink->set_processed_frames(4); // fabricated sink clock beyond 3 accepted
    const auto impossible = engine.c2_processed_coverage_probe();
    QCOMPARE(impossible.reason, Reason::CLOCK_AHEAD_OF_ACCEPTED);
    QVERIFY(!impossible.completeThroughWatermark);

    sink->set_processed_frames(0);
    sink->consume_all();
    engine.tick();
    const auto afterTick = engine.c2_processed_coverage_probe();
    QVERIFY(afterTick.acceptedOutputFrames > initial.acceptedOutputFrames);
    QVERIFY(!afterTick.completeThroughWatermark);
    const auto runs = engine.c2_accepted_output_runs();
    QCOMPARE(runs.size(), std::size_t{1}); // coalesced consecutive source runs
    QCOMPARE(runs.front().outputBegin, std::int64_t{0});
    QCOMPARE(runs.front().outputEnd, afterTick.acceptedOutputFrames);
}

void PlaybackSupportTest::c2D2AcceptedRunLedgerOldOwnerResetAndFailClosed()
{
    auto oldPcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 2500);
    auto newPcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 2500);
    PlaybackEngine engine;
    auto out = std::make_unique<FakeOutput>(128U, 4U);
    QVERIFY(engine.install_pcm_candidate(oldPcm.view(), std::move(out),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{601}));
    QVERIFY(engine.play());
    const auto oldRun = engine.c2_accepted_output_runs().front();
    QCOMPARE(oldRun.realizationId, std::optional{core::RealizationId{601}});
    QVERIFY(oldRun.exactOneToOne);
    const auto oldGeneration = oldRun.generation;

    QVERIFY(engine.handoff_pcm(newPcm.view(), nullptr, core::RealizationId{602}));
    const auto probe = engine.c2_processed_coverage_probe();
    QVERIFY(!probe.completeThroughWatermark);
    // Queued OLD PCM retains its owner stamp even after candidate NEW is set.
    QCOMPARE(engine.c2_accepted_output_runs().front().realizationId,
        std::optional{core::RealizationId{601}});
    QVERIFY(engine.stop());
    QCOMPARE(engine.c2_accepted_output_runs().size(), std::size_t{0});
    const auto afterReset = engine.c2_processed_coverage_probe(oldGeneration);
    QCOMPARE(afterReset.reason,
        platform::windows::internal::C2CoverageProbeReason::STALE_GENERATION);
}

void PlaybackSupportTest::c2D2AcceptedRunLedgerOverflowIsSticky()
{
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    // Deliberate alternating direct/loop history defeats safe GAP compaction:
    // fragmented provenance must remain bounded and overflow fail-closed.
    // Full decoded blocks, rather than a 1-frame sink that merely drains
    // the same 1024-frame pending container for hundreds of iterations:
    // each policy switch then contributes a fresh owner-stamped or GAP run.
    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 500'000);
    PlaybackEngine engine;
    auto out = std::make_unique<FakeOutput>(4096U, 4U);
    auto* sink = out.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(out),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{701}));
    const auto loop = *core::FrameRange::create(
        core::FrameIndex{100}, core::FrameIndex{400}).value();
    QVERIFY(engine.set_loop(loop));
    QVERIFY(engine.play());
    for (int i = 0; i < 480; ++i) {
        QVERIFY(engine.set_loop(i % 2 == 0
            ? std::optional{loop} : std::nullopt));
        sink->consume_all();
        engine.tick();
        if (engine.c2_processed_coverage_probe().acceptedLedgerOverflow) break;
    }
    const auto probe = engine.c2_processed_coverage_probe();
    QVERIFY(probe.acceptedLedgerOverflow);
    QCOMPARE(probe.reason, Reason::ACCEPTED_LEDGER_OVERFLOW);
    QCOMPARE(probe.acceptedRunCount, std::size_t{128});
    QVERIFY(probe.acceptedOutputFrames > 128);
    QVERIFY(!probe.completeThroughWatermark);
    for (const auto& run : engine.c2_accepted_output_runs()) {
        QCOMPARE(run.generation, probe.generation);
    }
    QVERIFY(engine.stop());
    const auto cleared = engine.c2_processed_coverage_probe();
    QVERIFY(!cleared.acceptedLedgerOverflow);
    QCOMPARE(cleared.acceptedRunCount, std::size_t{0});
}

void PlaybackSupportTest::c2D2SrcAcceptedRunsAreNotExactSourceCoverage()
{
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    auto inputRate = core::SampleRate::create(44'100);
    auto outputRate = core::SampleRate::create(48'000);
    QVERIFY(inputRate && outputRate);
    auto converter = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{*inputRate.value(), *outputRate.value(),
            audio::ChannelLayout::STEREO_LR, frame_count(441)});
    QVERIFY(converter);
    auto pcm = make_buffer(44'100, audio::ChannelLayout::STEREO_LR, 441);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(128U, 4U);
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::move(*converter.value()),
        nullptr, core::RealizationId{702}));
    QVERIFY(engine.play());
    const auto probe = engine.c2_processed_coverage_probe();
    QCOMPARE(probe.reason, Reason::UNPROVEN_SRC);
    QVERIFY(!probe.completeThroughWatermark);
    QVERIFY(probe.acceptedRunCount > 0U);
    for (const auto& run : engine.c2_accepted_output_runs()) {
        QVERIFY(!run.exactOneToOne);
        QVERIFY(!run.sourceBegin.has_value());
        QVERIFY(!run.sourceEnd.has_value());
    }
}

void PlaybackSupportTest::c2D3CrossfadeAppendedToOldPendingCannotInheritSource()
{
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    constexpr auto oldId = core::RealizationId{801};
    auto oldPcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 2500);
    auto newPcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 2500);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(128U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(oldPcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr, oldId));
    QVERIFY(engine.play());
    const auto before = engine.c2_accepted_output_runs();
    QVERIFY(!before.empty());
    const auto provedOld = before.front();
    QVERIFY(provedOld.exactOneToOne);
    QCOMPARE(provedOld.realizationId, std::optional{oldId});

    // A 128-byte sink leaves unaccepted OLD bytes in pendingBytes_ when the
    // crossfade is appended. Only the *already accepted* prefix stays exact.
    QVERIFY(engine.handoff_pcm(newPcm.view(), nullptr, core::RealizationId{802}));
    for (int i = 0; i < 4; ++i) {
        sink->consume_all();
        engine.tick();
    }
    const auto runs = engine.c2_accepted_output_runs();
    QVERIFY(runs.size() > before.size());
    QCOMPARE(runs.front().sourceBegin, provedOld.sourceBegin);
    QCOMPARE(runs.front().sourceEnd, provedOld.sourceEnd);
    for (std::size_t i = before.size(); i < runs.size(); ++i) {
        QVERIFY(!runs[i].exactOneToOne);
        QVERIFY(!runs[i].sourceBegin.has_value());
        QVERIFY(!runs[i].sourceEnd.has_value());
        QVERIFY(!runs[i].realizationId.has_value());
    }
    const auto probe = engine.c2_processed_coverage_probe();
    QCOMPARE(probe.reason, Reason::UNPROVEN_HANDOFF);
    QVERIFY(!probe.completeThroughWatermark);
}

void PlaybackSupportTest::c2D3FreshCrossfadePendingCannotInheritStaleSource()
{
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    auto oldPcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 3500);
    auto newPcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 3500);
    PlaybackEngine engine;
    // Exactly one 1024-frame decode block: OLD pendingBytes_ is empty after
    // prefill, but its last c2PendingOwner_ stamp still names OLD.
    auto output = std::make_unique<FakeOutput>(4096U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(oldPcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{811}));
    QVERIFY(engine.play());
    const auto first = engine.c2_accepted_output_runs();
    QCOMPARE(first.size(), std::size_t{1});
    QVERIFY(first.front().exactOneToOne);
    QCOMPARE(first.front().outputEnd, std::int64_t{1024});
    QVERIFY(engine.handoff_pcm(newPcm.view(), nullptr,
        core::RealizationId{812}));
    sink->consume_all();
    engine.tick();
    const auto after = engine.c2_accepted_output_runs();
    QVERIFY(after.size() >= std::size_t{2});
    // A freshly encoded mixed block may not inherit the stale OLD stamp.
    const auto& mixed = after[1];
    QCOMPARE(mixed.outputBegin, std::int64_t{1024});
    QVERIFY(!mixed.exactOneToOne);
    QVERIFY(!mixed.realizationId.has_value());
    QVERIFY(!mixed.sourceBegin.has_value());
    QVERIFY(!mixed.sourceEnd.has_value());
    const auto p = engine.c2_processed_coverage_probe();
    QCOMPARE(p.reason, Reason::UNPROVEN_HANDOFF);
    QVERIFY(!p.completeThroughWatermark);

    QVERIFY(engine.stop());
    const auto reset = engine.c2_processed_coverage_probe();
    QVERIFY(!reset.completeThroughWatermark);
    QCOMPARE(reset.reason, Reason::NOT_MONITORED);
    QCOMPARE(reset.acceptedRunCount, std::size_t{0});
}

void PlaybackSupportTest::c2D4ConsumedPrefixNoFuturePartialWritesAndSeek()
{
    using Kind = platform::windows::internal::C2ConsumedSliceKind;
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 40);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(16U, 4U, 2U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{901}));
    QVERIFY(engine.play());
    auto empty = engine.c2_consumed_prefix();
    QCOMPARE(empty.processedWatermarkExclusive, std::int64_t{0});
    QCOMPARE(empty.size, std::size_t{0});
    QVERIFY(!empty.partitionRepresented);
    QVERIFY(!empty.certifiedForLiveTruePeak);

    // The device accepted four whole frames. Do not report frame #3 as
    // consumed merely because it was generated or is already queued.
    sink->set_processed_frames(3);
    const auto partial = engine.c2_consumed_prefix();
    QCOMPARE(partial.processedWatermarkExclusive, std::int64_t{3});
    QCOMPARE(partial.size, std::size_t{1});
    QVERIFY(partial.partitionRepresented);
    QCOMPARE(partial.slices[0].kind, Kind::SOURCE_CANDIDATE_NOT_QUALIFIED);
    QCOMPARE(partial.slices[0].outputBegin, std::int64_t{0});
    QCOMPARE(partial.slices[0].outputEnd, std::int64_t{3});
    QCOMPARE(partial.slices[0].sourceBegin, std::optional<std::int64_t>{0});
    QCOMPARE(partial.slices[0].sourceEnd, std::optional<std::int64_t>{3});
    QVERIFY(!partial.certifiedForLiveTruePeak);

    sink->set_processed_frames(5); // deliberately fabricated future clock
    const auto impossible = engine.c2_consumed_prefix();
    QCOMPARE(impossible.reason, Reason::CLOCK_AHEAD_OF_ACCEPTED);
    QCOMPARE(impossible.size, std::size_t{0});
    QVERIFY(!impossible.partitionRepresented);

    QVERIFY(engine.seek(core::FrameIndex{10}));
    const auto stale = engine.c2_consumed_prefix(partial.generation);
    QCOMPARE(stale.reason, Reason::STALE_GENERATION);
    QCOMPARE(stale.size, std::size_t{0});
    const auto newEpoch = engine.c2_consumed_prefix();
    QVERIFY(newEpoch.generation > partial.generation);
    QVERIFY(!newEpoch.certifiedForLiveTruePeak);
}

void PlaybackSupportTest::c2D4ConsumedPrefixSrcAndHandoffExposeGaps()
{
    using Kind = platform::windows::internal::C2ConsumedSliceKind;
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    auto inputRate = core::SampleRate::create(44'100);
    auto deviceRate = core::SampleRate::create(48'000);
    QVERIFY(inputRate && deviceRate);
    auto src = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{*inputRate.value(), *deviceRate.value(),
            audio::ChannelLayout::STEREO_LR, frame_count(600)});
    QVERIFY(src);
    auto pcm = make_buffer(44'100, audio::ChannelLayout::STEREO_LR, 600);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(256U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::move(*src.value()),
        nullptr, core::RealizationId{902}));
    QVERIFY(engine.play());
    sink->set_processed_frames(15);
    const auto q = engine.c2_consumed_prefix();
    QVERIFY(q.partitionRepresented);
    QCOMPARE(q.processedWatermarkExclusive, std::int64_t{15});
    QCOMPARE(q.size, std::size_t{1});
    QCOMPARE(q.slices[0].kind, Kind::GAP_UNPROVEN);
    QCOMPARE(q.slices[0].reason, Reason::UNPROVEN_SRC);
    QVERIFY(!q.slices[0].sourceBegin.has_value());
    QVERIFY(!q.certifiedForLiveTruePeak);

    PlaybackEngine handoff;
    auto oldBuffer = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 2600);
    auto newBuffer = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 2600);
    auto out = std::make_unique<FakeOutput>(128U, 4U);
    auto* watch = out.get();
    QVERIFY(handoff.install_pcm_candidate(oldBuffer.view(), std::move(out),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{903}));
    QVERIFY(handoff.play());
    QVERIFY(handoff.handoff_pcm(newBuffer.view(), nullptr,
        core::RealizationId{904}));
    watch->consume_all();
    handoff.tick();
    watch->set_processed_frames(64);
    const auto transition = handoff.c2_consumed_prefix();
    QVERIFY(transition.partitionRepresented);
    QVERIFY(transition.size >= std::size_t{2});
    QCOMPARE(transition.slices[0].kind, Kind::SOURCE_CANDIDATE_NOT_QUALIFIED);
    QCOMPARE(transition.slices[1].kind, Kind::GAP_UNPROVEN);
    QCOMPARE(transition.slices[1].reason, Reason::UNPROVEN_HANDOFF);
    QVERIFY(!transition.certifiedForLiveTruePeak);
}

void PlaybackSupportTest::c2D4ConsumedPrefixLongLoopGapIsConserved()
{
    using Kind = platform::windows::internal::C2ConsumedSliceKind;
    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 32);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(4U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{905}));
    const auto loop = *core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{1}).value();
    QVERIFY(engine.set_loop(loop));
    QVERIFY(engine.play());
    for (int i = 0; i < 133; ++i) {
        sink->consume_all();
        engine.tick();
    }
    const auto probe = engine.c2_processed_coverage_probe();
    QVERIFY(!probe.acceptedLedgerOverflow);
    QCOMPARE(probe.acceptedRunCount, std::size_t{1});
    const auto partition = engine.c2_consumed_prefix();
    QVERIFY(partition.partitionRepresented);
    QCOMPARE(partition.processedWatermarkExclusive, probe.processedWatermarkExclusive);
    QCOMPARE(partition.size, std::size_t{1});
    QCOMPARE(partition.slices[0].kind, Kind::GAP_UNPROVEN);
    QCOMPARE(partition.slices[0].outputEnd, probe.processedWatermarkExclusive);
    QVERIFY(partition.slices[0].outputEnd > partition.slices[0].outputBegin);
    QVERIFY(!partition.certifiedForLiveTruePeak);
    auto expected = std::int64_t{0};
    for (const auto& slice : partition.view()) {
        QCOMPARE(slice.outputBegin, expected);
        QVERIFY(slice.outputEnd > slice.outputBegin);
        expected = slice.outputEnd;
    }
    QCOMPARE(expected, probe.processedWatermarkExclusive);
}

void PlaybackSupportTest::c2D4QueryIsObservationalAcrossLoopAndSeek()
{
    // Independent replay oracle: querying C2 metadata does not alter PCM
    // bytes, queue scheduling, hot loop toggles, seek, or sink consumption.
    auto audioSource = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 3000);
    auto view = audioSource.mutable_view();
    for (std::size_t ch = 0; ch < 2; ++ch) {
        const auto values = view.channel(ch);
        QVERIFY(values);
        for (std::size_t n = 0; n < values.value()->size(); ++n) {
            (*values.value())[n] = (ch == 0 ? 1.0 : -1.0)
                * static_cast<double>(n % 31U) / 64.0;
        }
    }
    const auto exercise = [&](bool observe) {
        PlaybackEngine engine;
        auto output = std::make_unique<FakeOutput>(64U, 4U, 7U);
        auto* sink = output.get();
        auto ok = engine.install_pcm_candidate(audioSource.view(),
            std::move(output), DeviceSampleFormat::PCM_S16,
            std::nullopt, nullptr, core::RealizationId{1001});
        if (!ok) return std::vector<std::byte>{};
        ok = engine.play();
        if (!ok) return std::vector<std::byte>{};
        const auto loop = core::FrameRange::create(
            core::FrameIndex{100}, core::FrameIndex{140});
        for (int i = 0; i < 52; ++i) {
            if (i == 5 && loop) {
                ok = engine.set_loop(*loop.value());
                if (!ok) return std::vector<std::byte>{};
            }
            if (i == 17) {
                ok = engine.set_loop(std::nullopt);
                if (!ok) return std::vector<std::byte>{};
            }
            if (i == 31) {
                ok = engine.seek(core::FrameIndex{300});
                if (!ok) return std::vector<std::byte>{};
            }
            sink->consume_all();
            engine.tick();
            if (observe) {
                const auto prefix = engine.c2_consumed_prefix();
                if (prefix.certifiedForLiveTruePeak) {
                    return std::vector<std::byte>{}; // fail the outer parity oracle
                }
            }
        }
        return sink->history();
    };
    const auto noObservation = exercise(false);
    const auto withObservation = exercise(true);
    QVERIFY(!noObservation.empty());
    QCOMPARE(withObservation, noObservation);
}

void PlaybackSupportTest::c2D4PauseDoesNotAdvanceConsumedPrefix()
{
    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 1000);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(256U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{1002}));
    QVERIFY(engine.play());
    sink->set_processed_frames(20);
    const auto before = engine.c2_consumed_prefix();
    QVERIFY(before.partitionRepresented);
    QCOMPARE(before.processedWatermarkExclusive, std::int64_t{20});
    QVERIFY(engine.pause());
    engine.tick();
    const auto frozen = engine.c2_consumed_prefix(before.generation);
    QCOMPARE(frozen.processedWatermarkExclusive, before.processedWatermarkExclusive);
    QCOMPARE(frozen.size, before.size);
    QCOMPARE(frozen.slices[0].sourceEnd, before.slices[0].sourceEnd);
    QVERIFY(!frozen.certifiedForLiveTruePeak);
    QVERIFY(engine.play());
    const auto resumed = engine.c2_consumed_prefix(before.generation);
    QCOMPARE(resumed.processedWatermarkExclusive, std::int64_t{20});
    QVERIFY(!resumed.certifiedForLiveTruePeak);
}

void PlaybackSupportTest::c2D5SinkWatermarkFeedsRealNumericalRecentOnlyAfterFullBucket()
{
    using Kind = platform::windows::internal::C2ConsumedSliceKind;
    using analysis::internal::ChronologicalTruePeakBuckets;
    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 5500);
    auto writable = pcm.mutable_view();
    const auto l = writable.channel(0);
    const auto r = writable.channel(1);
    QVERIFY(l && r);
    std::fill(l.value()->begin(), l.value()->end(), 0.0);
    std::fill(r.value()->begin(), r.value()->end(), 0.0);
    // A 1-frame spike between timer polls is never skipped when consumption
    // is replayed from the owner-stamped, sink-capped prefix.
    (*l.value())[4 * 480 + 29] = 0.9;
    (*r.value())[4 * 480 + 29] = -0.45;
    PlaybackEngine engine;
    auto out = std::make_unique<FakeOutput>(384U, 4U);
    auto* sink = out.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(out),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{1101}));
    auto numeric = ChronologicalTruePeakBuckets::create(48'000, 2);
    QVERIFY(numeric);
    auto accumulator = std::move(*numeric.value());
    QVERIFY(engine.play());
    std::int64_t fed = 0;
    int iteration = 0;
    // Device processes just 96 frames per iteration; coarse UI polls could
    // miss the spike if we relied on cursor differences or rendered values.
    for (; iteration < 80 && fed < 4800; ++iteration) {
        sink->consume_all();
        engine.tick();
        const auto prefix = engine.c2_consumed_prefix();
        QVERIFY(prefix.partitionRepresented);
        QVERIFY(!prefix.certifiedForLiveTruePeak);
        QVERIFY(prefix.processedWatermarkExclusive >= fed);
        const auto parts = prefix.view();
        QCOMPARE(parts.size(), std::size_t{1});
        QCOMPARE(parts.front().kind, Kind::SOURCE_CANDIDATE_NOT_QUALIFIED);
        QCOMPARE(parts.front().realizationId,
            std::optional{core::RealizationId{1101}});
        QVERIFY(parts.front().sourceBegin && parts.front().sourceEnd);
        // This TEST knows the immutable buffer owner and source identity.
        // Production must NOT use a mere candidate without separate proof.
        const auto end = *parts.front().sourceEnd;
        for (; fed < end; ++fed) {
            const std::array<double, 2> sample{
                (*l.value())[static_cast<std::size_t>(fed)],
                (*r.value())[static_cast<std::size_t>(fed)]};
            QVERIFY(accumulator.consume_frame(sample, true));
        }
        const auto evidence = accumulator.snapshot();
        if (fed < 4800) {
            QVERIFY(!evidence.recent100msComplete);
        }
    }
    QCOMPARE(fed, std::int64_t{4800});
    const auto observed = accumulator.snapshot();
    QVERIFY(observed.recent100msComplete);
    QVERIFY(!observed.shortHold1sComplete);
    QCOMPARE(observed.consumedSourceFrames, std::int64_t{4800});
    QCOMPARE(observed.lastCompleteBucketEnd, std::int64_t{4800});
    QVERIFY(observed.recentLinearPeak[0] > 0.85);
    QVERIFY(observed.recentLinearPeak[1] > 0.42);
    // The observer intentionally does not publish LIVE: the required
    // provenance/target/chain revision is *not* bound by this test adapter.
    QCOMPARE(engine.c2_consumed_prefix().certifiedForLiveTruePeak, false);
}

void PlaybackSupportTest::c2D6WorkerProjectionPreservesConsumedWatermarkAndGap()
{
    using platform::windows::internal::project_c2_prefix;
    using platform::windows::WindowsConsumedSliceKind;
    using platform::windows::WindowsConsumedGapReason;

    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 2600);
    auto replacement = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 2600);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(128U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{1201}));
    QVERIFY(engine.play());
    auto initial = project_c2_prefix(engine.c2_consumed_prefix());
    QVERIFY(!initial.certifiedForLiveTruePeak);
    QCOMPARE(initial.count, std::size_t{0});
    QVERIFY(!initial.partitionRepresented);
    const auto gen = initial.generation;
    QVERIFY(engine.handoff_pcm(replacement.view(), nullptr,
        core::RealizationId{1202}));
    sink->consume_all();
    engine.tick();
    sink->set_processed_frames(64);
    const auto actual = project_c2_prefix(engine.c2_consumed_prefix(gen));
    QVERIFY(actual.partitionRepresented);
    QCOMPARE(actual.processedWatermarkExclusive, std::int64_t{64});
    QCOMPARE(actual.count, std::size_t{2});
    QCOMPARE(actual.slices[0].kind,
        WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED);
    QCOMPARE(actual.slices[0].realizationId,
        std::optional{core::RealizationId{1201}});
    QCOMPARE(actual.slices[1].kind,
        WindowsConsumedSliceKind::GAP_UNPROVEN);
    QCOMPARE(actual.slices[1].reason,
        WindowsConsumedGapReason::UNPROVEN_HANDOFF);
    QVERIFY(!actual.slices[1].realizationId);
    QVERIFY(!actual.slices[1].sourceBegin);
    QVERIFY(!actual.slices[1].sourceEnd);
    QVERIFY(!actual.certifiedForLiveTruePeak);
}

void PlaybackSupportTest::c2D6WorkerProjectionNeverCertifiesProcessedTarget()
{
    using platform::windows::internal::project_c2_prefix;
    using platform::windows::WindowsConsumedGapReason;
    using platform::windows::WindowsConsumedSliceKind;
    auto pcm = make_buffer(48'000, audio::ChannelLayout::MONO_C, 1000);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(128U, 2U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{1301}));
    QVERIFY(engine.play());
    sink->set_processed_frames(5);
    auto valid = project_c2_prefix(engine.c2_consumed_prefix());
    QVERIFY(valid.partitionRepresented);
    QCOMPARE(valid.count, std::size_t{1});
    QCOMPARE(valid.slices[0].kind,
        WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED);
    QCOMPARE(valid.slices[0].sourceEnd, std::optional<std::int64_t>{5});
    QVERIFY(!valid.certifiedForLiveTruePeak);
    sink->set_processed_frames(500);
    const auto invalidClock = project_c2_prefix(engine.c2_consumed_prefix());
    QCOMPARE(invalidClock.reason,
        WindowsConsumedGapReason::CLOCK_AHEAD_OF_ACCEPTED);
    QVERIFY(!invalidClock.partitionRepresented);
    QVERIFY(!invalidClock.certifiedForLiveTruePeak);
    QVERIFY(engine.seek(core::FrameIndex{50}));
    const auto stale = project_c2_prefix(
        engine.c2_consumed_prefix(valid.generation));
    QCOMPARE(stale.reason, WindowsConsumedGapReason::STALE_GENERATION);
    QVERIFY(!stale.partitionRepresented);
    QVERIFY(!stale.certifiedForLiveTruePeak);
}


void PlaybackSupportTest::c2D9RealSinkSeekMaintainsRelativeOutputAndAbsoluteSource()
{
    using Kind = platform::windows::internal::C2ConsumedSliceKind;
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 30'000);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(384U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{1601}));
    QVERIFY(engine.play());
    sink->set_processed_frames(40);
    const auto first = engine.c2_consumed_prefix();
    QVERIFY(first.partitionRepresented);
    QCOMPARE(first.size, std::size_t{1});
    QCOMPARE(first.slices[0].sourceBegin, std::optional<std::int64_t>{0});
    QCOMPARE(first.slices[0].sourceEnd, std::optional<std::int64_t>{40});
    const auto initialGeneration = first.generation;

    // A real seek restarts the backend's processed clock at zero, but the
    // accepted PCM now starts at its ABSOLUTE SOURCE frame 4801.
    QVERIFY(engine.seek(core::FrameIndex{4801}));
    auto zero = engine.c2_consumed_prefix();
    QVERIFY(zero.generation > initialGeneration);
    QCOMPARE(zero.processedWatermarkExclusive, std::int64_t{0});
    QVERIFY(!zero.partitionRepresented);
    QVERIFY(!zero.certifiedForLiveTruePeak);
    sink->set_processed_frames(31);
    const auto afterSeek = engine.c2_consumed_prefix();
    QCOMPARE(afterSeek.reason, Reason::MISSING_CHRONOLOGICAL_LEDGER);
    QVERIFY(afterSeek.partitionRepresented);
    QCOMPARE(afterSeek.processedWatermarkExclusive, std::int64_t{31});
    QCOMPARE(afterSeek.size, std::size_t{1});
    QCOMPARE(afterSeek.slices[0].kind, Kind::SOURCE_CANDIDATE_NOT_QUALIFIED);
    QCOMPARE(afterSeek.slices[0].outputBegin, std::int64_t{0});
    QCOMPARE(afterSeek.slices[0].outputEnd, std::int64_t{31});
    QCOMPARE(afterSeek.slices[0].sourceBegin, std::optional<std::int64_t>{4801});
    QCOMPARE(afterSeek.slices[0].sourceEnd, std::optional<std::int64_t>{4832});
    QCOMPARE(afterSeek.slices[0].realizationId,
        std::optional{core::RealizationId{1601}});
    QVERIFY(!afterSeek.certifiedForLiveTruePeak);
    QVERIFY(engine.seek(core::FrameIndex{9603}));
    const auto stale = engine.c2_consumed_prefix(afterSeek.generation);
    QCOMPARE(stale.reason, Reason::STALE_GENERATION);
    sink->set_processed_frames(23);
    const auto second = engine.c2_consumed_prefix();
    QVERIFY(second.generation > afterSeek.generation);
    QCOMPARE(second.processedWatermarkExclusive, std::int64_t{23});
    QVERIFY(second.partitionRepresented);
    QCOMPARE(second.size, std::size_t{1});
    QCOMPARE(second.slices[0].sourceBegin, std::optional<std::int64_t>{9603});
    QCOMPARE(second.slices[0].sourceEnd, std::optional<std::int64_t>{9626});
    QVERIFY(!second.certifiedForLiveTruePeak);
}


void PlaybackSupportTest::c2D10LoopOffRecoversFreshSourceWithoutRestart()
{
    using Kind = platform::windows::internal::C2ConsumedSliceKind;
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    auto pcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 12'000);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(256U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{1801}));
    QVERIFY(engine.play());
    const auto starts = sink->startCalls;
    const auto stops = sink->stopCalls;
    const auto loop = core::FrameRange::create(
        core::FrameIndex{100}, core::FrameIndex{400});
    QVERIFY(loop);
    QVERIFY(engine.set_loop(*loop.value()));
    bool sawLoopGap = false;
    for (int i = 0; i < 17; ++i) {
        sink->consume_all();
        engine.tick();
        const auto prefix = engine.c2_consumed_prefix();
        for (const auto& slice : prefix.view()) {
            if (slice.kind == Kind::GAP_UNPROVEN) sawLoopGap = true;
        }
    }
    QVERIFY(sawLoopGap);
    QCOMPARE(sink->startCalls, starts);
    QCOMPARE(sink->stopCalls, stops);
    QVERIFY(engine.set_loop(std::nullopt));
    QCOMPARE(sink->startCalls, starts);
    QCOMPARE(sink->stopCalls, stops);
    bool sawFreshDirectTail = false;
    std::int64_t previousWatermark = 0;
    for (int i = 0; i < 160 && !sawFreshDirectTail; ++i) {
        sink->consume_all();
        engine.tick();
        const auto p = engine.c2_consumed_prefix();
        QVERIFY(!p.certifiedForLiveTruePeak);
        QVERIFY(p.processedWatermarkExclusive >= previousWatermark);
        previousWatermark = p.processedWatermarkExclusive;
        if (!p.partitionRepresented || p.size < 2U) continue;
        const auto& last = p.slices[p.size - 1U];
        if (last.kind != Kind::SOURCE_CANDIDATE_NOT_QUALIFIED
            || last.outputEnd != p.processedWatermarkExclusive) continue;
        bool precedingGap = false;
        for (std::size_t k = 0; k + 1U < p.size; ++k)
            precedingGap |= p.slices[k].kind == Kind::GAP_UNPROVEN;
        if (!precedingGap) continue;
        QVERIFY(p.reason == Reason::UNPROVEN_LOOP);
        QVERIFY(last.sourceBegin);
        QVERIFY(last.sourceEnd);
        QCOMPARE(*last.sourceEnd - *last.sourceBegin,
            last.outputEnd - last.outputBegin);
        QCOMPARE(last.realizationId,
            std::optional{core::RealizationId{1801}});
        sawFreshDirectTail = true;
    }
    QVERIFY2(sawFreshDirectTail,
        "Loop OFF must eventually yield new owner-stamped contiguous PROCESSED coverage.");
    QCOMPARE(sink->startCalls, starts);
    QCOMPARE(sink->stopCalls, stops);
}

void PlaybackSupportTest::c2D12HandoffRecoversConsumedPureNewSuffixWithoutRestart()
{
    using Kind = platform::windows::internal::C2ConsumedSliceKind;
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    auto oldPcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 48'000);
    auto newPcm = make_buffer(48'000, audio::ChannelLayout::STEREO_LR, 48'000);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(4096U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(oldPcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{1901}));
    QVERIFY(engine.play());
    const int starts = sink->startCalls;
    const int stops = sink->stopCalls;
    QVERIFY(engine.handoff_pcm(newPcm.view(), nullptr,
        core::RealizationId{1902}));
    bool sawGap = false;
    bool sawNew = false;
    for (int i = 0; i < 45 && !sawNew; ++i) {
        sink->consume_all();
        engine.tick();
        const auto p = engine.c2_consumed_prefix();
        QVERIFY(!p.certifiedForLiveTruePeak);
        if (!p.partitionRepresented) continue;
        for (const auto& slice : p.view())
            sawGap |= slice.kind == Kind::GAP_UNPROVEN;
        if (p.size < 2U) continue;
        const auto& last = p.slices[p.size - 1U];
        if (last.kind != Kind::SOURCE_CANDIDATE_NOT_QUALIFIED
            || last.realizationId != std::optional{core::RealizationId{1902}}
            || last.outputEnd != p.processedWatermarkExclusive) continue;
        QVERIFY(sawGap);
        QCOMPARE(p.reason, Reason::UNPROVEN_HANDOFF);
        QVERIFY(last.sourceBegin && last.sourceEnd);
        QCOMPARE(*last.sourceEnd - *last.sourceBegin,
                 last.outputEnd - last.outputBegin);
        sawNew = true;
    }
    QVERIFY2(sawNew, "Pure NEW post-crossfade source must recover without Stop.");
    QCOMPARE(sink->startCalls, starts);
    QCOMPARE(sink->stopCalls, stops);
}

void PlaybackSupportTest::c2D10Native44100IsNotInherentlyUnavailable()
{
    using Kind = platform::windows::internal::C2ConsumedSliceKind;
    using Reason = platform::windows::internal::C2CoverageProbeReason;
    auto pcm = make_buffer(44'100, audio::ChannelLayout::STEREO_LR, 3000);
    PlaybackEngine engine;
    auto output = std::make_unique<FakeOutput>(256U, 4U);
    auto* sink = output.get();
    QVERIFY(engine.install_pcm_candidate(pcm.view(), std::move(output),
        DeviceSampleFormat::PCM_S16, std::nullopt, nullptr,
        core::RealizationId{1802}));
    QVERIFY(engine.play());
    sink->set_processed_frames(20);
    const auto p = engine.c2_consumed_prefix();
    QCOMPARE(p.reason, Reason::MISSING_CHRONOLOGICAL_LEDGER);
    QVERIFY(p.partitionRepresented);
    QCOMPARE(p.size, std::size_t{1});
    QCOMPARE(p.slices[0].kind, Kind::SOURCE_CANDIDATE_NOT_QUALIFIED);
    QCOMPARE(p.slices[0].sourceBegin, std::optional<std::int64_t>{0});
    QCOMPARE(p.slices[0].sourceEnd, std::optional<std::int64_t>{20});
    QVERIFY(!p.certifiedForLiveTruePeak);
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::PlaybackSupportTest)

#include "test_playback_support.moc"
