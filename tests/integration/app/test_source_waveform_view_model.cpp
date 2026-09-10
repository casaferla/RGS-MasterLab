#include "playback_transport_view_model.hpp"
#include "source_waveform_view_model.hpp"
#include "waveform_presentation.hpp"

#include "../../unit/audio/wav_test_support.hpp"
#include "../../unit/platform/fake_playback_service.hpp"

#include <QSignalSpy>
#include <QTest>

#include <array>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <semaphore>
#include <stop_token>
#include <string>
#include <thread>

namespace rgsml::tests {
namespace {

[[nodiscard]] core::ResourceReference reference(std::string locator)
{
    auto value = core::ResourceReference::create(
        "test.memory", std::move(locator), true, false, "Waveform fixture");
    Q_ASSERT(value);
    return std::move(*value.value());
}

[[nodiscard]] core::Result<audio::WaveformSummary> summary(double sample)
{
    using namespace wav_support;
    const auto bits = std::bit_cast<std::uint64_t>(sample);
    const auto bytes = make_wav(
        3U,
        64U,
        1U,
        48'000U,
        f64_payload(std::array<std::uint64_t, 1>{bits}));
    auto reader = audio::WavReader::open(
        memory_reader(bytes, std::make_shared<ReaderControl>()));
    if (!reader) {
        return core::Result<audio::WaveformSummary>::failure(*reader.error());
    }
    return audio::build_waveform_summary(**reader.value());
}

[[nodiscard]] core::Result<audio::WaveformSummary> failure()
{
    return core::Result<audio::WaveformSummary>::failure(core::Error{
        core::ErrorCode::IoFailure,
        "Injected deterministic waveform failure.",
    });
}

[[nodiscard]] bool await_state(
    ui::WaveformPresentation& presentation,
    const QString& expected)
{
    if (presentation.state_token() == expected) {
        return true;
    }
    QSignalSpy changed{&presentation, &ui::WaveformPresentation::changed};
    for (int event = 0; event < 8 && presentation.state_token() != expected; ++event) {
        if (!changed.wait(2'000)) {
            break;
        }
    }
    return presentation.state_token() == expected;
}

}  // namespace

class SourceWaveformViewModelTest final : public QObject {
    Q_OBJECT

private slots:
    void replacementCancelsAndStaleCannotPublish();
    void failureRetryAndPlaybackRemainIndependent();
    void shutdownCancelsAndPublishesNoCallback();
};

void SourceWaveformViewModelTest::replacementCancelsAndStaleCannotPublish()
{
    ui::WaveformPresentation presentation;
    std::binary_semaphore firstEntered{0};
    std::binary_semaphore releaseFirst{0};
    const auto guiThread = std::this_thread::get_id();
    std::atomic<bool> ranAwayFromGui{false};

    app::SourceWaveformViewModel model{
        &presentation,
        [&](const core::ResourceReference& source, std::stop_token) {
            ranAwayFromGui.store(
                std::this_thread::get_id() != guiThread,
                std::memory_order_release);
            if (source.locator() == "A") {
                firstEntered.release();
                releaseFirst.acquire();
                // Deliberately ignore cancellation to prove the generation guard.
                return summary(-0.75);
            }
            return summary(0.5);
        }};

    model.source_committed(reference("A"));
    QCOMPARE(presentation.state_token(), QStringLiteral("BUILDING"));
    QVERIFY(!presentation.summary());
    firstEntered.acquire();
    model.source_committed(reference("B"));
    QCOMPARE(presentation.state_token(), QStringLiteral("BUILDING"));
    QVERIFY(!presentation.summary());
    releaseFirst.release();

    QVERIFY(await_state(presentation, QStringLiteral("READY")));
    QVERIFY(ranAwayFromGui.load(std::memory_order_acquire));
    QCOMPARE(model.generation(), std::uint64_t{2});
    QCOMPARE(model.source_replacement_cancellations(), std::uint64_t{1});
    QCOMPARE(model.stale_results_discarded(), std::uint64_t{1});
    const auto ready = presentation.summary();
    QVERIFY(ready);
    const auto base = ready->level(0U);
    QVERIFY(base);
    const auto peak = base.value()->peak(0U, core::FrameIndex{0});
    QVERIFY(peak);
    QCOMPARE(peak.value()->minimum, 0.5);
}

void SourceWaveformViewModelTest::failureRetryAndPlaybackRemainIndependent()
{
    ui::WaveformPresentation presentation;
    std::atomic<int> attempts{0};
    std::binary_semaphore buildEntered{0};
    std::binary_semaphore releaseBuild{0};

    auto playbackService = std::make_unique<FakePlaybackService>();
    auto* observedPlayback = playbackService.get();
    app::PlaybackTransportViewModel playback{std::move(playbackService)};
    app::SourceWaveformViewModel model{
        &presentation,
        [&](const core::ResourceReference&, std::stop_token) {
            const auto attempt = ++attempts;
            if (attempt == 1) {
                return failure();
            }
            buildEntered.release();
            releaseBuild.acquire();
            return summary(0.25);
        }};

    model.source_committed(reference("retry"));
    QVERIFY(await_state(presentation, QStringLiteral("FAILED")));
    QVERIFY(!presentation.summary());
    presentation.requestRetry();
    buildEntered.acquire();
    QCOMPARE(presentation.state_token(), QStringLiteral("BUILDING"));

    playback.prepare_source(reference("playback"), 48'000);
    playback.playOrResume();
    playback.pause();
    playback.stop();
    QCOMPARE(observedPlayback->prepareCalls, 1);
    QCOMPARE(observedPlayback->playCalls, 1);
    QCOMPARE(observedPlayback->pauseCalls, 1);
    QCOMPARE(observedPlayback->stopCalls, 1);

    releaseBuild.release();
    QVERIFY(await_state(presentation, QStringLiteral("READY")));
    QCOMPARE(attempts.load(), 2);
}

void SourceWaveformViewModelTest::shutdownCancelsAndPublishesNoCallback()
{
    ui::WaveformPresentation presentation;
    std::binary_semaphore entered{0};
    std::mutex mutex;
    std::condition_variable_any cancelled;
    int changeCount = 0;
    QObject::connect(
        &presentation,
        &ui::WaveformPresentation::changed,
        &presentation,
        [&changeCount] { ++changeCount; });

    {
        app::SourceWaveformViewModel model{
            &presentation,
            [&](const core::ResourceReference&, std::stop_token stopToken) {
                entered.release();
                std::unique_lock lock{mutex};
                static_cast<void>(cancelled.wait(
                    lock, stopToken, [] { return false; }));
                return failure();
            }};
        model.source_committed(reference("shutdown"));
        entered.acquire();
    }
    const int countAfterDestruction = changeCount;
    QCoreApplication::processEvents();
    QCOMPARE(changeCount, countAfterDestruction);
    QCOMPARE(presentation.state_token(), QStringLiteral("BUILDING"));
}

}  // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::SourceWaveformViewModelTest)

#include "test_source_waveform_view_model.moc"
