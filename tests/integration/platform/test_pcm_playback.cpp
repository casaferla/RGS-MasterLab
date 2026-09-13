#include "playback_support.hpp"
#include "../../unit/audio/wav_test_support.hpp"

#include <rgsml/audio/audio_buffer.hpp>

#include <QTest>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::tests {
namespace {

using platform::windows::internal::DeviceSampleFormat;
using platform::windows::internal::IPlaybackOutput;
using platform::windows::internal::OutputState;
using platform::windows::internal::PlaybackEngine;

class RecordingOutput final : public IPlaybackOutput {
public:
    [[nodiscard]] std::size_t writable_bytes() const noexcept override
    {
        return 4096U - queue_.size();
    }
    [[nodiscard]] std::size_t queued_bytes() const noexcept override
    {
        return queue_.size();
    }
    [[nodiscard]] core::Result<std::size_t> enqueue(
        std::span<const std::byte> bytes) override
    {
        queue_.insert(queue_.end(), bytes.begin(), bytes.end());
        history_.insert(history_.end(), bytes.begin(), bytes.end());
        return core::Result<std::size_t>::success(bytes.size());
    }
    void clear_queue() noexcept override { queue_.clear(); }
    [[nodiscard]] core::Status start() override
    {
        state_ = OutputState::ACTIVE;
        return core::Status::success();
    }
    [[nodiscard]] core::Status suspend() override
    {
        state_ = OutputState::SUSPENDED;
        return core::Status::success();
    }
    [[nodiscard]] core::Status resume() override
    {
        state_ = OutputState::ACTIVE;
        return core::Status::success();
    }
    [[nodiscard]] core::Status stop() override
    {
        state_ = OutputState::STOPPED;
        queue_.clear();
        return core::Status::success();
    }
    [[nodiscard]] std::int64_t processed_frames() const noexcept override
    {
        return processed_;
    }
    [[nodiscard]] OutputState state() const noexcept override { return state_; }
    [[nodiscard]] std::optional<core::Error> error() const override
    {
        return std::nullopt;
    }

    const std::vector<std::byte>& history() const noexcept { return history_; }
    std::int64_t processed_{0};

private:
    OutputState state_{OutputState::STOPPED};
    std::vector<std::byte> queue_;
    std::vector<std::byte> history_;
};

[[nodiscard]] audio::AudioBuffer partial_buffer()
{
    auto rate = core::SampleRate::create(48'000);
    auto format = audio::AudioFormat::create(
        *rate.value(), audio::ChannelLayout::MONO_C);
    auto count = core::FrameCount::create(8);
    auto buffer = audio::AudioBuffer::create(
        *format.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{100}, *count.value());
    auto channel = buffer.value()->mutable_view().channel(0);
    for (std::size_t index = 0; index < channel.value()->size(); ++index) {
        (*channel.value())[index] = static_cast<double>(index + 1U) / 16.0;
    }
    return std::move(*buffer.value());
}

[[nodiscard]] float first_float(const std::vector<std::byte>& bytes)
{
    const auto bits = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[0]))
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[3])) << 24U);
    return std::bit_cast<float>(bits);
}

}  // namespace

class PcmPlaybackTest final : public QObject {
    Q_OBJECT

private slots:
    void borrowedPartialTimelineAndLoopUseTheExistingEngine();
    void clearBeforeOwnerDestructionLeavesNoBorrowedInput();
    void repeatedWavAndPcmReplacementUsesOneEngine();
    void wavAndPcmBackingsShareExactAndPairedRatePaths();
};

void PcmPlaybackTest::borrowedPartialTimelineAndLoopUseTheExistingEngine()
{
    auto buffer = partial_buffer();
    PlaybackEngine engine;
    auto output = std::make_unique<RecordingOutput>();
    auto* observed = output.get();
    QVERIFY(engine.install_pcm_candidate(
        buffer.view(), std::move(output), DeviceSampleFormat::IEEE_F32));
    auto initial = engine.snapshot();
    QVERIFY(initial);
    QCOMPARE(initial.value()->position.value(), std::int64_t{100});
    QCOMPARE(initial.value()->duration->value(), std::int64_t{108});
    QVERIFY(!engine.seek(core::FrameIndex{99}));
    QVERIFY(engine.seek(core::FrameIndex{102}));
    const auto loop = *core::FrameRange::create(
        core::FrameIndex{102}, core::FrameIndex{106}).value();
    QVERIFY(engine.set_loop(loop));
    QVERIFY(engine.play());
    QVERIFY(observed->history().size() >= sizeof(float));
    QCOMPARE(first_float(observed->history()), 3.0F / 16.0F);
    observed->processed_ = 2;
    engine.tick();
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{104});
    QVERIFY(engine.pause());
    QVERIFY(engine.play());
    QVERIFY(engine.stop());
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{100});
}

void PcmPlaybackTest::clearBeforeOwnerDestructionLeavesNoBorrowedInput()
{
    PlaybackEngine engine;
    {
        auto buffer = partial_buffer();
        QVERIFY(engine.install_pcm_candidate(
            buffer.view(), std::make_unique<RecordingOutput>(),
            DeviceSampleFormat::IEEE_F32));
        QVERIFY(engine.clear());
    }
    auto snapshot = engine.snapshot();
    QVERIFY(snapshot);
    QCOMPARE(snapshot.value()->state, core::PlaybackState::NO_SOURCE);
    QVERIFY(!engine.play());
}

