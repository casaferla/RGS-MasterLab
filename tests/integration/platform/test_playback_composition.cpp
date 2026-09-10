#include "playback_transport_view_model.hpp"
#include "source_selection_view_model.hpp"

#include "../../audio_golden/wav/golden_vectors.hpp"
#include "../../unit/platform/fake_playback_service.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

namespace rgsml::tests {
namespace {

[[nodiscard]] QString write_wav(
    QTemporaryDir& directory,
    const QString& name,
    const std::array<std::uint8_t, 50>& bytes)
{
    const auto path = directory.filePath(name);
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)
        || file.write(
               reinterpret_cast<const char*>(bytes.data()),
               static_cast<qint64>(bytes.size()))
            != static_cast<qint64>(bytes.size())) {
        return {};
    }
    file.close();
    return path;
}

}  // namespace

class PlaybackCompositionTest final : public QObject {
    Q_OBJECT

private slots:
    void sourceTransactionAndTransportIntents();
    void prepareFailureCannotExposeStalePlayback();
};

void PlaybackCompositionTest::sourceTransactionAndTransportIntents()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    app::SourceSelectionViewModel source;
    source.set_playback_transport(&transport);

    QVERIFY(!transport.playback_available());
    QVERIFY(!transport.can_play());
    QCOMPARE(transport.state_label(), QStringLiteral("No Source"));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto firstPath = write_wav(
        directory,
        QStringLiteral("First.wav"),
        audio_golden::kRiffPcm16Mono);
    QVERIFY(!firstPath.isEmpty());
    source.selectSource(QUrl::fromLocalFile(firstPath));
    QCoreApplication::processEvents();
    QCOMPARE(observed->prepareCalls, 1);
    QCOMPARE(observed->clearCalls, 1);
    QVERIFY(transport.playback_available());
    QVERIFY(transport.can_play());
    QCOMPARE(transport.state_label(), QStringLiteral("Stopped"));
    QCOMPARE(transport.duration_frames(), qint64{48000});

    transport.playOrResume();
    QCOMPARE(observed->playCalls, 1);
    QVERIFY(transport.is_playing());
    transport.pause();
    QCOMPARE(observed->pauseCalls, 1);
    QVERIFY(transport.is_paused());
    transport.playOrResume();
    QCOMPARE(observed->playCalls, 2);
    transport.stop();
    QCOMPARE(observed->stopCalls, 1);
    QCOMPARE(transport.position_frames(), qint64{0});

    source.cancelSourceSelection();
    QCOMPARE(observed->prepareCalls, 1);
    source.selectSource(QUrl{QStringLiteral("https://example.invalid/not-local.wav")});
    QCOMPARE(observed->prepareCalls, 1);
    QVERIFY(transport.playback_available());
}

void PlaybackCompositionTest::prepareFailureCannotExposeStalePlayback()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    app::SourceSelectionViewModel source;
    source.set_playback_transport(&transport);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto firstPath = write_wav(
        directory,
        QStringLiteral("First.wav"),
        audio_golden::kRiffPcm16Mono);
    const auto secondPath = write_wav(
        directory,
        QStringLiteral("Second.wav"),
        audio_golden::kRiffPcm16Mono);
    source.selectSource(QUrl::fromLocalFile(firstPath));
    transport.playOrResume();
    QVERIFY(transport.is_playing());

    observed->failPrepare = true;
    source.selectSource(QUrl::fromLocalFile(secondPath));
    QCoreApplication::processEvents();
    QCOMPARE(source.display_name(), QStringLiteral("Second.wav"));
    QCOMPARE(observed->prepareCalls, 2);
    QVERIFY(observed->stopCalls >= 1);
    QCOMPARE(observed->clearCalls, 2);
    QVERIFY(!transport.playback_available());
    QVERIFY(!transport.can_play());
    QVERIFY(!transport.error_message().isEmpty());
    QCOMPARE(observed->state, core::PlaybackState::NO_SOURCE);

    observed->failPrepare = false;
    source.selectSource(QUrl::fromLocalFile(secondPath));
    QCoreApplication::processEvents();
    QCOMPARE(observed->prepareCalls, 3);
    QVERIFY(transport.playback_available());
    QVERIFY(transport.error_message().isEmpty());
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::PlaybackCompositionTest)

#include "test_playback_composition.moc"
