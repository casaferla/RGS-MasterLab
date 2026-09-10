#include "playback_transport_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "waveform_item.hpp"
#include "waveform_presentation.hpp"

#include "../audio_golden/wav/golden_vectors.hpp"
#include "../unit/audio/wav_test_support.hpp"
#include "../unit/platform/fake_playback_service.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>

#include <memory>

namespace rgsml::tests {
namespace {

[[nodiscard]] QString write_file(
    QTemporaryDir& directory,
    const QString& name,
    const QByteArray& bytes)
{
    const auto path = directory.filePath(name);
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)
        || file.write(bytes) != bytes.size()) {
        return {};
    }
    file.close();
    return path;
}

[[nodiscard]] QByteArray valid_wav()
{
    return QByteArray{
        reinterpret_cast<const char*>(audio_golden::kRiffPcm16Mono.data()),
        static_cast<qsizetype>(audio_golden::kRiffPcm16Mono.size()),
    };
}

[[nodiscard]] std::shared_ptr<const audio::WaveformSummary> valid_summary()
{
    using namespace wav_support;
    auto reader = audio::WavReader::open(memory_reader(
        from_u8_array(audio_golden::kRiffPcm16Mono),
        std::make_shared<ReaderControl>()));
    Q_ASSERT(reader);
    auto summary = audio::build_waveform_summary(**reader.value());
    Q_ASSERT(summary);
    return std::make_shared<audio::WaveformSummary>(
        std::move(*summary.value()));
}

}  // namespace

class SourceMetadataPanelSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyReadyErrorAndWindowLifecycle();
};