void PcmPlaybackTest::repeatedWavAndPcmReplacementUsesOneEngine()
{
    using namespace wav_support;
    const auto wav = make_wav(
        1U, 16U, 1U, 48'000U,
        pcm_payload(std::vector<std::int64_t>{0, 1024, -1024, 2048}, 16U));
    auto first = audio::WavReader::open(
        memory_reader(wav, std::make_shared<ReaderControl>()));
    QVERIFY(first);
    PlaybackEngine engine;
    QVERIFY(engine.install_candidate(
        std::move(*first.value()), std::make_unique<RecordingOutput>(),
        DeviceSampleFormat::IEEE_F32, std::nullopt));
    QCOMPARE(engine.snapshot().value()->duration->value(), std::int64_t{4});

    auto pcm = partial_buffer();
    QVERIFY(engine.install_pcm_candidate(
        pcm.view(), std::make_unique<RecordingOutput>(),
        DeviceSampleFormat::IEEE_F32));
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{100});
    QCOMPARE(engine.snapshot().value()->duration->value(), std::int64_t{108});

    auto second = audio::WavReader::open(
        memory_reader(wav, std::make_shared<ReaderControl>()));
    QVERIFY(second);
    QVERIFY(engine.install_candidate(
        std::move(*second.value()), std::make_unique<RecordingOutput>(),
        DeviceSampleFormat::IEEE_F32, std::nullopt));
    QCOMPARE(engine.snapshot().value()->position.value(), std::int64_t{0});
    QCOMPARE(engine.snapshot().value()->duration->value(), std::int64_t{4});
}

void PcmPlaybackTest::wavAndPcmBackingsShareExactAndPairedRatePaths()
{
    using namespace wav_support;
    constexpr std::int64_t kFrames = 257;
    std::vector<std::int64_t> sampleCodes;
    sampleCodes.reserve(static_cast<std::size_t>(kFrames * 2));
    for (std::int64_t frame = 0; frame < kFrames; ++frame) {
        sampleCodes.push_back(((frame * 257) % 30'000) - 15'000);
        sampleCodes.push_back(15'000 - ((frame * 193) % 30'000));
    }

    for (const auto inputRate : {44'100, 48'000}) {
        const auto pairedRate = inputRate == 44'100 ? 48'000 : 44'100;
        const auto wav = make_wav(
            1U, 16U, 2U, static_cast<std::uint32_t>(inputRate),
            pcm_payload(sampleCodes, 16U));

        auto canonicalReader = audio::WavReader::open(
            memory_reader(wav, std::make_shared<ReaderControl>()));
        QVERIFY(canonicalReader);
        auto source = audio::AudioBuffer::create(
            (*canonicalReader.value())->info().audio_format(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},
            (*canonicalReader.value())->info().frame_count());
        QVERIFY(source);
        auto decoded = (*canonicalReader.value())->read_frames(
            core::FrameIndex{0}, source.value()->mutable_view());
        QVERIFY(decoded);
        QCOMPARE(decoded.value()->value(), kFrames);

        for (const auto outputRate : {inputRate, pairedRate}) {
            const bool paired = outputRate != inputRate;
            auto selected = platform::windows::internal::select_device_format(
                source.value()->view().format(),
                !paired,
                false,
                paired,
                false);
            QVERIFY(selected);
            QCOMPARE(selected.value()->sampleRateHz, outputRate);
            QCOMPARE(selected.value()->srcApplied, paired);

            auto makeAdapter = [&]()
                -> core::Result<audio::PlaybackSampleRateAdapter> {
                auto rate = core::SampleRate::create(outputRate);
                if (!rate) {
                    return core::Result<audio::PlaybackSampleRateAdapter>::failure(
                        *rate.error());
                }
                return audio::PlaybackSampleRateAdapter::create(
                    audio::PlaybackRateSpec{
                        source.value()->view().format().sample_rate(),
                        *rate.value(),
                        source.value()->view().format().channel_layout(),
                        source.value()->view().frame_count(),
                    });
            };

            std::optional<audio::PlaybackSampleRateAdapter> wavAdapter;
            std::int64_t expectedFrames = kFrames;
            if (paired) {
                auto adapter = makeAdapter();
                QVERIFY(adapter);
                expectedFrames = adapter.value()->output_frame_count().value();
                wavAdapter.emplace(std::move(*adapter.value()));
            }
            auto wavReader = audio::WavReader::open(
                memory_reader(wav, std::make_shared<ReaderControl>()));
            QVERIFY(wavReader);
            PlaybackEngine wavEngine;
            auto wavOutput = std::make_unique<RecordingOutput>();
            auto* wavObserved = wavOutput.get();
            QVERIFY(wavEngine.install_candidate(
                std::move(*wavReader.value()),
                std::move(wavOutput),
                selected.value()->sampleFormat,
                std::move(wavAdapter)));
            QVERIFY(wavEngine.play());
            const auto wavHistory = wavObserved->history();

            std::optional<audio::PlaybackSampleRateAdapter> pcmAdapter;
            if (paired) {
                auto adapter = makeAdapter();
                QVERIFY(adapter);
                pcmAdapter.emplace(std::move(*adapter.value()));
            }
            PlaybackEngine pcmEngine;
            auto pcmOutput = std::make_unique<RecordingOutput>();
            auto* pcmObserved = pcmOutput.get();
            QVERIFY(pcmEngine.install_pcm_candidate(
                source.value()->view(),
                std::move(pcmOutput),
                selected.value()->sampleFormat,
                std::move(pcmAdapter)));
            QVERIFY(pcmEngine.play());
            const auto pcmHistory = pcmObserved->history();

            const auto expectedBytes = static_cast<std::size_t>(
                expectedFrames * 2 * static_cast<std::int64_t>(sizeof(float)));
            QCOMPARE(wavHistory.size(), expectedBytes);
            QCOMPARE(pcmHistory.size(), expectedBytes);
            QVERIFY(wavHistory == pcmHistory);
        }
    }
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::PcmPlaybackTest)

#include "test_pcm_playback.moc"
