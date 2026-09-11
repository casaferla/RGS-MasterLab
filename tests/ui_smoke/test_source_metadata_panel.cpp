#include "audition_region_view_model.hpp"
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
#include <QRegularExpressionValidator>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
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
    using namespace wav_support;
    std::vector<std::int64_t> codes(1'000U);
    for (std::size_t index = 0; index < codes.size(); ++index) {
        codes[index] = static_cast<std::int64_t>((index * 97U) % 65'536U) - 32'768;
    }
    const auto bytes = make_wav(
        1U, 16U, 1U, 44'100U, pcm_payload(codes, 16U));
    return QByteArray{
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()),
    };
}

[[nodiscard]] std::shared_ptr<const audio::WaveformSummary> valid_summary()
{
    using namespace wav_support;
    const auto qbytes = valid_wav();
    Bytes bytes(static_cast<std::size_t>(qbytes.size()));
    std::memcpy(bytes.data(), qbytes.constData(), static_cast<std::size_t>(qbytes.size()));
    auto reader = audio::WavReader::open(
        memory_reader(std::move(bytes), std::make_shared<ReaderControl>()));
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
    app::AuditionRegionViewModel auditionRegion{&playbackTransport};
    model.set_source_committed_handler(
        [&model, &auditionRegion](const core::ResourceReference&) {
            const auto frames = core::FrameCount::create(model.frame_count());
            const auto rate = core::SampleRate::create(model.sample_rate_hz());
            QVERIFY(frames && rate);
            auditionRegion.source_committed(*frames.value(), *rate.value());
        });
    waveformPresentation.set_seek_handler(
        [&auditionRegion](core::FrameIndex position) {
            return auditionRegion.seek(position);
        });
    waveformPresentation.set_region_commit_handler(
        [&auditionRegion](core::FrameRange candidate) {
            return auditionRegion.set_region(candidate);
        });
    connect(
        &waveformPresentation,
        &ui::WaveformPresentation::changed,
        &auditionRegion,
        [&waveformPresentation, &auditionRegion] {
            auditionRegion.set_waveform_ready(waveformPresentation.ready());
        });
    connect(
        &auditionRegion,
        &app::AuditionRegionViewModel::changed,
        &waveformPresentation,
        [&waveformPresentation, &auditionRegion] {
            waveformPresentation.set_displayed_region(auditionRegion.region());
        });
    QQmlApplicationEngine engine;
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
    engine.rootContext()->setContextProperty(
        QStringLiteral("sourceSelection"), &model);
    engine.rootContext()->setContextProperty(
        QStringLiteral("playbackTransport"), &playbackTransport);
    engine.rootContext()->setContextProperty(
        QStringLiteral("sourceWaveform"), &waveformPresentation);
    engine.rootContext()->setContextProperty(
        QStringLiteral("auditionRegion"), &auditionRegion);
    engine.loadFromModule("Rgsml.Ui", "Main");
    QCOMPARE(engine.rootObjects().size(), 1);

    auto* root = engine.rootObjects().front();
    auto* window = qobject_cast<QWindow*>(root);
    QVERIFY(window);
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
    QCOMPARE(waveformItem->acceptedMouseButtons(), Qt::LeftButton);
    auto* zoomIn = root->findChild<QObject*>(QStringLiteral("waveformZoomInButton"));
    auto* zoomOut = root->findChild<QObject*>(QStringLiteral("waveformZoomOutButton"));
    auto* fitSource = root->findChild<QObject*>(QStringLiteral("waveformFitSourceButton"));
    const std::array segmentNames{
        QStringLiteral("auditionRegionStartHours"),
        QStringLiteral("auditionRegionStartMinutes"),
        QStringLiteral("auditionRegionStartSeconds"),
        QStringLiteral("auditionRegionStartFraction"),
        QStringLiteral("auditionRegionEndHours"),
        QStringLiteral("auditionRegionEndMinutes"),
        QStringLiteral("auditionRegionEndSeconds"),
        QStringLiteral("auditionRegionEndFraction"),
    };
    std::array<QObject*, 8> segmentFields{};
    for (std::size_t index = 0; index < segmentNames.size(); ++index) {
        segmentFields[index] = root->findChild<QObject*>(segmentNames[index]);
        QVERIFY(segmentFields[index]);
    }
    auto* fitRegion = root->findChild<QObject*>(QStringLiteral("waveformFitRegionButton"));
    auto* loopRegion = root->findChild<QObject*>(QStringLiteral("auditionRegionLoopCheckBox"));
    auto* clearRegion = root->findChild<QObject*>(QStringLiteral("auditionRegionClearButton"));
    QVERIFY(zoomIn && zoomOut && fitSource);
    QVERIFY(fitRegion && loopRegion && clearRegion);
    QVERIFY(!zoomIn->property("enabled").toBool());
    for (auto* field : segmentFields) {
        QVERIFY(!field->property("enabled").toBool());
    }
    const auto verifyTimeCharacterFilter = [](QObject* field) {
        auto* validator = field->property("validator").value<QValidator*>();
        QVERIFY(validator);
        QString partial = QStringLiteral("5");
        int partialPosition = partial.size();
        QCOMPARE(
            validator->validate(partial, partialPosition),
            QValidator::Acceptable);
        QString valid = QStringLiteral("123456789");
        int validPosition = valid.size();
        QCOMPARE(
            validator->validate(valid, validPosition),
            QValidator::Acceptable);
        QString empty;
        int emptyPosition = 0;
        QCOMPARE(
            validator->validate(empty, emptyPosition),
            QValidator::Acceptable);
        QString invalid = QStringLiteral("1a:-2");
        int invalidPosition = invalid.size();
        QCOMPARE(
            validator->validate(invalid, invalidPosition),
            QValidator::Invalid);
    };
    for (auto* field : segmentFields) {
        verifyTimeCharacterFilter(field);
    }
    QCOMPARE(segmentFields[1]->property("maximumLength").toInt(), 2);
    QCOMPARE(segmentFields[2]->property("maximumLength").toInt(), 2);
    QCOMPARE(segmentFields[3]->property("maximumLength").toInt(), 9);
    QCOMPARE(segmentFields[7]->property("maximumLength").toInt(), 9);
    for (const auto& separatorName : {
             QStringLiteral("auditionRegionStartHoursMinutesSeparator"),
             QStringLiteral("auditionRegionStartMinutesSecondsSeparator"),
             QStringLiteral("auditionRegionStartSecondsFractionSeparator"),
             QStringLiteral("auditionRegionEndHoursMinutesSeparator"),
             QStringLiteral("auditionRegionEndMinutesSecondsSeparator"),
             QStringLiteral("auditionRegionEndSecondsFractionSeparator")}) {
        auto* separator = root->findChild<QObject*>(separatorName);
        QVERIFY(separator);
        QVERIFY(!separator->property("activeFocusOnTab").toBool());
    }

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
                ->property("text").toString().contains(QStringLiteral("1000")));
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceDurationMetadata"))
                ->property("text").toString().contains(QStringLiteral("0:00.023")));
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
    QVERIFY(waveformPresentation.can_navigate());
    QVERIFY(zoomIn->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(zoomIn, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY(!waveformPresentation.full_fit());
    QVERIFY(zoomOut->property("enabled").toBool());
    QVERIFY(fitSource->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(fitSource, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY(waveformPresentation.full_fit());

    const auto oneFrameRegion = core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{1});
    QVERIFY(oneFrameRegion);
    QVERIFY(auditionRegion.set_region(*oneFrameRegion.value()));
    QCoreApplication::processEvents();
    for (auto* field : segmentFields) {
        QVERIFY(field->property("enabled").toBool());
    }
    QVERIFY(fitRegion->property("enabled").toBool());
    QVERIFY(loopRegion->property("enabled").toBool());
    QVERIFY(clearRegion->property("enabled").toBool());

    auto* firstSegment = qobject_cast<QQuickItem*>(segmentFields.front());
    QVERIFY(firstSegment);
    firstSegment->forceActiveFocus(Qt::TabFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(firstSegment->hasActiveFocus());
    QCOMPARE(segmentFields.front()->property("selectionStart").toInt(), 0);
    QCOMPARE(
        segmentFields.front()->property("selectionEnd").toInt(),
        segmentFields.front()->property("text").toString().size());
    segmentFields.front()->setProperty("text", QStringLiteral("0"));
    QTest::keyClick(window, Qt::Key_Tab);
    QCoreApplication::processEvents();
    QCOMPARE(
        segmentFields.front()->property("text").toString(),
        QStringLiteral("00"));
    auto* secondSegment = qobject_cast<QQuickItem*>(segmentFields[1]);
    QVERIFY(secondSegment && secondSegment->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_Backtab);
    QCoreApplication::processEvents();
    QVERIFY(firstSegment->hasActiveFocus());
    for (std::size_t index = 1; index < segmentFields.size(); ++index) {
        QTest::keyClick(window, Qt::Key_Tab);
        QCoreApplication::processEvents();
        auto* focused = qobject_cast<QQuickItem*>(segmentFields[index]);
        QVERIFY(focused && focused->hasActiveFocus());
    }
    for (std::size_t index = segmentFields.size() - 1U; index > 0U; --index) {
        QTest::keyClick(window, Qt::Key_Backtab);
        QCoreApplication::processEvents();
        auto* focused = qobject_cast<QQuickItem*>(segmentFields[index - 1U]);
        QVERIFY(focused && focused->hasActiveFocus());
    }

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
    QVERIFY(auditionRegion.has_region());

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
