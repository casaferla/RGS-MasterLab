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

struct LayoutEvalMetrics {
    int width{0};
    int height{0};
    bool visible{false};
    bool isCompact{false};
    double sourceHeight{0.0};
    double waveformHeight{0.0};
    double controlHeight{0.0};
    double regionHeight{0.0};
    double workspaceHeight{0.0};
    double hostHeight{0.0};
};

struct LayoutEvalResult {
    bool valid{false};
    QString errorMessage;
    LayoutEvalMetrics metrics{};
};

[[nodiscard]] LayoutEvalResult evaluate_layout_at_size(
    QQmlEngine& engine,
    int logicalWidth,
    int logicalHeight)
{
    QQmlComponent component{&engine, QUrl{QStringLiteral("qrc:/qt/qml/Rgsml/Ui/qml/Main.qml")}};
    if (component.status() == QQmlComponent::Error) {
        return LayoutEvalResult{
            .valid = false,
            .errorMessage = QStringLiteral("Main.qml component error: ") + component.errorString(),
            .metrics = {}
        };
    }

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("visible"), false);
    initialProperties.insert(QStringLiteral("width"), logicalWidth);
    initialProperties.insert(QStringLiteral("height"), logicalHeight);

    QScopedPointer<QObject> obj{component.createWithInitialProperties(initialProperties)};
    if (!obj) {
        return LayoutEvalResult{
            .valid = false,
            .errorMessage = QStringLiteral("Main.qml component creation returned null: ") + component.errorString(),
            .metrics = {}
        };
    }

    auto* qwin = qobject_cast<QQuickWindow*>(obj.get());
    if (!qwin) {
        return LayoutEvalResult{
            .valid = false,
            .errorMessage = QStringLiteral("Created root object is not a QQuickWindow"),
            .metrics = {}
        };
    }

    QCoreApplication::processEvents();

    auto* workspace = obj->findChild<QObject*>(QStringLiteral("dspWorkspace"));
    auto* host = obj->findChild<QObject*>(QStringLiteral("dspEditorHost"));
    auto* waveform = obj->findChild<QObject*>(QStringLiteral("sourceWaveformPanel"));
    auto* source = obj->findChild<QObject*>(QStringLiteral("sourceMetadataPanel"));
    auto* control = obj->findChild<QObject*>(QStringLiteral("controlStrip"));
    auto* region = obj->findChild<QObject*>(QStringLiteral("auditionRegionControls"));

    if (!workspace || !host || !waveform || !source || !control || !region) {
        return LayoutEvalResult{
            .valid = false,
            .errorMessage = QStringLiteral("One or more required Main child components not found in offscreen harness"),
            .metrics = {}
        };
    }

    LayoutEvalMetrics metrics;
    metrics.width = qwin->property("width").toInt();
    metrics.height = qwin->property("height").toInt();
    metrics.visible = qwin->property("visible").toBool();
    metrics.isCompact = obj->property("isCompactLayout").toBool();
    metrics.sourceHeight = source->property("height").toDouble();
    metrics.waveformHeight = waveform->property("height").toDouble();
    metrics.controlHeight = control->property("height").toDouble();
    metrics.regionHeight = region->property("height").toDouble();
    metrics.workspaceHeight = workspace->property("height").toDouble();
    metrics.hostHeight = host->property("height").toDouble();

    return LayoutEvalResult{
        .valid = true,
        .errorMessage = {},
        .metrics = metrics
    };
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
            QVERIFY(eqViewModel.is_default());
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

    // Verify Docked DSP Architecture components exist
    auto* dspWorkspaceObj = root->findChild<QObject*>(QStringLiteral("dspWorkspace"));
    auto* dspChainSelectorObj = root->findChild<QObject*>(QStringLiteral("dspChainSelector"));
    auto* dspEditorHostObj = root->findChild<QObject*>(QStringLiteral("dspEditorHost"));
    QVERIFY2(dspWorkspaceObj != nullptr, "dspWorkspace must exist in Main");
    QVERIFY2(dspChainSelectorObj != nullptr, "dspChainSelector must exist in dspWorkspace");
    QVERIFY2(dspEditorHostObj != nullptr, "dspEditorHost must exist in dspWorkspace");

    // Verify no production standalone EQ tool window exists
    QVERIFY2(root->findChild<QObject*>(QStringLiteral("parametricEqToolWindow")) == nullptr,
        "No production ParametricEqEditorWindow instance must exist in Main");

    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("gui01_1184x688_unavailable.png"),
        QSize{1184, 688}));
    auto* sourceOpen = root->findChild<QObject*>(QStringLiteral("sourceOpenButton"));
    QVERIFY(sourceOpen);
    auto* sourcePanel = root->findChild<QObject*>(QStringLiteral("sourceMetadataPanel"));
    QVERIFY(sourcePanel);
    QCOMPARE(sourceOpen->property("height").toInt(), 32);
    QCOMPARE(sourceOpen->property("width").toInt(), 110);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceFileDialog")));
    QVERIFY2((window->flags() & Qt::FramelessWindowHint) == 0, "Main window must NOT be frameless (native windowing model)");
    const auto itemCenter = [](QObject* object) {
        auto* item = qobject_cast<QQuickItem*>(object);
        Q_ASSERT(item != nullptr);
        return item->mapToScene(QPointF{item->width() * 0.5, item->height() * 0.5}).toPoint();
    };
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("gui01_c1_caption_normal.png"), QSize{1440, 900}));
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

    // Exercise View Menu real mouse interaction when Source is loaded: selects/reveals docked EQ
    qInfo().noquote() << "M12C_SMOKE_PHASE=view-menu-selects-docked-eq";
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

    auto* realEqMenuItem = root->findChild<QObject*>(QStringLiteral("menuViewParametricEq"));
    auto* realEqMenuQuickItem = qobject_cast<QQuickItem*>(realEqMenuItem);
    QVERIFY(realEqMenuItem && realEqMenuQuickItem);
    QVERIFY(realEqMenuItem->property("visible").toBool());
    QVERIFY(realEqMenuItem->property("enabled").toBool());

    const auto eqMenuCenter = realEqMenuQuickItem->mapToScene(QPointF{
        realEqMenuQuickItem->width() / 2, realEqMenuQuickItem->height() / 2});
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, eqMenuCenter.toPoint());
    QTest::qWait(120);
    QCoreApplication::processEvents();

    QCOMPARE(dspWorkspaceObj->property("selectedModuleIndex").toInt(), 0);

    qInfo().noquote() << "M12C_SMOKE_PHASE=editor-lookup";
    auto* eqEditor = dspEditorHostObj->findChild<QObject*>(QStringLiteral("parametricEqEditor"));
    QVERIFY2(eqEditor != nullptr, "parametricEqEditor must exist inside dspEditorHost");
    auto* eqGraph = eqEditor->findChild<QObject*>(QStringLiteral("parametricEqGraph"));
    QVERIFY2(eqGraph != nullptr, "parametricEqGraph must exist inside eqEditor");
    QVERIFY2(!eqViewModel.selected_band_response_points().isEmpty(), "eqViewModel response points must not be empty");
    QCOMPARE(eqGraph->property("maxFreq").toDouble(), 19845.0); // 44.1 kHz UI Source.wav loaded endpoint (0.45 * 44100 = 19845 Hz)
    QVERIFY2(eqEditor->findChild<QObject*>(QStringLiteral("spectrumAnalyzer")) == nullptr, "No fake analyzer or spectrum item must exist");

    qInfo().noquote() << "M12C_SMOKE_PHASE=dsp-chain-row-led-and-byp";
    auto* dspChainRow0 = root->findChild<QObject*>(QStringLiteral("dspChainRow_0"));
    auto* dspChainConfigLed0 = root->findChild<QObject*>(QStringLiteral("dspChainConfigLed_0"));
    auto* dspChainBypassBadge0 = root->findChild<QObject*>(QStringLiteral("dspChainBypassBadge_0"));
    QVERIFY2(dspChainRow0 != nullptr && dspChainConfigLed0 != nullptr, "DSP chain row and config LED must exist");
    QVERIFY2(dspChainBypassBadge0 != nullptr, "DSP chain bypass badge object must exist");

    // Canonical Flat state -> config LED is dark (#273A4D), BYP badge is hidden
    QVERIFY2(eqViewModel.is_default(), "EQ state must be default/flat initially");
    QCOMPARE(dspChainConfigLed0->property("color").value<QColor>(), QColor{QStringLiteral("#273A4D")});
    QVERIFY2(!dspChainBypassBadge0->property("visible").toBool(), "BYP badge must be hidden initially");

    qInfo().noquote() << "M12C_SMOKE_PHASE=band-controls";
    auto* band0Btn = find_child_by_name(eqEditor, QStringLiteral("bandSelectorButton_0"));
    QVERIFY2(band0Btn != nullptr, "bandSelectorButton_0 must exist");
    QVERIFY2(band0Btn->property("activeFocusOnTab").toBool(), "Band selector button must be Tab focusable");

    auto* addBandBtn = eqEditor->findChild<QObject*>(QStringLiteral("addBandButton"));
    auto* removeBandBtn = eqEditor->findChild<QObject*>(QStringLiteral("removeBandButton"));
    QVERIFY2(addBandBtn != nullptr && removeBandBtn != nullptr, "Add and Remove band buttons must exist");
    QCOMPARE(eqViewModel.band_count(), 1);
    QVERIFY2(!eqViewModel.can_undo(), "can_undo must be false initially for canonical Flat EQ");
    QVERIFY2(!eqViewModel.can_redo(), "can_redo must be false initially for canonical Flat EQ");

    // Add band -> state becomes Manual non-default -> config LED turns Green #00D47A
    QVERIFY2(QMetaObject::invokeMethod(addBandBtn, "clicked"), "Clicking addBandButton must succeed");
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.band_count(), 2);
    QCOMPARE(eqViewModel.selected_index(), 1);
    QVERIFY2(!eqViewModel.is_default(), "EQ state must no longer be default after adding a band");
    QCOMPARE(dspChainConfigLed0->property("color").value<QColor>(), QColor{QStringLiteral("#00D47A")});

    // Keyboard Space activation on bandSelectorButton_0
    auto* band0BtnCurrent = find_child_by_name(eqEditor, QStringLiteral("bandSelectorButton_0"));
    QVERIFY2(band0BtnCurrent != nullptr, "bandSelectorButton_0 must exist after band addition");
    auto* band0Item = qobject_cast<QQuickItem*>(band0BtnCurrent);
    QVERIFY2(band0Item != nullptr, "bandSelectorButton_0 must be a QQuickItem");
    window->requestActivate();
    QTest::qWait(50);
    QCoreApplication::processEvents();
    band0Item->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY2(band0Item->hasActiveFocus(), "bandSelectorButton_0 must have active focus");

    const quint64 genBeforeKeyboardSelect = eqViewModel.preview_generation();
    QTest::keyClick(window, Qt::Key_Space);
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.selected_index(), 0);
    QCOMPARE(eqViewModel.preview_generation(), genBeforeKeyboardSelect);

    // Filter-specific control visibility & slope commit
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

    // Reset filter to BELL
    QVERIFY2(QMetaObject::invokeMethod(filterBell, "clicked"), "Resetting filter to BELL must succeed");
    QCoreApplication::processEvents();

    qInfo().noquote() << "M12C_SMOKE_PHASE=invalid-draft";
    auto* freqInput = find_child_by_name(eqEditor, QStringLiteral("frequencyInput"));
    QVERIFY2(freqInput != nullptr, "frequencyInput control must exist");
    auto* freqInputItem = qobject_cast<QQuickItem*>(freqInput);
    QVERIFY2(freqInputItem != nullptr, "frequencyInput must be a QQuickItem");
    window->requestActivate();
    QTest::qWait(50);
    QCoreApplication::processEvents();
    freqInputItem->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY2(freqInputItem->hasActiveFocus(), "frequencyInput must receive active focus");

    QMetaObject::invokeMethod(freqInput, "selectAll");
    for (const char c : std::string_view{"99999"}) {
        QTest::keyClick(window, c);
    }
    QCoreApplication::processEvents();

    QCOMPARE(freqInput->property("text").toString(), QStringLiteral("99999"));
    QCOMPARE(eqViewModel.validation_field(), QStringLiteral("frequency"));
    auto* valMsgText = find_child_by_name(eqEditor, QStringLiteral("validationMessageText"));
    QVERIFY2(valMsgText && valMsgText->property("visible").toBool(), "Validation message text must be visible for invalid draft");

    QTest::keyClick(window, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QCOMPARE(freqInput->property("text").toString(), QStringLiteral("1000"));
    QCOMPARE(eqViewModel.frequency_text(), QStringLiteral("1000"));
    QVERIFY2(eqViewModel.validation_field().isEmpty(), "Validation field must be empty after Escape key cancel");

    qInfo().noquote() << "M12C_SMOKE_PHASE=ab-and-byp-semantics";
    auto* abBypassBtn = dspEditorHostObj->findChild<QObject*>(QStringLiteral("abButtonBypass"));
    auto* abActiveBtn = dspEditorHostObj->findChild<QObject*>(QStringLiteral("abButtonActive"));
    auto* undoBtn = dspEditorHostObj->findChild<QObject*>(QStringLiteral("eqUndoButton"));
    auto* redoBtn = dspEditorHostObj->findChild<QObject*>(QStringLiteral("eqRedoButton"));
    auto* resetFlatBtn = eqEditor->findChild<QObject*>(QStringLiteral("eqResetFlatButton"));
    auto* overallToggleBtn = eqEditor->findChild<QObject*>(QStringLiteral("eqOverallToggleButton"));

    QVERIFY2(abBypassBtn != nullptr && abActiveBtn != nullptr, "A and B buttons must exist in host header");
    QVERIFY2(undoBtn != nullptr && redoBtn != nullptr && resetFlatBtn != nullptr, "Undo, Redo, and Reset Flat buttons must exist");
    QVERIFY2(overallToggleBtn != nullptr, "Overall toggle button must exist");

    // Click Bypass -> BYP badge becomes visible on DSP chain row, while config LED remains unchanged
    QVERIFY2(QMetaObject::invokeMethod(abBypassBtn, "clicked"), "Clicking abButtonBypass must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqViewModel.bypass(), "eqViewModel.bypass must be true after clicking Bypass");
    QVERIFY2(dspChainBypassBadge0->property("visible").toBool(), "dspChainBypassBadge_0 must be visible when bypassed");

    QVERIFY2(QMetaObject::invokeMethod(abActiveBtn, "clicked"), "Clicking abButtonActive must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(!eqViewModel.bypass(), "eqViewModel.bypass must be false after clicking Active");
    QVERIFY2(!dspChainBypassBadge0->property("visible").toBool(), "dspChainBypassBadge_0 must be hidden when active");

    // Reset Flat -> restores Default status -> LED turns dark #273A4D
    QVERIFY2(QMetaObject::invokeMethod(resetFlatBtn, "clicked"), "Clicking eqResetFlatButton must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqViewModel.is_default(), "EQ state must be default/flat after Reset Flat");
    QCOMPARE(dspChainConfigLed0->property("color").value<QColor>(), QColor{QStringLiteral("#273A4D")});

    // Undo -> restores 2-band non-default state -> LED turns Green #00D47A
    QVERIFY2(QMetaObject::invokeMethod(undoBtn, "clicked"), "Clicking eqUndoButton must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(!eqViewModel.is_default(), "EQ state must be non-default after Undo");
    QCOMPARE(dspChainConfigLed0->property("color").value<QColor>(), QColor{QStringLiteral("#00D47A")});

    qInfo().noquote() << "M12C_SMOKE_PHASE=stereo-mixed-routing-and-band-colors";
    const auto visualSourceBytes = visual_wav();
    const auto visualSourcePath = write_file(
        directory, QStringLiteral("GUI-01 Visual Source.wav"), visualSourceBytes);
    QVERIFY(!visualSourcePath.isEmpty());
    model.selectSource(QUrl::fromLocalFile(visualSourcePath));
    waveformPresentation.publish_ready(summary_from_wav(visualSourceBytes));
    QCoreApplication::processEvents();

    // Add bands until 6 exist
    while (eqViewModel.band_count() < 6) {
        QVERIFY(QMetaObject::invokeMethod(addBandBtn, "clicked"));
        QCoreApplication::processEvents();
    }
    QCOMPARE(eqViewModel.band_count(), 6);

    // Verify 48 kHz visual_wav() source maxFreq endpoint (0.45 * 48000 = 21600 Hz, capped at 20000 Hz)
    QCOMPARE(eqGraph->property("maxFreq").toDouble(), 20000.0);

    // Verify 6 Band selector accent colors
    const std::array expectedColors{
        QColor{QStringLiteral("#2ED3FF")}, // 1 Cyan
        QColor{QStringLiteral("#2FD98F")}, // 2 Emerald
        QColor{QStringLiteral("#FFD84A")}, // 3 Warm Yellow
        QColor{QStringLiteral("#FF6B6B")}, // 4 Coral Red
        QColor{QStringLiteral("#4F7CFF")}, // 5 Cobalt Blue
        QColor{QStringLiteral("#F5F8FC")}  // 6 Neutral White
    };

    for (std::size_t idx = 0; idx < expectedColors.size(); ++idx) {
        auto* bandBtn = find_child_by_name(eqEditor, QString("bandSelectorButton_%1").arg(idx));
        QVERIFY2(bandBtn != nullptr, qPrintable(QString("bandSelectorButton_%1 must exist").arg(idx)));
        QCOMPARE(bandBtn->property("accentColor").value<QColor>(), expectedColors[idx]);
    }

    // Capture visual evidence image for 6 band colors + handles + Overall lavender
    QVERIFY(capture_visual_evidence(window, QStringLiteral("m12c_b1_band_colors.png"), QSize{1440, 900}));

    // Verify Mixed routing on stereo source
    auto* routeMidBtnStereo = find_child_by_name(eqEditor, QStringLiteral("routingButton_MID"));
    QVERIFY2(routeMidBtnStereo != nullptr && routeMidBtnStereo->property("enabled").toBool(), "routingButton_MID must be enabled on stereo source");
    QVERIFY2(QMetaObject::invokeMethod(routeMidBtnStereo, "clicked"), "Clicking routingButton_MID must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqViewModel.mixed_routing(), "eqViewModel.mixedRouting must be true after setting band 2 to MID");
    auto* mixedIndicatorText = find_child_by_name(eqEditor, QStringLiteral("mixedText"));
    QVERIFY2(mixedIndicatorText != nullptr && mixedIndicatorText->property("visible").toBool(), "MIXED ROUTING ACTIVE indicator must be visible");

    // Restore band 5 routing to STEREO
    eqViewModel.selectBand(5);
    QCoreApplication::processEvents();
    auto* routeStereoBtnStereo = find_child_by_name(eqEditor, QStringLiteral("routingButton_STEREO"));
    QVERIFY2(routeStereoBtnStereo != nullptr && QMetaObject::invokeMethod(routeStereoBtnStereo, "clicked"), "Clicking routingButton_STEREO must succeed");
    QCoreApplication::processEvents();

    // Verify Docked EQ at 1440x900 and 1184x688 reference compositions
    qInfo().noquote() << "M12C_SMOKE_PHASE=responsive-composition";

    // 1. Offscreen QML layout harness to evaluate exact logical compositions at 1440x900 and 1184x688
    // independent of CI physical HyperVMonitor screen clamping.
    const auto res1440 = evaluate_layout_at_size(engine, 1440, 900);
    QVERIFY2(res1440.valid, qPrintable(res1440.errorMessage));
    QCOMPARE(res1440.metrics.visible, false);
    QCOMPARE(res1440.metrics.width, 1440);
    QCOMPARE(res1440.metrics.height, 900);
    QCOMPARE(res1440.metrics.isCompact, false);
    QCOMPARE(res1440.metrics.sourceHeight, 72.0);
    QVERIFY2(res1440.metrics.waveformHeight >= 180.0, "Waveform height must be >= 180 px at 1440x900");
    QCOMPARE(res1440.metrics.controlHeight, 72.0);
    QCOMPARE(res1440.metrics.regionHeight, 72.0);
    QVERIFY2(res1440.metrics.hostHeight >= 400.0, "dspEditorHost must be dominant (>= 400 px) at 1440x900");

    const auto res1184 = evaluate_layout_at_size(engine, 1184, 688);
    QVERIFY2(res1184.valid, qPrintable(res1184.errorMessage));
    QCOMPARE(res1184.metrics.visible, false);
    QCOMPARE(res1184.metrics.width, 1184);
    QCOMPARE(res1184.metrics.height, 688);
    QCOMPARE(res1184.metrics.isCompact, true);
    QCOMPARE(res1184.metrics.sourceHeight, 48.0);
    QVERIFY2(res1184.metrics.waveformHeight >= 96.0, "Waveform height must be >= 96 px at 1184x688");
    QCOMPARE(res1184.metrics.controlHeight, 56.0);
    QCOMPARE(res1184.metrics.regionHeight, 56.0);
    QVERIFY2(res1184.metrics.workspaceHeight >= 300.0, "dspWorkspace must receive min 300 px height at 1184x688");

    // 2. Native window resize & visual evidence capture clamped to available monitor geometry
    const QRect available = window->screen() ? window->screen()->availableGeometry() : QRect{0, 0, 1440, 900};
    const int normalW = std::max(window->minimumWidth(), std::min(1440, available.width()));
    const int normalH = std::max(window->minimumHeight(), std::min(900, available.height()));
    window->resize(normalW, normalH);
    QTest::qWait(50);
    QCoreApplication::processEvents();
    QCOMPARE(window->size(), QSize(normalW, normalH));
    QVERIFY(capture_visual_evidence(window, QStringLiteral("gui01_1440x900_prepared.png"), QSize{normalW, normalH}));

    const int compactW = std::max(window->minimumWidth(), std::min(1184, available.width()));
    const int compactH = std::max(window->minimumHeight(), std::min(688, available.height()));
    window->resize(compactW, compactH);
    QTest::qWait(50);
    QCoreApplication::processEvents();
    QCOMPARE(window->size(), QSize(compactW, compactH));
    QVERIFY(capture_visual_evidence(window, QStringLiteral("gui01_1184x688_prepared.png"), QSize{compactW, compactH}));

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