void SourceMetadataPanelSmokeTest::emptyReadyErrorAndWindowLifecycle()
{
    auto playbackService = std::make_unique<FakePlaybackService>();
    auto* observedPlayback = playbackService.get();
    app::PlaybackTransportViewModel playbackTransport{
        std::move(playbackService)};
    app::SourceSelectionViewModel model;
    model.set_playback_transport(&playbackTransport);
    ui::WaveformPresentation waveformPresentation;
    QQmlApplicationEngine engine;
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
    engine.rootContext()->setContextProperty(
        QStringLiteral("sourceSelection"), &model);
    engine.rootContext()->setContextProperty(
        QStringLiteral("playbackTransport"), &playbackTransport);
    engine.rootContext()->setContextProperty(
        QStringLiteral("sourceWaveform"), &waveformPresentation);
    engine.loadFromModule("Rgsml.Ui", "Main");
    QCOMPARE(engine.rootObjects().size(), 1);

    auto* root = engine.rootObjects().front();
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceOpenButton")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceFileDialog")));
    auto* empty = root->findChild<QObject*>(QStringLiteral("sourceEmptyState"));
    auto* display = root->findChild<QObject*>(QStringLiteral("sourceDisplayName"));
    auto* readOnly = root->findChild<QObject*>(QStringLiteral("sourceReadOnlyBadge"));
    auto* error = root->findChild<QObject*>(QStringLiteral("sourceErrorMessage"));
    QVERIFY(empty);
    QVERIFY(display);
    QVERIFY(readOnly);
    QVERIFY(error);
    auto* playPause = root->findChild<QObject*>(QStringLiteral("playPauseButton"));
    auto* stop = root->findChild<QObject*>(QStringLiteral("stopButton"));
    auto* playbackState = root->findChild<QObject*>(
        QStringLiteral("playbackStateLabel"));
    auto* playbackTime = root->findChild<QObject*>(
        QStringLiteral("playbackTimeLabel"));
    auto* playbackError = root->findChild<QObject*>(
        QStringLiteral("playbackErrorMessage"));
    QVERIFY(playPause);
    QVERIFY(stop);
    QVERIFY(playbackState);
    QVERIFY(playbackTime);
    QVERIFY(playbackError);
    QVERIFY(!playPause->property("enabled").toBool());
    QVERIFY(!stop->property("enabled").toBool());
    QCOMPARE(playbackState->property("text").toString(), QStringLiteral("No Source"));
    QVERIFY(empty->property("visible").toBool());
    QVERIFY(!display->property("visible").toBool());
    auto* waveformPanel = root->findChild<QObject*>(
        QStringLiteral("sourceWaveformPanel"));
    auto* waveformStatus = root->findChild<QObject*>(
        QStringLiteral("sourceWaveformStatus"));
    auto* waveformObject = root->findChild<QObject*>(
        QStringLiteral("sourceWaveformOverview"));
    auto* waveformItem = qobject_cast<ui::WaveformItem*>(waveformObject);
    auto* waveformRetry = root->findChild<QObject*>(
        QStringLiteral("sourceWaveformRetryButton"));
    QVERIFY(waveformPanel);
    QVERIFY(waveformStatus);
    QVERIFY(waveformItem);
    QVERIFY(waveformRetry);
    QCOMPARE(waveformPresentation.state_token(), QStringLiteral("EMPTY"));
    QVERIFY(waveformStatus->property("visible").toBool());
    QVERIFY(!waveformItem->isVisible());
    QCOMPARE(waveformItem->acceptedMouseButtons(), Qt::NoButton);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto validPath = write_file(
        directory, QStringLiteral("UI Source.wav"), valid_wav());
    model.selectSource(QUrl::fromLocalFile(validPath));
    QCoreApplication::processEvents();
    QVERIFY(!empty->property("visible").toBool());
    QVERIFY(display->property("visible").toBool());
    QCOMPARE(display->property("text").toString(), QStringLiteral("UI Source.wav"));
    QVERIFY(readOnly->property("visible").toBool());
    QCOMPARE(readOnly->property("text").toString(), QStringLiteral("Read-only"));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceContainerMetadata"))
                ->property("text").toString().contains(QStringLiteral("RIFF")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceFormatMetadata"))
                ->property("text").toString().contains(QStringLiteral("16-bit")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceRateMetadata"))
                ->property("text").toString().contains(QStringLiteral("44100")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceChannelsMetadata"))
                ->property("text").toString().contains(QStringLiteral("Mono")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceFramesMetadata"))
                ->property("text").toString().contains(QStringLiteral("3")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceDurationMetadata"))
                ->property("text").toString().contains(QStringLiteral("0:00.000")));
    QVERIFY(playPause->property("enabled").toBool());
    QVERIFY(stop->property("enabled").toBool());
    QCOMPARE(playbackState->property("text").toString(), QStringLiteral("Stopped"));
    QVERIFY(playbackTime->property("text").toString().contains(QStringLiteral("/")));

    waveformPresentation.publish_building();
    QCoreApplication::processEvents();
    QCOMPARE(waveformPresentation.state_token(), QStringLiteral("BUILDING"));
    QVERIFY(waveformStatus->property("visible").toBool());
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceWaveformBuildingIndicator"))
                ->property("visible").toBool());

    waveformPresentation.publish_ready(valid_summary());
    QCoreApplication::processEvents();
    QCOMPARE(waveformPresentation.state_token(), QStringLiteral("READY"));
    QVERIFY(waveformItem->isVisible());
    QVERIFY(!waveformStatus->property("visible").toBool());
    QCOMPARE(waveformPresentation.channel_count(), 1);
    QVERIFY(waveformPresentation.base_bucket_count() > 0);
    QVERIFY(waveformPresentation.payload_bytes() > 0);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceWaveformMonoLane"))
                ->property("visible").toBool());

    QVERIFY(QMetaObject::invokeMethod(playPause, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(observedPlayback->playCalls, 1);
    QCOMPARE(playPause->property("text").toString(), QStringLiteral("Pause"));
    QVERIFY(QMetaObject::invokeMethod(playPause, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(observedPlayback->pauseCalls, 1);
    QCOMPARE(playbackState->property("text").toString(), QStringLiteral("Paused"));
    QVERIFY(QMetaObject::invokeMethod(stop, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(observedPlayback->stopCalls, 1);
    QCOMPARE(playbackState->property("text").toString(), QStringLiteral("Stopped"));

    const auto invalidPath = write_file(
        directory, QStringLiteral("Invalid.wav"), QByteArray{"bad"});
    model.selectSource(QUrl::fromLocalFile(invalidPath));
    QCoreApplication::processEvents();
    QVERIFY(error->property("visible").toBool());
    QVERIFY(!error->property("text").toString().isEmpty());
    QCOMPARE(display->property("text").toString(), QStringLiteral("UI Source.wav"));

    waveformPresentation.publish_failed(QStringLiteral("Injected waveform failure."));
    QCoreApplication::processEvents();
    QCOMPARE(waveformPresentation.state_token(), QStringLiteral("FAILED"));
    QVERIFY(waveformStatus->property("visible").toBool());
    QVERIFY(waveformRetry->property("visible").toBool());
    QSignalSpy retrySpy{
        &waveformPresentation,
        &ui::WaveformPresentation::retryRequested};
    QVERIFY(QMetaObject::invokeMethod(waveformRetry, "clicked"));
    QCOMPARE(retrySpy.count(), 1);
    waveformPresentation.publish_ready(valid_summary());
    QCoreApplication::processEvents();

    auto* window = qobject_cast<QWindow*>(root);
    QVERIFY(window);
    window->resize(800, 500);
    QCoreApplication::processEvents();
    QCOMPARE(window->size(), QSize(800, 500));
    window->resize(1600, 900);
    QCoreApplication::processEvents();
    QCOMPARE(window->size(), QSize(1600, 900));
    window->close();
    QCoreApplication::processEvents();
}

}  // namespace rgsml::tests

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    rgsml::tests::SourceMetadataPanelSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_source_metadata_panel.moc"
