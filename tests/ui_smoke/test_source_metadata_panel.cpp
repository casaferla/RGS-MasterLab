#include "audition_region_view_model.hpp"
#include "audition_source_selector.hpp"
#include "eq_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "playback_transport_view_model.hpp"
#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "waveform_item.hpp"
#include "waveform_presentation.hpp"
#include "windows_window_chrome_helper.hpp"

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

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>
#endif

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

[[nodiscard]] QObject* find_child_by_name(QObject* parent, const QString& name)
{
    if (parent == nullptr) {
        return nullptr;
    }
    if (parent->objectName() == name) {
        return parent;
    }
    if (auto* quickItem = qobject_cast<QQuickItem*>(parent)) {
        for (auto* childItem : quickItem->childItems()) {
            if (childItem == nullptr) {
                continue;
            }
            if (childItem->objectName() == name) {
                return childItem;
            }
            if (auto* found = find_child_by_name(childItem, name)) {
                return found;
            }
        }
    }
    for (auto* childObj : parent->children()) {
        if (childObj == nullptr) {
            continue;
        }
        if (childObj->objectName() == name) {
            return childObj;
        }
        if (auto* found = find_child_by_name(childObj, name)) {
            return found;
        }
    }
    return nullptr;
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
            eqViewModel.resetForNewSource();
            QCOMPARE(eqViewModel.band_count(), 1);
            QCOMPARE(eqViewModel.selected_index(), 0);
            QCOMPARE(eqViewModel.filter_label(), QStringLiteral("BELL"));
            QCOMPARE(eqViewModel.routing_label(), QStringLiteral("STEREO"));
            QCOMPARE(eqViewModel.frequency(), 1000.0);
            QCOMPARE(eqViewModel.gain(), 0.0);
            QCOMPARE(eqViewModel.q(), 0.707);
            QVERIFY(!eqViewModel.bypass());
            QVERIFY(!eqViewModel.can_undo());
            QVERIFY(!eqViewModel.can_redo());
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

    // Exercise View Menu real mouse interaction when Source is loaded
    qInfo().noquote() << "M12B_SMOKE_PHASE=view-menu-real-click";
    auto* viewMenuLabel = root->findChild<QObject*>(QStringLiteral("desktopMenuBarLabel_View"));
    auto* viewMenu = root->findChild<QObject*>(QStringLiteral("desktopViewMenu"));
    QVERIFY(viewMenuLabel && viewMenu);
    auto* viewMenuBarItem = qobject_cast<QQuickItem*>(viewMenuLabel->parent());
    QVERIFY(viewMenuBarItem);
    const auto viewMenuCenter = viewMenuBarItem->mapToScene(QPointF{
        viewMenuBarItem->width() / 2, viewMenuBarItem->height() / 2});
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, viewMenuCenter.toPoint());
    QTest::qWait(120);
    QCoreApplication::processEvents();
    QVERIFY2(viewMenu->property("visible").toBool(), "Clicking View menu must open desktopViewMenu");
    QVERIFY2(viewMenu->property("width").toReal() >= 230, "desktopViewMenu width must be >= 230");

    auto* realEqMenuItem = root->findChild<QObject*>(QStringLiteral("menuViewParametricEq"));
    auto* realEqMenuQuickItem = qobject_cast<QQuickItem*>(realEqMenuItem);
    QVERIFY(realEqMenuItem && realEqMenuQuickItem);
    QVERIFY(realEqMenuItem->property("visible").toBool());
    QVERIFY(realEqMenuItem->property("enabled").toBool());
    QVERIFY(realEqMenuQuickItem->width() > 0);

    const auto eqMenuCenter = realEqMenuQuickItem->mapToScene(QPointF{
        realEqMenuQuickItem->width() / 2, realEqMenuQuickItem->height() / 2});
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, eqMenuCenter.toPoint());
    QTest::qWait(120);
    QCoreApplication::processEvents();

    auto* eqToolWindow = root->findChild<QObject*>(QStringLiteral("parametricEqToolWindow"));
    QVERIFY2(eqToolWindow != nullptr, "parametricEqToolWindow must exist in QML hierarchy");
    auto* eqWindowObj = qobject_cast<QWindow*>(eqToolWindow);
    QVERIFY2(eqWindowObj != nullptr, "parametricEqToolWindow must be a QWindow");
    QCOMPARE(eqToolWindow->property("title").toString(), QStringLiteral("Parametric EQ — RGS MasterLab"));
    QVERIFY2(eqToolWindow->property("visible").toBool(), "parametricEqToolWindow must be visible after real mouse click on View menu item");

    qInfo().noquote() << "M12B_SMOKE_PHASE=editor-lookup";
    auto* eqEditor = eqToolWindow->findChild<QObject*>(QStringLiteral("parametricEqEditor"));
    QVERIFY2(eqEditor != nullptr, "parametricEqEditor must exist inside eqToolWindow");
    auto* eqGraph = eqEditor->findChild<QObject*>(QStringLiteral("parametricEqGraph"));
    QVERIFY2(eqGraph != nullptr, "parametricEqGraph must exist inside eqEditor");
    QVERIFY2(!eqViewModel.selected_band_response_points().isEmpty(), "eqViewModel response points must not be empty");
    QCOMPARE(eqGraph->property("maxFreq").toDouble(), 19845.0); // TR-01: 44.1 kHz C++ endpoint (0.45 * 44100)
    QVERIFY2(eqEditor->findChild<QObject*>(QStringLiteral("spectrumAnalyzer")) == nullptr, "No fake analyzer or spectrum item must exist");

    qInfo().noquote() << "M12B_SMOKE_PHASE=band-controls";
    auto* band0Btn = find_child_by_name(eqEditor, QStringLiteral("bandSelectorButton_0"));
    QVERIFY2(band0Btn != nullptr, "bandSelectorButton_0 must exist");
    QVERIFY2(band0Btn->property("activeFocusOnTab").toBool(), "Band selector button must be Tab focusable");

    auto* addBandBtn = eqEditor->findChild<QObject*>(QStringLiteral("addBandButton"));
    auto* removeBandBtn = eqEditor->findChild<QObject*>(QStringLiteral("removeBandButton"));
    QVERIFY2(addBandBtn != nullptr && removeBandBtn != nullptr, "Add and Remove band buttons must exist");
    QCOMPARE(eqViewModel.band_count(), 1);
    QVERIFY2(!eqViewModel.can_undo(), "can_undo must be false initially for canonical Flat EQ");
    QVERIFY2(!eqViewModel.can_redo(), "can_redo must be false initially for canonical Flat EQ");
    QVERIFY2(addBandBtn->property("enabled").toBool(), "Add band button must be enabled initially");
    QVERIFY2(!removeBandBtn->property("enabled").toBool(), "Remove band button must be disabled when 1 band exists");

    QVERIFY2(QMetaObject::invokeMethod(addBandBtn, "clicked"), "Clicking addBandButton must succeed");
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.band_count(), 2);
    QCOMPARE(eqViewModel.selected_index(), 1);
    QVERIFY2(removeBandBtn->property("enabled").toBool(), "Remove band button must be enabled when 2 bands exist");

    // Keyboard Space activation on bandSelectorButton_0
    auto* band0BtnCurrent = find_child_by_name(eqEditor, QStringLiteral("bandSelectorButton_0"));
    QVERIFY2(band0BtnCurrent != nullptr, "bandSelectorButton_0 must exist after band addition");
    auto* band0Item = qobject_cast<QQuickItem*>(band0BtnCurrent);
    QVERIFY2(band0Item != nullptr, "bandSelectorButton_0 must be a QQuickItem");
    eqWindowObj->requestActivate();
    QTest::qWait(50);
    QCoreApplication::processEvents();
    band0Item->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY2(band0Item->hasActiveFocus(), "bandSelectorButton_0 must have active focus");

    const quint64 genBeforeKeyboardSelect = eqViewModel.preview_generation();
    QTest::keyClick(eqWindowObj, Qt::Key_Space);
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.selected_index(), 0);
    QCOMPARE(eqViewModel.preview_generation(), genBeforeKeyboardSelect);

    // Verify selection-only click on band handle 1 does not increment preview generation
    auto* band1BtnCurrent = find_child_by_name(eqEditor, QStringLiteral("bandSelectorButton_1"));
    QVERIFY2(band1BtnCurrent != nullptr, "bandSelectorButton_1 must exist");
    const quint64 genBeforeSelectionClick = eqViewModel.preview_generation();
    QVERIFY(QMetaObject::invokeMethod(band1BtnCurrent, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.selected_index(), 1);
    QCOMPARE(eqViewModel.preview_generation(), genBeforeSelectionClick);

    // Filter-specific control visibility & TR-02 HP/LP discrete slope commit
    auto* filterBell = find_child_by_name(eqEditor, QStringLiteral("filterButton_BELL"));
    auto* filterNotch = find_child_by_name(eqEditor, QStringLiteral("filterButton_NOTCH"));
    auto* filterLowShelf = find_child_by_name(eqEditor, QStringLiteral("filterButton_LOW_SHELF"));
    auto* filterHighPass = find_child_by_name(eqEditor, QStringLiteral("filterButton_HIGH_PASS"));
    auto* gainFieldObj = find_child_by_name(eqEditor, QStringLiteral("gainField"));
    auto* qFieldObj = find_child_by_name(eqEditor, QStringLiteral("qField"));
    auto* shelfSlopeFieldObj = find_child_by_name(eqEditor, QStringLiteral("shelfSlopeField"));

    QVERIFY2(filterBell && filterNotch && filterLowShelf && filterHighPass, "Filter buttons must exist");
    QVERIFY2(gainFieldObj->property("visible").toBool(), "Gain field must be visible for Bell filter");
    QVERIFY2(qFieldObj->property("visible").toBool(), "Q field must be visible for Bell filter");

    QVERIFY2(QMetaObject::invokeMethod(filterNotch, "clicked"), "Clicking filterButton_NOTCH must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(!gainFieldObj->property("visible").toBool(), "Gain field must be hidden for Notch filter");

    QVERIFY2(QMetaObject::invokeMethod(filterLowShelf, "clicked"), "Clicking filterButton_LOW_SHELF must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(shelfSlopeFieldObj->property("visible").toBool(), "Shelf slope field must be visible for Low Shelf filter");

    QVERIFY2(QMetaObject::invokeMethod(filterHighPass, "clicked"), "Clicking filterButton_HIGH_PASS must succeed");
    QCoreApplication::processEvents();

    auto* slope24Btn = find_child_by_name(eqEditor, QStringLiteral("slopeButton_24"));
    QVERIFY2(slope24Btn != nullptr, "slopeButton_24 must exist for High Pass filter");
    const quint64 genBeforeSlope = eqViewModel.preview_generation();
    QVERIFY2(QMetaObject::invokeMethod(slope24Btn, "clicked"), "Clicking slopeButton_24 must succeed");
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.slope_db_per_oct(), 24);
    QCOMPARE(eqViewModel.preview_generation(), genBeforeSlope + 1U);

    // Reset filter to BELL for remaining tests
    QVERIFY2(QMetaObject::invokeMethod(filterBell, "clicked"), "Resetting filter to BELL must succeed");
    QCoreApplication::processEvents();

    // Verify mixedText is hidden when mixedRouting is false on mono source
    auto* mixedIndicatorMono = find_child_by_name(eqEditor, QStringLiteral("mixedText"));
    QVERIFY2(mixedIndicatorMono != nullptr, "mixedText object must exist in eqEditor");
    QVERIFY2(!mixedIndicatorMono->property("visible").toBool(), "MIXED ROUTING ACTIVE indicator must be hidden when mixedRouting is false");

    qInfo().noquote() << "M12B_SMOKE_PHASE=mono-routing";
    auto* routeMidBtn = find_child_by_name(eqEditor, QStringLiteral("routingButton_MID"));
    QVERIFY2(routeMidBtn != nullptr, "routingButton_MID must exist");
    QVERIFY2(!routeMidBtn->property("enabled").toBool(), "MID routing must be disabled for mono source");

    qInfo().noquote() << "M12B_SMOKE_PHASE=invalid-draft";
    auto* freqInput = find_child_by_name(eqEditor, QStringLiteral("frequencyInput"));
    QVERIFY2(freqInput != nullptr, "frequencyInput control must exist");
    auto* freqInputItem = qobject_cast<QQuickItem*>(freqInput);
    QVERIFY2(freqInputItem != nullptr, "frequencyInput must be a QQuickItem");
    eqWindowObj->requestActivate();
    QTest::qWait(50);
    QCoreApplication::processEvents();
    freqInputItem->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY2(freqInputItem->hasActiveFocus(), "frequencyInput must receive active focus");

    QMetaObject::invokeMethod(freqInput, "selectAll");
    for (const char c : std::string_view{"99999"}) {
        QTest::keyClick(eqWindowObj, c);
    }
    QCoreApplication::processEvents();

    QCOMPARE(freqInput->property("text").toString(), QStringLiteral("99999"));
    QCOMPARE(eqViewModel.validation_field(), QStringLiteral("frequency"));
    auto* valMsgText = find_child_by_name(eqEditor, QStringLiteral("validationMessageText"));
    QVERIFY2(valMsgText && valMsgText->property("visible").toBool(), "Validation message text must be visible for invalid draft");
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_eq_editor_invalid_draft.png"), QSize{1040, 660}));

    QTest::keyClick(eqWindowObj, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QCOMPARE(freqInput->property("text").toString(), QStringLiteral("1000"));
    QCOMPARE(eqViewModel.frequency_text(), QStringLiteral("1000"));
    QVERIFY2(eqViewModel.validation_field().isEmpty(), "Validation field must be empty after Escape key cancel");
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_eq_editor_1040x660_active.png"), QSize{1040, 660}));

    qInfo().noquote() << "M12B_SMOKE_PHASE=ab";
    auto* abBypassBtn = eqEditor->findChild<QObject*>(QStringLiteral("abButtonBypass"));
    auto* abActiveBtn = eqEditor->findChild<QObject*>(QStringLiteral("abButtonActive"));
    auto* undoBtn = eqEditor->findChild<QObject*>(QStringLiteral("eqUndoButton"));
    auto* redoBtn = eqEditor->findChild<QObject*>(QStringLiteral("eqRedoButton"));
    auto* resetFlatBtn = eqEditor->findChild<QObject*>(QStringLiteral("eqResetFlatButton"));
    auto* overallToggleBtn = eqEditor->findChild<QObject*>(QStringLiteral("eqOverallToggleButton"));

    QVERIFY2(abBypassBtn != nullptr && abActiveBtn != nullptr, "A and B buttons must exist");
    QVERIFY2(undoBtn != nullptr && redoBtn != nullptr && resetFlatBtn != nullptr, "Undo, Redo, and Reset Flat buttons must exist");
    QVERIFY2(overallToggleBtn != nullptr, "Overall toggle button must exist");
    QVERIFY2(undoBtn->property("enabled").toBool(), "Undo button must be enabled after band addition edit");
    QVERIFY2(!redoBtn->property("enabled").toBool(), "Redo button must be disabled when redo history is empty");
    QVERIFY2(resetFlatBtn->property("enabled").toBool(), "Reset Flat button must be enabled");

    // Click Overall toggle and verify showCombinedResponse state change without audio preview request
    const quint64 genBeforeToggle = eqViewModel.preview_generation();
    QVERIFY2(QMetaObject::invokeMethod(overallToggleBtn, "clicked"), "Clicking eqOverallToggleButton must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqViewModel.show_combined_response(), "showCombinedResponse must be true after clicking Overall");
    QCOMPARE(eqViewModel.preview_generation(), genBeforeToggle);

    QVERIFY2(QMetaObject::invokeMethod(abBypassBtn, "clicked"), "Clicking abButtonBypass must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqViewModel.bypass(), "eqViewModel.bypass must be true after clicking Bypass");
    QVERIFY2(abBypassBtn->property("selected").toBool(), "Bypass button must be selected when bypassed");

    QVERIFY2(QMetaObject::invokeMethod(abActiveBtn, "clicked"), "Clicking abButtonActive must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(!eqViewModel.bypass(), "eqViewModel.bypass must be false after clicking Active");
    QVERIFY2(abActiveBtn->property("selected").toBool(), "Active button must be selected when active");

    qInfo().noquote() << "M12B_SMOKE_PHASE=reopen";
    eqWindowObj->close();
    QCoreApplication::processEvents();
    QVERIFY2(!eqToolWindow->property("visible").toBool(), "Tool window must be hidden after close");
    QCOMPARE(eqViewModel.band_count(), 2);

    QVERIFY2(QMetaObject::invokeMethod(eqMenuItem, "triggered"), "Re-triggering eqMenuItem must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqToolWindow->property("visible").toBool(), "Tool window must be visible after reopening");
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

    // Verify Mixed routing on stereo source and capture visual evidence
    qInfo().noquote() << "M12B_SMOKE_PHASE=stereo-mixed-routing";
    QVERIFY2(QMetaObject::invokeMethod(eqMenuItem, "triggered"), "Triggering eqMenuItem on stereo source must succeed");
    QCoreApplication::processEvents();
    auto* eqEditorStereo = eqToolWindow->findChild<QObject*>(QStringLiteral("parametricEqEditor"));
    QVERIFY2(eqEditorStereo != nullptr, "parametricEqEditor must exist on stereo source");

    // Capture 1-band 1040x660 evidence
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_wow_1040x660_1band.png"), QSize{1040, 660}));

    // Add bands until 6 exist
    auto* addBandStereo = eqEditorStereo->findChild<QObject*>(QStringLiteral("addBandButton"));
    QVERIFY2(addBandStereo != nullptr, "addBandButton must exist");
    while (eqViewModel.band_count() < 6) {
        QVERIFY(QMetaObject::invokeMethod(addBandStereo, "clicked"));
        QCoreApplication::processEvents();
    }
    QCOMPARE(eqViewModel.band_count(), 6);

    // Capture 6-bands 1040x660 evidence
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_wow_1040x660_6bands.png"), QSize{1040, 660}));

    // Set band 2 to MID to enable mixed routing
    auto* routeMidBtnStereo = find_child_by_name(eqEditorStereo, QStringLiteral("routingButton_MID"));
    QVERIFY2(routeMidBtnStereo != nullptr, "routingButton_MID must exist on stereo source");
    QVERIFY2(routeMidBtnStereo->property("enabled").toBool(), "routingButton_MID must be enabled on stereo source");
    QVERIFY2(QMetaObject::invokeMethod(routeMidBtnStereo, "clicked"), "Clicking routingButton_MID must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqViewModel.mixed_routing(), "eqViewModel.mixedRouting must be true after setting band 2 to MID");
    auto* mixedIndicatorText = find_child_by_name(eqEditorStereo, QStringLiteral("mixedText"));
    QVERIFY2(mixedIndicatorText != nullptr, "mixedText object must exist in eqEditorStereo");
    QVERIFY2(mixedIndicatorText->property("visible").toBool(), "MIXED ROUTING ACTIVE indicator must be visible");
    QCOMPARE(mixedIndicatorText->property("text").toString(), QStringLiteral("MIXED ROUTING ACTIVE"));

    // Capture 900x580 6-bands mixed evidence
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_wow_900x580_6bands_mixed.png"), QSize{900, 580}));
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_eq_editor_mixed_routing.png"), QSize{1040, 660}));

    // Test High Pass filter and capture 6 discrete slope choices
    auto* filterHPStereo = find_child_by_name(eqEditorStereo, QStringLiteral("filterButton_HIGH_PASS"));
    QVERIFY(filterHPStereo != nullptr && QMetaObject::invokeMethod(filterHPStereo, "clicked"));
    QCoreApplication::processEvents();
    for (int slopeVal : {6, 12, 18, 24, 36, 48}) {
        auto* slopeBtn = find_child_by_name(eqEditorStereo, QString("slopeButton_%1").arg(slopeVal));
        QVERIFY2(slopeBtn != nullptr, "All six slope buttons must exist for High Pass filter");
    }
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_wow_hp_lp_six_slope.png"), QSize{1040, 660}));

    // Reset filter to Bell
    auto* filterBellStereo = find_child_by_name(eqEditorStereo, QStringLiteral("filterButton_BELL"));
    QVERIFY(filterBellStereo != nullptr && QMetaObject::invokeMethod(filterBellStereo, "clicked"));
    QCoreApplication::processEvents();

    // Test A/B bypass evidence
    auto* abBypassStereo = eqEditorStereo->findChild<QObject*>(QStringLiteral("abButtonBypass"));
    QVERIFY(abBypassStereo != nullptr && QMetaObject::invokeMethod(abBypassStereo, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_wow_ab_bypass.png"), QSize{1040, 660}));
    auto* abActiveStereo = eqEditorStereo->findChild<QObject*>(QStringLiteral("abButtonActive"));
    QVERIFY(abActiveStereo != nullptr && QMetaObject::invokeMethod(abActiveStereo, "clicked"));
    QCoreApplication::processEvents();

    // Test keyboard focus evidence
    auto* freqFieldStereo = find_child_by_name(eqEditorStereo, QStringLiteral("frequencyInput"));
    if (auto* freqItem = qobject_cast<QQuickItem*>(freqFieldStereo)) {
        eqWindowObj->requestActivate();
        QTest::qWait(50);
        QCoreApplication::processEvents();
        freqItem->forceActiveFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
        QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_wow_keyboard_focus.png"), QSize{1040, 660}));
    }

    // Real QML multi-move graph handle drag smoke test
    qInfo().noquote() << "M12B_SMOKE_PHASE=real-graph-handle-multi-drag";
    auto* eqGraphObj = eqEditorStereo->findChild<QObject*>(QStringLiteral("parametricEqGraph"));
    QVERIFY2(eqGraphObj != nullptr, "parametricEqGraph must exist for real drag smoke test");
    auto* eqGraphItem = qobject_cast<QQuickItem*>(eqGraphObj);
    QVERIFY2(eqGraphItem != nullptr, "parametricEqGraph must be a QQuickItem");

    // Set Band 0 frequency to 100 Hz so its handle is spatially separated from other bands
    eqViewModel.selectBand(0);
    eqViewModel.setDraftFrequencyText(QStringLiteral("100"));
    QVERIFY(eqViewModel.commitDraft());
    QCoreApplication::processEvents();

    // Obtain delegate handle item for band 0 (index 0)
    QQuickItem* band0HandleItem = nullptr;
    for (auto* childItem : eqGraphItem->childItems()) {
        if (childItem != nullptr && childItem->property("index").isValid() && childItem->property("index").toInt() == 0) {
            band0HandleItem = childItem;
            break;
        }
    }
    QVERIFY2(band0HandleItem != nullptr, "Band 0 handle delegate item must exist in graph");
    QPointer<QQuickItem> trackedHandleDelegate = band0HandleItem;

    eqWindowObj->requestActivate();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    const QPoint handleCenterLocal = QPoint{
        static_cast<int>(band0HandleItem->width() * 0.5),
        static_cast<int>(band0HandleItem->height() * 0.5)
    };
    const QPoint handleCenterScene = band0HandleItem->mapToScene(handleCenterLocal).toPoint();

    const quint64 genBeforeDrag = eqViewModel.preview_generation();
    const double initialFreq = eqViewModel.frequency_text().toDouble();
    const double initialGain = eqViewModel.gain_text().toDouble();

    // Mouse press on actual handle
    QTest::mousePress(eqWindowObj, Qt::LeftButton, Qt::NoModifier, handleCenterScene);
    QTest::qWait(20);
    QCoreApplication::processEvents();

    // issue MULTIPLE mouseMove events across substantial horizontal/vertical distance
    QPoint dragPt = handleCenterScene;
    for (int step = 1; step <= 5; ++step) {
        dragPt += QPoint{15, -10}; // Move right and up
        QTest::mouseMove(eqWindowObj, dragPt);
        QTest::qWait(20);
        QCoreApplication::processEvents();

        QVERIFY2(!trackedHandleDelegate.isNull(), "Handle delegate must NOT be destroyed/recreated during drag move step");
        QCOMPARE(eqViewModel.preview_generation(), genBeforeDrag); // No render requested on intermediate drag moves
    }

    QVERIFY2(eqViewModel.frequency_text().toDouble() > initialFreq, "Draft frequency must continuously update during drag");
    QVERIFY2(eqViewModel.gain_text().toDouble() > initialGain, "Draft gain must continuously update during drag");

    // Mouse release
    QTest::mouseRelease(eqWindowObj, Qt::LeftButton, Qt::NoModifier, dragPt);
    QTest::qWait(20);
    QCoreApplication::processEvents();

    QCOMPARE(eqViewModel.preview_generation(), genBeforeDrag + 1U); // Exactly one preview generation increment on release

    // Select another band then select dragged band again to verify persistence
    eqViewModel.selectBand(1);
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.selected_index(), 1);

    eqViewModel.selectBand(0);
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.selected_index(), 0);
    QVERIFY2(eqViewModel.frequency_text().toDouble() > initialFreq, "Released frequency position must persist across re-selection");

    // Restore band 5 (6th band) routing to STEREO before switching away from stereo source
    eqViewModel.selectBand(5);
    QCoreApplication::processEvents();
    auto* routeStereoBtnStereo = find_child_by_name(eqEditorStereo, QStringLiteral("routingButton_STEREO"));
    QVERIFY2(routeStereoBtnStereo != nullptr, "routingButton_STEREO must exist");
    QVERIFY2(QMetaObject::invokeMethod(routeStereoBtnStereo, "clicked"), "Clicking routingButton_STEREO must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(!eqViewModel.mixed_routing(), "eqViewModel.mixedRouting must be false after restoring band 5 to STEREO");

    // Query OBSERVED runtime geometry at 1040x660 and 900x580
    eqWindowObj->resize(1040, 660);
    QTest::qWait(50);
    QCoreApplication::processEvents();

    auto itemGeometry = [](QObject* obj) -> QString {
        auto* item = qobject_cast<QQuickItem*>(obj);
        if (item == nullptr) return QStringLiteral("[0,0,0,0]");
        return QString("[%1,%2,%3,%4]")
            .arg(item->x()).arg(item->y()).arg(item->width()).arg(item->height());
    };

    auto* addBtn1040 = eqEditorStereo->findChild<QObject*>(QStringLiteral("addBandButton"));
    auto* removeBtn1040 = eqEditorStereo->findChild<QObject*>(QStringLiteral("removeBandButton"));
    auto* filterGroupObj = eqEditorStereo->findChild<QObject*>(QStringLiteral("eqFilterGroup"));
    auto* routingGroupObj = eqEditorStereo->findChild<QObject*>(QStringLiteral("eqRoutingGroup"));
    auto* leftBtn = find_child_by_name(eqEditorStereo, QStringLiteral("routingButton_LEFT"));

    QVERIFY2(leftBtn != nullptr, "routingButton_LEFT must exist");
    QVERIFY2(qobject_cast<QQuickItem*>(leftBtn)->isVisible(), "routingButton_LEFT must be visible at 1040x660");

    const QString geomEditor1040 = itemGeometry(eqEditorStereo);
    const QString geomAdd1040 = itemGeometry(addBtn1040);
    const QString geomRemove1040 = itemGeometry(removeBtn1040);
    const QString geomFilterGroup = itemGeometry(filterGroupObj);
    const QString geomRoutingGroup = itemGeometry(routingGroupObj);
    const QSize actualClient1040 = eqWindowObj->size();

    // Resize to 900x580 and sample compact geometry
    eqWindowObj->resize(900, 580);
    QTest::qWait(50);
    QCoreApplication::processEvents();

    const QSize actualClient900 = eqWindowObj->size();
    const QString geomAdd900 = itemGeometry(addBtn1040);
    const QString geomRemove900 = itemGeometry(removeBtn1040);

    QVERIFY2(qobject_cast<QQuickItem*>(leftBtn)->isVisible(), "routingButton_LEFT must remain visible at 900x580");

    const auto jsonGeometry = QString(R"({
  "requested_1040x660": [1040, 660],
  "actual_client_1040x660": [%1, %2],
  "observed_1040x660": {
    "editor": %3,
    "add_button": %4,
    "remove_button": %5,
    "filter_group": %6,
    "routing_group": %7
  },
  "requested_900x580": [900, 580],
  "actual_client_900x580": [%8, %9],
  "observed_900x580": {
    "add_button_compact": %10,
    "remove_button_compact": %11
  }
})")
        .arg(actualClient1040.width()).arg(actualClient1040.height())
        .arg(geomEditor1040)
        .arg(geomAdd1040)
        .arg(geomRemove1040)
        .arg(geomFilterGroup)
        .arg(geomRoutingGroup)
        .arg(actualClient900.width()).arg(actualClient900.height())
        .arg(geomAdd900)
        .arg(geomRemove900);

    const auto evidenceDir = qEnvironmentVariable("RGSML_GUI01_EVIDENCE_DIR");
    if (!evidenceDir.isEmpty() && QDir{}.mkpath(evidenceDir)) {
        QFile jsonFile{QDir{evidenceDir}.filePath(QStringLiteral("m12b_wow_runtime_geometry.json"))};
        if (jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            jsonFile.write(jsonGeometry.toUtf8());
            jsonFile.close();
        }
    }

    eqWindowObj->close();
    QCoreApplication::processEvents();

    QVERIFY(capture_visual_evidence(window, QStringLiteral("main_shell_1440x900_postwow.png"), QSize{1440, 900}));
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

    const std::array studioButtonNames{
        QStringLiteral("auditionPreparedButton"),
        QStringLiteral("auditionProcessedButton"),
        QStringLiteral("auditionGoldButton"),
        QStringLiteral("waveformFitRegionButton"),
        QStringLiteral("auditionRegionClearButton"),
        QStringLiteral("sourceOpenButton"),
    };
    for (const auto& btnName : studioButtonNames) {
        auto* btnObj = root->findChild<QObject*>(btnName);
        QVERIFY2(btnObj != nullptr, qPrintable(QStringLiteral("Button %1 must exist").arg(btnName)));
        auto* btnItem = qobject_cast<QQuickItem*>(btnObj);
        QVERIFY2(btnItem != nullptr, qPrintable(QStringLiteral("Button %1 must be a QQuickItem").arg(btnName)));
        auto* contentRowObj = btnObj->findChild<QObject*>(QStringLiteral("contentRow"));
        QVERIFY2(contentRowObj != nullptr, qPrintable(QStringLiteral("contentRow must exist in %1").arg(btnName)));
        auto* contentRowItem = qobject_cast<QQuickItem*>(contentRowObj);
        QVERIFY2(contentRowItem != nullptr, qPrintable(QStringLiteral("contentRow must be a QQuickItem in %1").arg(btnName)));

        const auto btnCenter = btnItem->mapToScene(QPointF{btnItem->width() * 0.5, btnItem->height() * 0.5});
        const auto rowCenter = contentRowItem->mapToScene(QPointF{contentRowItem->width() * 0.5, contentRowItem->height() * 0.5});

        QVERIFY2(std::abs(btnCenter.x() - rowCenter.x()) <= 1.0,
            qPrintable(QStringLiteral("Button %1 contentRow horizontal center diff %2 > 1.0 px")
                .arg(btnName).arg(std::abs(btnCenter.x() - rowCenter.x()))));
        QVERIFY2(std::abs(btnCenter.y() - rowCenter.y()) <= 1.0,
            qPrintable(QStringLiteral("Button %1 contentRow vertical center diff %2 > 1.0 px")
                .arg(btnName).arg(std::abs(btnCenter.y() - rowCenter.y()))));
    }

    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("gui01_1440x900_gold.png"),
        QSize{1440, 900}));
    QVERIFY(QMetaObject::invokeMethod(zoomIn, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY(zoomOut->property("enabled").toBool());
    QVERIFY(fitSource->property("enabled").toBool());

    for (int i = 0; i < 100 && !auditionSelector.processed_available(); ++i) {
        QTest::qWait(10);
    }
    QVERIFY2(auditionSelector.processed_available(), "PROCESSED audition target must be available after EQ preview");
    QVERIFY2(processedTarget->property("enabled").toBool(), "auditionProcessedButton must be enabled when PROCESSED is available");

    const std::array globalTabOrder{
        sourceOpen,
        waveformObject,
        zoomOut,
        continuousZoom,
        zoomIn,
        fitSource,
        preparedTarget,
        processedTarget,
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

    // Verify 44.1 kHz Mono evidence and routing restrictions on 44.1 kHz source
    QVERIFY2(QMetaObject::invokeMethod(eqMenuItem, "triggered"), "Triggering eqMenuItem on 44.1 kHz source must succeed");
    QCoreApplication::processEvents();

    // Explicitly wait for previewStatus to settle to READY on 44.1 kHz mono source
    for (int i = 0; i < 100 && eqViewModel.preview_status() != QStringLiteral("READY"); ++i) {
        QTest::qWait(10);
    }
    if (eqViewModel.preview_status() == QStringLiteral("ERROR")) {
        QFAIL(qPrintable(QStringLiteral("EQ preview failed: ") + eqViewModel.preview_error()));
    }
    QCOMPARE(eqViewModel.preview_status(), QStringLiteral("READY"));
    QVERIFY2(eqViewModel.preview_error().isEmpty(), "previewError must be empty for 44.1 kHz mono source");

    auto* eqEditor441 = eqToolWindow->findChild<QObject*>(QStringLiteral("parametricEqEditor"));
    QVERIFY2(eqEditor441 != nullptr, "parametricEqEditor must exist on 44.1 kHz source");
    auto* eqGraph441 = eqEditor441->findChild<QObject*>(QStringLiteral("parametricEqGraph"));
    QVERIFY2(eqGraph441 != nullptr, "parametricEqGraph must exist on 44.1 kHz source");
    QCOMPARE(eqGraph441->property("maxFreq").toDouble(), 19845.0);
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_wow_graph_44100_end.png"), QSize{1040, 660}));

    // Capture 900x580 mono evidence
    QVERIFY(capture_visual_evidence(eqWindowObj, QStringLiteral("m12b_wow_900x580_mono.png"), QSize{900, 580}));

    // Verify mono routing restrictions
    auto* routeStereoMono = find_child_by_name(eqEditor441, QStringLiteral("routingButton_STEREO"));
    auto* routeMidMono = find_child_by_name(eqEditor441, QStringLiteral("routingButton_MID"));
    auto* routeSideMono = find_child_by_name(eqEditor441, QStringLiteral("routingButton_SIDE"));
    auto* routeLeftMono = find_child_by_name(eqEditor441, QStringLiteral("routingButton_LEFT"));
    auto* routeRightMono = find_child_by_name(eqEditor441, QStringLiteral("routingButton_RIGHT"));
    QVERIFY2(routeStereoMono && routeStereoMono->property("enabled").toBool(), "STEREO routing must be enabled on mono source");
    QVERIFY2(routeMidMono && !routeMidMono->property("enabled").toBool(), "MID routing must be disabled on mono source");
    QVERIFY2(routeSideMono && !routeSideMono->property("enabled").toBool(), "SIDE routing must be disabled on mono source");
    QVERIFY2(routeLeftMono && !routeLeftMono->property("enabled").toBool(), "LEFT routing must be disabled on mono source");
    QVERIFY2(routeRightMono && !routeRightMono->property("enabled").toBool(), "RIGHT routing must be disabled on mono source");

    // Test Blocker C EQ tool window lifetime: Close EQ alone -> EQ hidden, Main remains open
    eqWindowObj->close();
    QCoreApplication::processEvents();
    QVERIFY2(!eqToolWindow->property("visible").toBool(), "EQ tool window must be hidden after closing EQ alone");
    QVERIFY2(!eqToolWindow->property("forceClose").toBool(), "EQ tool window forceClose must remain false when EQ is closed alone");
    QVERIFY2(window->isVisible(), "Main application window must remain visible after closing EQ tool window alone");

    // Reopen EQ window and verify it is visible
    QVERIFY2(QMetaObject::invokeMethod(eqMenuItem, "triggered"), "Re-triggering View -> Parametric EQ must reopen tool window");
    QCoreApplication::processEvents();
    QVERIFY2(eqToolWindow->property("visible").toBool(), "EQ tool window must be visible after reopening");

    const std::array cornerNames{
        QStringLiteral("resizeTopLeft"),
        QStringLiteral("resizeTopRight"),
        QStringLiteral("resizeBottomLeft"),
        QStringLiteral("resizeBottomRight"),
    };
    for (const auto& cornerName : cornerNames) {
        auto* cornerObj = root->findChild<QObject*>(cornerName);
        QVERIFY2(cornerObj != nullptr, qPrintable(QStringLiteral("Corner %1 must exist").arg(cornerName)));
        auto* cornerItem = qobject_cast<QQuickItem*>(cornerObj);
        QVERIFY2(cornerItem != nullptr, qPrintable(QStringLiteral("Corner %1 must be a QQuickItem").arg(cornerName)));
        QCOMPARE(cornerItem->property("width").toInt(), 8);
        QCOMPARE(cornerItem->property("height").toInt(), 8);
    }

    // Maximize / Restore geometry preservation test (screen-aware)
    window->showNormal();
    const QRect available = window->screen() ? window->screen()->availableGeometry() : QRect{0, 0, 1440, 900};
    const int targetWidth = std::max(window->minimumWidth(), std::min(1280, available.width()));
    const int targetHeight = std::max(window->minimumHeight(), std::min(720, available.height()));
    window->resize(targetWidth, targetHeight);
    QTest::qWait(200);
    QCoreApplication::processEvents();

    const QSize acceptedNormalSize = window->size();
    QVERIFY(acceptedNormalSize.width() >= window->minimumWidth());
    QVERIFY(acceptedNormalSize.height() >= window->minimumHeight());

    QTRY_COMPARE_WITH_TIMEOUT(root->property("normalGeometry").toRect().width(), acceptedNormalSize.width(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("normalGeometry").toRect().height(), acceptedNormalSize.height(), 2000);

#ifdef _WIN32
    // Native Windows Window Chrome Helper Hit-Test Exclusions & Window Styles Assertion
    auto* rootQuickWindow = qobject_cast<QQuickWindow*>(window);
    QVERIFY(rootQuickWindow != nullptr);

    rgsml::app::WindowsWindowChromeHelper testChromeHelper{rootQuickWindow};

    const HWND rootHwnd = reinterpret_cast<HWND>(rootQuickWindow->winId());
    QVERIFY(rootHwnd != nullptr);
    const LONG rootStyle = GetWindowLongW(rootHwnd, GWL_STYLE);
    QVERIFY2((rootStyle & WS_THICKFRAME) != 0, "rootQuickWindow must have WS_THICKFRAME style flag");
    QVERIFY2((rootStyle & WS_MAXIMIZEBOX) != 0, "rootQuickWindow must have WS_MAXIMIZEBOX style flag");
    const std::array chromeExclusionNames{
        QStringLiteral("desktopMenuBar"),
        QStringLiteral("headerAuditionTargetSelector"),
        QStringLiteral("windowMinimizeButton"),
        QStringLiteral("windowMaximizeButton"),
        QStringLiteral("windowCloseButton"),
    };
    for (const auto& name : chromeExclusionNames) {
        auto* item = rootQuickWindow->findChild<QQuickItem*>(name);
        QVERIFY2(item != nullptr, qPrintable(QStringLiteral("Exclusion item %1 must exist").arg(name)));
        testChromeHelper.add_exclusion_item(item);
    }

    MSG testMsg{};
    testMsg.hwnd = reinterpret_cast<HWND>(rootQuickWindow->winId());
    testMsg.message = WM_NCHITTEST;

    // Dynamically scan candidate points across header (y=20) outside all exclusions
    std::vector<QQuickItem*> exclusionItems;
    for (const auto& name : chromeExclusionNames) {
        if (auto* item = rootQuickWindow->findChild<QQuickItem*>(name)) {
            exclusionItems.push_back(item);
        }
    }

    QPoint validDraggablePt{-1, -1};
    const int winW = rootQuickWindow->width();
    for (int candX = 10; candX <= winW - 10; candX += 10) {
        const QPoint localPt{candX, 20};
        const QPoint globalPt = rootQuickWindow->mapToGlobal(localPt);
        bool insideExclusion = false;
        for (auto* exclItem : exclusionItems) {
            if (exclItem != nullptr && exclItem->isVisible() && exclItem->isEnabled()) {
                const QPointF itemLocal = exclItem->mapFromGlobal(globalPt);
                if (itemLocal.x() >= 0 && itemLocal.x() < exclItem->width()
                    && itemLocal.y() >= 0 && itemLocal.y() < exclItem->height()) {
                    insideExclusion = true;
                    break;
                }
            }
        }
        if (!insideExclusion) {
            validDraggablePt = globalPt;
            break;
        }
    }

    QVERIFY2(validDraggablePt.x() >= 0, "A valid non-interactive draggable header test point must exist");
    testMsg.lParam = MAKELPARAM(validDraggablePt.x(), validDraggablePt.y());
    qintptr hitResult = 0;
    // WM_NCHITTEST returns false so QML headerMouseArea startSystemMove() handles main window move authority
    QVERIFY(!testChromeHelper.nativeEventFilter("windows_generic_MSG", &testMsg, &hitResult));

    // Test a point over windowCloseButton (exclusion item)
    auto* closeBtnItem = rootQuickWindow->findChild<QQuickItem*>(QStringLiteral("windowCloseButton"));
    QVERIFY(closeBtnItem != nullptr);
    const QPoint closeGlobalPt = closeBtnItem->mapToGlobal(QPointF{closeBtnItem->width() * 0.5, closeBtnItem->height() * 0.5}).toPoint();
    testMsg.lParam = MAKELPARAM(closeGlobalPt.x(), closeGlobalPt.y());
    hitResult = 0;
    QVERIFY(!testChromeHelper.nativeEventFilter("windows_generic_MSG", &testMsg, &hitResult));
#endif

    auto* windowMaximizeBtn = root->findChild<QObject*>(QStringLiteral("windowMaximizeButton"));
    QVERIFY2(windowMaximizeBtn != nullptr, "windowMaximizeButton must exist");

    // Click maximize
    QVERIFY(QMetaObject::invokeMethod(windowMaximizeBtn, "clicked"));
    QTest::qWait(100);
    QCoreApplication::processEvents();
    QCOMPARE(window->visibility(), QWindow::Maximized);

    // Click restore
    QVERIFY(QMetaObject::invokeMethod(windowMaximizeBtn, "clicked"));
    QTest::qWait(100);
    QCoreApplication::processEvents();
    QVERIFY(window->visibility() != QWindow::Maximized);
    QCOMPARE(window->size(), acceptedNormalSize);

    window->resize(1184, 688);
    QCoreApplication::processEvents();
    QCOMPARE(window->size(), QSize(1184, 688));
    const auto minimumWaveformHeight = waveformPanel->property("height").toReal();
    QVERIFY(minimumWaveformHeight >= 265.0);

    const int largeWidth = std::max(window->minimumWidth(), std::min(1440, available.width()));
    const int largeHeight = std::max(window->minimumHeight(), std::min(900, available.height()));
    window->resize(largeWidth, largeHeight);
    QTest::qWait(50);
    QCoreApplication::processEvents();

    const QSize acceptedLargeSize = window->size();
    QVERIFY(acceptedLargeSize.width() >= window->minimumWidth());
    QVERIFY(acceptedLargeSize.height() >= window->minimumHeight());

    if (acceptedLargeSize.height() > window->minimumHeight()) {
        QVERIFY(waveformPanel->property("height").toReal() > minimumWaveformHeight);
    } else {
        QVERIFY(waveformPanel->property("height").toReal() >= 265.0);
    }

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
