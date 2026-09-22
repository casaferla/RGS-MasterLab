#include "audition_region_view_model.hpp"
#include "audition_source_selector.hpp"
#include "eq_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "playback_transport_view_model.hpp"
#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "waveform_item.hpp"
#include "waveform_presentation.hpp"

#include "../audio_golden/wav/golden_vectors.hpp"
#include "../unit/audio/wav_test_support.hpp"
#include "../unit/platform/fake_playback_service.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QRegularExpressionValidator>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickStyle>
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

[[nodiscard]] QByteArray visual_wav()
{
    using namespace wav_support;
    constexpr std::size_t frameCount = 192'000U;
    std::vector<std::int64_t> codes;
    codes.reserve(frameCount * 2U);
    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        const auto phase = static_cast<std::int64_t>(frame % 1'600U);
        const auto triangle = phase < 800 ? phase - 400 : 1'200 - phase;
        const auto section = static_cast<std::int64_t>((frame / 12'000U) % 8U);
        const auto envelope = 16 + section * 5;
        const auto left = std::clamp<std::int64_t>(triangle * envelope, -30'000, 30'000);
        const auto right = std::clamp<std::int64_t>(
            ((phase + 211) % 1'600 - 400) * (50 - section * 3),
            -30'000,
            30'000);
        codes.push_back(left);
        codes.push_back(right);
    }
    const auto bytes = make_wav(
        1U, 16U, 2U, 48'000U, pcm_payload(codes, 16U));
    return QByteArray{
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()),
    };
}

[[nodiscard]] std::shared_ptr<const audio::WaveformSummary> summary_from_wav(
    const QByteArray& qbytes)
{
    using namespace wav_support;
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

[[nodiscard]] std::shared_ptr<const audio::WaveformSummary> valid_summary()
{
    return summary_from_wav(valid_wav());
}

[[nodiscard]] bool capture_visual_evidence(
    QWindow* window,
    const QString& fileName,
    QSize size)
{
    const auto outputDirectory = qEnvironmentVariable("RGSML_GUI01_EVIDENCE_DIR");
    if (outputDirectory.isEmpty()) {
        return true;
    }
    if (!QDir{}.mkpath(outputDirectory)) {
        return false;
    }
    auto* quickWindow = qobject_cast<QQuickWindow*>(window);
    if (quickWindow == nullptr) {
        return false;
    }
    window->resize(size);
    window->show();
    QTest::qWait(150);
    const auto image = quickWindow->grabWindow();
    return !image.isNull()
        && image.save(QDir{outputDirectory}.filePath(fileName), "PNG");
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
    app::AuditionSourceSelector auditionSelector{&playbackTransport};
    app::EqViewModel eqViewModel{
        [&auditionSelector] {
            return auditionSelector.prepared_realization_snapshot();
        },
        [&auditionSelector](render::RenderResult result) {
            const bool wasProcessed = auditionSelector.active_target() == app::AuditionTarget::PROCESSED;
            auto status = auditionSelector.set_processed_realization(std::move(result));
            if (status && wasProcessed) {
                static_cast<void>(auditionSelector.switch_to(app::AuditionTarget::PROCESSED));
            }
            return status;
        }
    };
    app::GoldSelectionViewModel goldSelection{&auditionSelector};
    app::ProjectSessionViewModel projectSession{
        &model, &goldSelection, &auditionRegion, &playbackTransport};
    playbackTransport.set_pcm_prepare_handler(
        [observedPlayback](audio::AudioBufferView view) {
            observedPlayback->state = core::PlaybackState::STOPPED;
            observedPlayback->position = view.absolute_start_frame();
            observedPlayback->duration = *core::FrameCount::create(
                view.absolute_end_frame().value()).value();
            observedPlayback->loop.reset();
            return core::Status::success();
        });
    model.set_source_committed_handler(
        [&model, &auditionRegion, &auditionSelector, &goldSelection, &eqViewModel](
            const core::ResourceReference& source) {
            const auto frames = core::FrameCount::create(model.frame_count());
            const auto rate = core::SampleRate::create(model.sample_rate_hz());
            QVERIFY(frames && rate);
            auditionRegion.source_committed(*frames.value(), *rate.value());
            QVERIFY(auditionSelector.source_committed(source));
            QVERIFY(auditionSelector.switch_to(app::AuditionTarget::PREPARED));
            eqViewModel.trigger_preview();
            goldSelection.sourceChanged();
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
    engine.rootContext()->setContextProperty(
        QStringLiteral("auditionSelector"), &auditionSelector);
    engine.rootContext()->setContextProperty(
        QStringLiteral("goldSelection"), &goldSelection);
    engine.rootContext()->setContextProperty(
        QStringLiteral("projectSession"), &projectSession);
    engine.rootContext()->setContextProperty(
        QStringLiteral("eqViewModel"), &eqViewModel);
    engine.loadFromModule("Rgsml.Ui", "Main");
    QCOMPARE(engine.rootObjects().size(), 1);

    auto* root = engine.rootObjects().front();
    auto* window = qobject_cast<QWindow*>(root);
    QVERIFY(window);
    QCOMPARE(window->minimumWidth(), 1184);
    QCOMPARE(window->minimumHeight(), 688);
    auto* header = root->findChild<QObject*>(QStringLiteral("applicationHeader"));
    QVERIFY(header);
    QCOMPARE(header->property("height").toInt(), 48);
    auto* desktopMenu = root->findChild<QObject*>(QStringLiteral("desktopMenuBar"));
    QVERIFY(desktopMenu);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("controlStripElasticCenter")));
    auto* statusBar = root->findChild<QObject*>(QStringLiteral("statusBar"));
    auto* statusIndicator = root->findChild<QObject*>(QStringLiteral("statusReadyIndicator"));
    QVERIFY(statusBar && statusIndicator);
    QCOMPARE(statusBar->property("height").toInt(), 24);
    QCOMPARE(statusIndicator->property("width").toInt(), 8);
    QCOMPARE(statusIndicator->property("height").toInt(), 8);
    QCOMPARE(statusIndicator->property("color").value<QColor>(), QColor{QStringLiteral("#00E6E6")});
    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("gui01_1184x688_unavailable.png"),
        QSize{1184, 688}));
    auto* sourceOpen = root->findChild<QObject*>(QStringLiteral("sourceOpenButton"));
    QVERIFY(sourceOpen);
    auto* sourcePanel = root->findChild<QObject*>(QStringLiteral("sourceMetadataPanel"));
    QVERIFY(sourcePanel);
    QCOMPARE(sourcePanel->property("height").toInt(), 72);
    QCOMPARE(sourceOpen->property("height").toInt(), 32);
    QCOMPARE(sourceOpen->property("width").toInt(), 110);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceFileDialog")));
    auto* minimizeButton = root->findChild<QObject*>(QStringLiteral("windowMinimizeButton"));
    auto* maximizeButton = root->findChild<QObject*>(QStringLiteral("windowMaximizeButton"));
    auto* closeButton = root->findChild<QObject*>(QStringLiteral("windowCloseButton"));
    QVERIFY(minimizeButton && maximizeButton && closeButton);
    const auto itemCenter = [](QObject* object) {
        auto* item = qobject_cast<QQuickItem*>(object);
        Q_ASSERT(item != nullptr);
        return item->mapToScene(QPointF{item->width() * 0.5, item->height() * 0.5}).toPoint();
    };
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("gui01_c1_caption_normal.png"), QSize{1440, 900}));
    QTest::mouseMove(window, itemCenter(maximizeButton));
    QTest::qWait(50);
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("gui01_c1_caption_hover.png"), QSize{1440, 900}));
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier,
        itemCenter(maximizeButton));
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("gui01_c1_caption_pressed.png"), QSize{1440, 900}));
    QTest::mouseMove(window, QPoint{640, 220});
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint{640, 220});
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
    auto* preparedTarget = root->findChild<QObject*>(
        QStringLiteral("auditionPreparedButton"));
    auto* processedTarget = root->findChild<QObject*>(
        QStringLiteral("auditionProcessedButton"));
    auto* goldTarget = root->findChild<QObject*>(
        QStringLiteral("auditionGoldButton"));
    auto* goldState = root->findChild<QObject*>(QStringLiteral("goldStateLabel"));
    QVERIFY(preparedTarget && processedTarget && goldTarget);
    QVERIFY(goldState);
    QVERIFY(!preparedTarget->property("enabled").toBool());
    QVERIFY(!processedTarget->property("enabled").toBool());
    QVERIFY(!goldTarget->property("enabled").toBool());
    QCOMPARE(goldState->property("text").toString(), QStringLiteral("Gold: not loaded"));
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
    auto* continuousZoom = root->findChild<QObject*>(
        QStringLiteral("waveformZoomControl"));
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
    QVERIFY(zoomIn && zoomOut && continuousZoom && fitSource);
    QCOMPARE(zoomOut->property("width").toInt(), 32);
    QCOMPARE(zoomOut->property("height").toInt(), 32);
    QCOMPARE(zoomIn->property("width").toInt(), 32);
    QCOMPARE(fitSource->property("width").toInt(), 32);
    QCOMPARE(continuousZoom->property("width").toInt(), 160);
    QVERIFY(fitRegion && loopRegion && clearRegion);

    const std::array menuNames{
        QStringLiteral("File"),
        QStringLiteral("Edit"),
        QStringLiteral("View"),
        QStringLiteral("Transport"),
        QStringLiteral("Help"),
    };
    for (const auto& menuName : menuNames) {
        auto* label = root->findChild<QObject*>(
            QStringLiteral("desktopMenuBarLabel_") + menuName);
        QVERIFY(label);
        QCOMPARE(label->property("text").toString(), menuName);
        QVERIFY(label->parent());
        QCOMPARE(
            label->parent()->property("text").toString(),
            QStringLiteral("&") + menuName);
    }
    QTest::keyClick(window, Qt::Key_F10);
    QCoreApplication::processEvents();
    QVERIFY(desktopMenu->property("activeFocus").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QCoreApplication::processEvents();

    auto* fileMenu = root->findChild<QObject*>(QStringLiteral("desktopFileMenu"));
    auto* fileMenuLabel = root->findChild<QObject*>(
        QStringLiteral("desktopMenuBarLabel_File"));
    auto* fileMenuBarItem = qobject_cast<QQuickItem*>(fileMenuLabel->parent());
    QVERIFY(fileMenu && fileMenuBarItem);
    const auto fileMenuCenter = fileMenuBarItem->mapToScene(QPointF{
        fileMenuBarItem->width() / 2, fileMenuBarItem->height() / 2});
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
        fileMenuCenter.toPoint());
    QTest::qWait(120);
    QVERIFY2(fileMenu->property("visible").toBool(),
        "Clicking File must open its menu popup");
    QVERIFY2(fileMenu->property("width").toReal() >= 230,
        "The File popup must have a visible width");
    auto* goldMenuItem = qobject_cast<QQuickItem*>(
        root->findChild<QObject*>(QStringLiteral("menuOpenGold")));
    auto* goldFileDialog = root->findChild<QObject*>(
        QStringLiteral("goldFileDialog"));
    QVERIFY(goldMenuItem && goldFileDialog);
    QVERIFY(goldMenuItem->property("enabled").toBool());
    QVERIFY(goldMenuItem->width() > 0);
    auto* eqMenuItem = root->findChild<QObject*>(
        QStringLiteral("menuViewParametricEq"));
    QVERIFY(eqMenuItem);
    QVERIFY(!eqMenuItem->property("enabled").toBool());

    auto* openProjectItem = root->findChild<QObject*>(
        QStringLiteral("menuOpenProject"));
    auto* saveProjectItem = root->findChild<QObject*>(
        QStringLiteral("menuSaveProjectAs"));
    QVERIFY(openProjectItem && saveProjectItem);
    QVERIFY(openProjectItem->property("enabled").toBool());
    QVERIFY(!saveProjectItem->property("enabled").toBool());
    QVERIFY(root->findChild<QObject*>(
        QStringLiteral("projectOpenFileDialog")));
    QVERIFY(root->findChild<QObject*>(
        QStringLiteral("projectSaveFileDialog")));
    QTest::keyClick(window, Qt::Key_Escape);
    QCoreApplication::processEvents();

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
    QCOMPARE(segmentFields[0]->property("width").toInt(), 44);
    QCOMPARE(segmentFields[1]->property("width").toInt(), 44);
    QCOMPARE(segmentFields[2]->property("width").toInt(), 44);
    QCOMPARE(segmentFields[3]->property("width").toInt(), 96);
    QCOMPARE(segmentFields[0]->property("height").toInt(), 28);
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
    QVERIFY(saveProjectItem->property("enabled").toBool());
    QVERIFY(eqMenuItem->property("enabled").toBool());

    // Exercise Parametric EQ Tool Window & Editor
    auto* eqToolWindow = root->findChild<QObject*>(QStringLiteral("parametricEqToolWindow"));
    QVERIFY(eqToolWindow);
    auto* eqWindowObj = qobject_cast<QWindow*>(eqToolWindow);
    QVERIFY(eqWindowObj);
    QCOMPARE(eqToolWindow->property("title").toString(), QStringLiteral("Parametric EQ — RGS MasterLab"));

    QVERIFY(QMetaObject::invokeMethod(eqMenuItem, "triggered"));
    QCoreApplication::processEvents();
    QVERIFY(eqToolWindow->property("visible").toBool());

    auto* eqEditor = eqToolWindow->findChild<QObject*>(QStringLiteral("parametricEqEditor"));
    QVERIFY(eqEditor);
    auto* eqGraph = eqEditor->findChild<QObject*>(QStringLiteral("parametricEqGraph"));
    QVERIFY(eqGraph);
    QVERIFY(!eqViewModel.selected_band_response_points().isEmpty());

    auto* addBandBtn = eqEditor->findChild<QObject*>(QStringLiteral("addBandButton"));
    auto* removeBandBtn = eqEditor->findChild<QObject*>(QStringLiteral("removeBandButton"));
    QVERIFY(addBandBtn && removeBandBtn);
    QCOMPARE(eqViewModel.band_count(), 1);
    QVERIFY(addBandBtn->property("enabled").toBool());
    QVERIFY(!removeBandBtn->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(addBandBtn, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.band_count(), 2);
    QVERIFY(removeBandBtn->property("enabled").toBool());

    auto* routeMidBtn = eqEditor->findChild<QObject*>(QStringLiteral("routingButton_MID"));
    QVERIFY(routeMidBtn);
    QVERIFY(QMetaObject::invokeMethod(routeMidBtn, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY(eqViewModel.mixed_routing());
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_eq_editor_mixed_routing.png"), QSize{1040, 660}));

    eqViewModel.setDraftFrequencyText(QStringLiteral("99999"));
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.validation_field(), QStringLiteral("frequency"));
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_eq_editor_invalid_draft.png"), QSize{1040, 660}));

    eqViewModel.cancelDraft();
    QCoreApplication::processEvents();
    QVERIFY(eqViewModel.validation_field().isEmpty());
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_eq_editor_1040x660_active.png"), QSize{1040, 660}));

    auto* abBypassBtn = eqEditor->findChild<QObject*>(QStringLiteral("abButtonBypass"));
    QVERIFY(abBypassBtn);
    QVERIFY(QMetaObject::invokeMethod(abBypassBtn, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY(eqViewModel.bypass());

    auto* abActiveBtn = eqEditor->findChild<QObject*>(QStringLiteral("abButtonActive"));
    QVERIFY(abActiveBtn);
    QVERIFY(QMetaObject::invokeMethod(abActiveBtn, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY(!eqViewModel.bypass());

    // Window close preserves state
    eqWindowObj->close();
    QCoreApplication::processEvents();
    QVERIFY(!eqToolWindow->property("visible").toBool());
    QCOMPARE(eqViewModel.band_count(), 2);

    QVERIFY(QMetaObject::invokeMethod(eqMenuItem, "triggered"));
    QCoreApplication::processEvents();
    QVERIFY(eqToolWindow->property("visible").toBool());
    QCOMPARE(eqViewModel.band_count(), 2);
    eqWindowObj->close();
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
    QVERIFY(!playbackTime->property("text").toString().isEmpty());
    QCOMPARE(root->findChild<QObject*>(QStringLiteral("activeAuditionTargetLabel"))
                 ->property("text").toString(), QStringLiteral("Active: PREPARED"));
    QVERIFY(preparedTarget->property("enabled").toBool());
    QVERIFY(preparedTarget->property("selected").toBool());

    const auto goldPath = write_file(
        directory, QStringLiteral("UI Gold.wav"), valid_wav());
    goldSelection.selectGold(QUrl::fromLocalFile(goldPath));
    QCoreApplication::processEvents();
    QVERIFY(goldSelection.has_gold());
    QVERIFY(goldTarget->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(goldTarget, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(root->findChild<QObject*>(QStringLiteral("activeAuditionTargetLabel"))
                 ->property("text").toString(), QStringLiteral("Active: GOLD"));
    QVERIFY(!auditionSelector.source_playhead_visible());
    goldSelection.clearGold();
    QCoreApplication::processEvents();
    QVERIFY(!goldSelection.has_gold());
    QVERIFY(auditionSelector.source_playhead_visible());
    QCOMPARE(root->findChild<QObject*>(QStringLiteral("activeAuditionTargetLabel"))
                 ->property("text").toString(), QStringLiteral("Active: PREPARED"));

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
    auto* loopRegionItem = qobject_cast<QQuickItem*>(loopRegion);
    QVERIFY(loopRegionItem);
    loopRegionItem->forceActiveFocus(Qt::TabFocusReason);
    auditionRegion.requestLoopEnabled(true);
    QCoreApplication::processEvents();
    QVERIFY(loopRegion->property("checked").toBool());
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("gui01_c7_region_actions_toggle_focus.png"), QSize{1440, 900}));
    auditionRegion.requestLoopEnabled(false);
    QCoreApplication::processEvents();

    const auto visualSourceBytes = visual_wav();
    const auto visualSourcePath = write_file(
        directory, QStringLiteral("GUI-01 Visual Source.wav"), visualSourceBytes);
    QVERIFY(!visualSourcePath.isEmpty());
    model.selectSource(QUrl::fromLocalFile(visualSourcePath));
    waveformPresentation.publish_ready(summary_from_wav(visualSourceBytes));
    const auto visualRegion = core::FrameRange::create(
        core::FrameIndex{48'000}, core::FrameIndex{144'000});
    QVERIFY(visualRegion);
    QVERIFY(auditionRegion.set_region(*visualRegion.value()));
    QCoreApplication::processEvents();
    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("gui01_1440x900_prepared.png"),
        QSize{1440, 900}));
    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("gui01_1184x688_prepared.png"),
        QSize{1184, 688}));
    auto* zoomInItem = qobject_cast<QQuickItem*>(zoomIn);
    QVERIFY(zoomInItem);
    zoomInItem->forceActiveFocus(Qt::TabFocusReason);
    QTest::mouseMove(window, itemCenter(zoomIn));
    QCoreApplication::processEvents();
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("gui01_c3_navigator_hover_focus.png"), QSize{1440, 900}));

    goldSelection.selectGold(QUrl::fromLocalFile(goldPath));
    QCoreApplication::processEvents();
    QVERIFY(QMetaObject::invokeMethod(goldTarget, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("gui01_1440x900_gold.png"),
        QSize{1440, 900}));
    QVERIFY(QMetaObject::invokeMethod(zoomIn, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY(zoomOut->property("enabled").toBool());
    QVERIFY(fitSource->property("enabled").toBool());

    const std::array globalTabOrder{
        sourceOpen,
        waveformObject,
        zoomOut,
        continuousZoom,
        zoomIn,
        fitSource,
        preparedTarget,
        goldTarget,
        stop,
        playPause,
        segmentFields[0],
        segmentFields[1],
        segmentFields[2],
        segmentFields[3],
        segmentFields[4],
        segmentFields[5],
        segmentFields[6],
        segmentFields[7],
        fitRegion,
        loopRegion,
        clearRegion,
    };
    auto* firstGlobalTabItem = qobject_cast<QQuickItem*>(globalTabOrder.front());
    QVERIFY(firstGlobalTabItem);
    firstGlobalTabItem->forceActiveFocus(Qt::TabFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(firstGlobalTabItem->hasActiveFocus());
    for (std::size_t index = 1; index < globalTabOrder.size(); ++index) {
        QTest::keyClick(window, Qt::Key_Tab);
        QCoreApplication::processEvents();
        auto* focused = qobject_cast<QQuickItem*>(globalTabOrder[index]);
        QVERIFY(focused && focused->hasActiveFocus());
    }
    QTest::keyClick(window, Qt::Key_Tab);
    QCoreApplication::processEvents();
    QVERIFY(firstGlobalTabItem->hasActiveFocus());
    qInfo().noquote()
        << "GUI01_GLOBAL_TAB_ORDER=Source -> Waveform -> Precision Navigator"
           " -> Audition Target Selector -> Transport -> Region Time Editor"
           " -> Region Actions";
    qInfo().noquote()
        << "GUI01_REGION_TIME_EDITOR_TAB_ORDER=Start HH -> MM -> SS -> FRACTION"
           " -> End HH -> MM -> SS -> FRACTION";

    goldSelection.clearGold();
    QCoreApplication::processEvents();

    model.selectSource(QUrl::fromLocalFile(validPath));
    waveformPresentation.publish_ready(valid_summary());
    QVERIFY(auditionRegion.set_region(*oneFrameRegion.value()));
    QCoreApplication::processEvents();

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

    segmentFields[1]->setProperty("text", QStringLiteral("99"));
    QTest::keyClick(window, Qt::Key_Tab);
    QCoreApplication::processEvents();
    QVERIFY(!auditionRegion.error_message().isEmpty());
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("gui01_c6_invalid_edit_focus.png"), QSize{1440, 900}));
    auditionRegion.clearError();
    QCoreApplication::processEvents();

    const int stopCallsBeforeTransportExercise = observedPlayback->stopCalls;
    QVERIFY(QMetaObject::invokeMethod(playPause, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(observedPlayback->playCalls, 1);
    QCOMPARE(playPause->property("iconKind").toString(), QStringLiteral("pause"));
    auto* playPauseItem = qobject_cast<QQuickItem*>(playPause);
    QVERIFY(playPauseItem);
    playPauseItem->forceActiveFocus(Qt::TabFocusReason);
    QTest::mouseMove(window, itemCenter(playPause));
    QCoreApplication::processEvents();
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("gui01_c2_transport_playing_focus.png"), QSize{1440, 900}));
    QVERIFY(QMetaObject::invokeMethod(playPause, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(observedPlayback->pauseCalls, 1);
    QCOMPARE(playbackState->property("text").toString(), QStringLiteral("Paused"));
    QVERIFY(QMetaObject::invokeMethod(stop, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(observedPlayback->stopCalls, stopCallsBeforeTransportExercise + 1);
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

    window->resize(1184, 688);
    QCoreApplication::processEvents();
    QCOMPARE(window->size(), QSize(1184, 688));
    const auto minimumWaveformHeight = waveformPanel->property("height").toReal();
    QVERIFY(minimumWaveformHeight >= 265.0);
    window->resize(1440, 900);
    QCoreApplication::processEvents();
    QCOMPARE(window->size(), QSize(1440, 900));
    QVERIFY(waveformPanel->property("height").toReal() > minimumWaveformHeight);
    window->close();
    QCoreApplication::processEvents();
}

}  // namespace rgsml::tests

int main(int argc, char* argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication application(argc, argv);
    rgsml::tests::SourceMetadataPanelSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_source_metadata_panel.moc"
