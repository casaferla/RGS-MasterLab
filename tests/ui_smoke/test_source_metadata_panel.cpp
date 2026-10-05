#include "audition_region_view_model.hpp"
#include "audition_source_selector.hpp"
#include "compressor_view_model.hpp"
#include "dsp_chain_adapter_model.hpp"
#include "eq_view_model.hpp"
#include "gain_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "live_spectrum_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"
#include "playback_transport_view_model.hpp"
#include <rgsml/analysis/live_spectrum_analyzer.hpp>
#include <rgsml/dsp/module_registry.hpp>
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

[[maybe_unused]] [[nodiscard]] std::shared_ptr<const audio::WaveformSummary> valid_summary()
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

[[nodiscard]] bool check_item_contained_in_ancestor(
    QQuickItem* childItem,
    QQuickItem* ancestorItem,
    double tolerance = 0.5)
{
    if (childItem == nullptr || ancestorItem == nullptr) {
        return false;
    }
    const QPointF childTopLeft = childItem->mapToItem(ancestorItem, QPointF{0.0, 0.0});
    const double right = childTopLeft.x() + childItem->width();
    const double bottom = childTopLeft.y() + childItem->height();
    return childTopLeft.x() >= -tolerance
        && right <= ancestorItem->width() + tolerance
        && childTopLeft.y() >= -tolerance
        && bottom <= ancestorItem->height() + tolerance;
}

struct LayoutEvalMetrics {
    int width{0};
    int height{0};
    bool visible{false};
    bool adaptiveContextVisible{false};
    bool compactBottomRightContained{false};
    bool hostContainedInCompactBottomRight{false};
    bool eqEditorContainedInHost{false};
    bool gainEditorContainedInHost{false};
    bool eqInspectorContained{false};
    bool eqHeaderControlsContained{false};
    bool eqFilterButtonsContained{false};
    bool eqRoutingButtonsContained{false};
    bool eqNumericFieldsContained{false};
    bool eqSlopeControlsContained{false};
    bool gainControlsContained{false};
    bool dspChainVisible{false};
    bool manualEditVisible{false};
    double eqEditorHeight{0.0};
    double eqGraphHeight{0.0};
    double eqInspectorY{0.0};
    double eqInspectorHeight{0.0};
    double eqStatusY{0.0};
    double eqStatusHeight{0.0};
    double sourceHeight{0.0};
    double waveformHeight{0.0};
    double controlHeight{0.0};
    double regionHeight{0.0};
    double controlContentLeft{0.0};
    double controlContentRight{0.0};
    double regionContentLeft{0.0};
    double regionContentRight{0.0};
    double regionContentTop{0.0};
    double regionContentBottom{0.0};
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
    auto* controlContent = obj->findChild<QObject*>(QStringLiteral("controlStripContent"));
    auto* regionContent = obj->findChild<QObject*>(QStringLiteral("auditionRegionContent"));
    auto* adaptiveContext = obj->findChild<QObject*>(QStringLiteral("adaptiveContextWorkspace"));
    auto* eqEditor = obj->findChild<QObject*>(QStringLiteral("parametricEqEditor"));
    auto* gainEditor = obj->findChild<QObject*>(QStringLiteral("inputGainEditor"));
    auto* eqGraph = obj->findChild<QObject*>(QStringLiteral("parametricEqGraph"));
    auto* eqInspector = obj->findChild<QObject*>(QStringLiteral("eqInspectorRegion"));
    auto* eqStatus = obj->findChild<QObject*>(QStringLiteral("eqStatusRegion"));
    auto* compactBottomSplit = obj->findChild<QObject*>(QStringLiteral("compactBottomSplit"));
    auto* compactBottomRight = obj->findChild<QObject*>(QStringLiteral("compactBottomRight"));
    auto* chainSelector = obj->findChild<QObject*>(QStringLiteral("dspChainSelector"));
    auto* parametricEqRow = chainSelector
        ? qvariant_cast<QObject*>(chainSelector->property("parametricEqRow"))
        : nullptr;
    auto* manualEditText = parametricEqRow
        ? parametricEqRow->findChild<QObject*>(QStringLiteral("dspChainStateText_1"))
        : nullptr;

    if (!workspace || !host || !waveform || !source || !control || !region
        || !controlContent || !regionContent || !compactBottomSplit || !compactBottomRight
        || !adaptiveContext || !eqEditor || !gainEditor || !eqGraph || !eqInspector || !eqStatus
        || !chainSelector || !parametricEqRow || !manualEditText) {
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
    metrics.adaptiveContextVisible = adaptiveContext->property("visible").toBool();

    auto* compactBottomSplitItem = qobject_cast<QQuickItem*>(compactBottomSplit);
    auto* compactBottomRightItem = qobject_cast<QQuickItem*>(compactBottomRight);
    auto* hostItem = qobject_cast<QQuickItem*>(host);
    auto* eqEditorItem = qobject_cast<QQuickItem*>(eqEditor);
    auto* gainEditorItem = qobject_cast<QQuickItem*>(gainEditor);
    auto* chainSelectorItem = qobject_cast<QQuickItem*>(chainSelector);

    // The offscreen harness keeps the QQuickWindow hidden, so QQuickWindow::contentItem()
    // is not a reliable window-bounds surrogate for LayoutItemProxy geometry. Validate the
    // proxy against its actual authored RowLayout container here; visible-window target
    // containment is verified later against the live rehosted items.
    metrics.compactBottomRightContained =
        check_item_contained_in_ancestor(compactBottomRightItem, compactBottomSplitItem);
    metrics.hostContainedInCompactBottomRight = check_item_contained_in_ancestor(hostItem, compactBottomRightItem);
    metrics.eqEditorContainedInHost = check_item_contained_in_ancestor(eqEditorItem, hostItem);
    metrics.gainEditorContainedInHost = check_item_contained_in_ancestor(gainEditorItem, hostItem);
    metrics.dspChainVisible = chainSelectorItem != nullptr && chainSelectorItem->isVisible();
    metrics.manualEditVisible = manualEditText->property("visible").toBool();

    if (eqEditorItem != nullptr && hostItem != nullptr) {
        metrics.eqEditorHeight = eqEditorItem->height();

        if (auto* graphItem = qobject_cast<QQuickItem*>(eqGraph); graphItem != nullptr) {
            metrics.eqGraphHeight = graphItem->height();
        }

        if (auto* inspectorItem = qobject_cast<QQuickItem*>(eqInspector); inspectorItem != nullptr) {
            const QPointF inspectorTopLeft = inspectorItem->mapToItem(eqEditorItem, QPointF{0.0, 0.0});
            metrics.eqInspectorY = inspectorTopLeft.y();
            metrics.eqInspectorHeight = inspectorItem->height();
            metrics.eqInspectorContained = check_item_contained_in_ancestor(inspectorItem, eqEditorItem);
        }

        if (auto* statusItem = qobject_cast<QQuickItem*>(eqStatus); statusItem != nullptr) {
            const QPointF statusTopLeft = statusItem->mapToItem(eqEditorItem, QPointF{0.0, 0.0});
            metrics.eqStatusY = statusTopLeft.y();
            metrics.eqStatusHeight = statusItem->height();
        }

        // Header / Action Controls
        auto* abActive = host->findChild<QObject*>(QStringLiteral("abButtonActive"));
        auto* abBypass = host->findChild<QObject*>(QStringLiteral("abButtonBypass"));
        auto* undoBtn = host->findChild<QObject*>(QStringLiteral("eqUndoButton"));
        auto* redoBtn = host->findChild<QObject*>(QStringLiteral("eqRedoButton"));
        metrics.eqHeaderControlsContained =
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(abActive), hostItem) &&
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(abBypass), hostItem) &&
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(undoBtn), hostItem) &&
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(redoBtn), hostItem);

        // Filter Buttons
        bool filterButtonsOk = true;
        for (const auto& token : {QStringLiteral("BELL"), QStringLiteral("NOTCH"), QStringLiteral("LOW_SHELF"),
                                  QStringLiteral("HIGH_SHELF"), QStringLiteral("HIGH_PASS"), QStringLiteral("LOW_PASS")}) {
            auto* filterBtn = find_child_by_name(eqEditor, QStringLiteral("filterButton_") + token);
            if (!check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(filterBtn), eqEditorItem)) {
                filterButtonsOk = false;
                break;
            }
        }
        metrics.eqFilterButtonsContained = filterButtonsOk;

        // Routing Buttons & Mixed Routing Badge
        bool routingButtonsOk = true;
        for (const auto& token : {QStringLiteral("STEREO"), QStringLiteral("MID"), QStringLiteral("SIDE"),
                                  QStringLiteral("LEFT"), QStringLiteral("RIGHT")}) {
            auto* routingBtn = find_child_by_name(eqEditor, QStringLiteral("routingButton_") + token);
            if (!check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(routingBtn), eqEditorItem)) {
                routingButtonsOk = false;
                break;
            }
        }
        auto* mixedBadge = eqEditor->findChild<QObject*>(QStringLiteral("mixedRoutingBadge"));
        if (mixedBadge && mixedBadge->property("visible").toBool()) {
            if (!check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(mixedBadge), eqEditorItem)) {
                routingButtonsOk = false;
            }
        }
        metrics.eqRoutingButtonsContained = routingButtonsOk;

        // Numeric Fields
        auto* freqField = find_child_by_name(eqEditor, QStringLiteral("frequencyField"));
        auto* gainField = find_child_by_name(eqEditor, QStringLiteral("gainField"));
        auto* qField = find_child_by_name(eqEditor, QStringLiteral("qField"));
        metrics.eqNumericFieldsContained =
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(freqField), eqEditorItem) &&
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(gainField), eqEditorItem) &&
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(qField), eqEditorItem);

        // Slope Controls
        auto* slopeGroup = find_child_by_name(eqEditor, QStringLiteral("slopeControls"));
        metrics.eqSlopeControlsContained = check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(slopeGroup), eqEditorItem);
    }

    if (gainEditorItem != nullptr && hostItem != nullptr) {
        auto* resetGainBtn = gainEditor->findChild<QObject*>(QStringLiteral("resetGainButton"));
        auto* gainDisplay = gainEditor->findChild<QObject*>(QStringLiteral("gainDbDisplay"));
        auto* gainSld = gainEditor->findChild<QObject*>(QStringLiteral("gainSlider"));
        auto* gainIn = gainEditor->findChild<QObject*>(QStringLiteral("gainDbInput"));
        metrics.gainControlsContained =
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(resetGainBtn), gainEditorItem) &&
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(gainDisplay), gainEditorItem) &&
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(gainSld), gainEditorItem) &&
            check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(gainIn), gainEditorItem);
    }

    metrics.sourceHeight = source->property("height").toDouble();
    metrics.waveformHeight = waveform->property("height").toDouble();
    metrics.controlHeight = control->property("height").toDouble();
    metrics.regionHeight = region->property("height").toDouble();

    if (auto* controlItem = qobject_cast<QQuickItem*>(control); controlItem != nullptr) {
        if (auto* contentItem = qobject_cast<QQuickItem*>(controlContent); contentItem != nullptr) {
            metrics.controlContentLeft = contentItem->x();
            metrics.controlContentRight = controlItem->width() - (contentItem->x() + contentItem->width());
        }
    }
    if (auto* regionItem = qobject_cast<QQuickItem*>(region); regionItem != nullptr) {
        if (auto* contentItem = qobject_cast<QQuickItem*>(regionContent); contentItem != nullptr) {
            metrics.regionContentLeft = contentItem->x();
            metrics.regionContentRight = regionItem->width() - (contentItem->x() + contentItem->width());
            metrics.regionContentTop = contentItem->y();
            metrics.regionContentBottom = regionItem->height() - (contentItem->y() + contentItem->height());
        }
    }

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

    auto moduleRegistry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto compUuid = *core::Uuid::parse("44444444-4444-4444-4444-444444444444").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();
    const auto compId = *dsp::ModuleInstanceId::from_uuid(compUuid).value();
    auto masteringChainStateRes = app::MasteringChainState::create_default(*moduleRegistry.value(), chainUuid, gainId, eqId, compId);
    QVERIFY(masteringChainStateRes);
    auto masteringChainState = std::move(*masteringChainStateRes.value());

    app::MasteringPreviewController previewController{
        &masteringChainState,
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
    app::GainViewModel gainViewModel{&masteringChainState, &previewController};
    app::EqViewModel eqViewModel{
        &masteringChainState,
        &previewController
    };
    app::CompressorViewModel compressorViewModel{&masteringChainState, &previewController};
    app::DspChainAdapterModel dspChainAdapterModel{&gainViewModel, &eqViewModel, &compressorViewModel, &masteringChainState};

    app::GoldSelectionViewModel goldSelection{&auditionSelector};
    app::ProjectSessionViewModel projectSession{
        &model, &goldSelection, &auditionRegion, &playbackTransport};
    analysis::LiveSpectrumAnalyzer spectrumAnalyzer;
    spectrumAnalyzer.start();
    app::LiveSpectrumViewModel liveSpectrumVM{&spectrumAnalyzer};
    playbackTransport.set_pcm_prepare_handler(
        [observedPlayback](audio::AudioBufferView view, std::shared_ptr<const void>) {
            observedPlayback->state = core::PlaybackState::STOPPED;
            observedPlayback->position = view.absolute_start_frame();
            observedPlayback->duration = *core::FrameCount::create(
                view.absolute_end_frame().value()).value();
            observedPlayback->loop.reset();
            return core::Status::success();
        });
    model.set_source_committed_handler(
        [&model, &auditionRegion, &auditionSelector, &goldSelection, &dspChainAdapterModel](
            const core::ResourceReference& source) {
            const auto frames = core::FrameCount::create(model.frame_count());
            const auto rate = core::SampleRate::create(model.sample_rate_hz());
            QVERIFY(frames && rate);
            auditionRegion.source_committed(*frames.value(), *rate.value());
            QVERIFY(auditionSelector.source_committed(source));
            QVERIFY(auditionSelector.switch_to(app::AuditionTarget::PREPARED));
            dspChainAdapterModel.resetForNewSource();
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
        QStringLiteral("gainViewModel"), &gainViewModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("eqViewModel"), &eqViewModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("compressorViewModel"), &compressorViewModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("dspChainAdapterModel"), &dspChainAdapterModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("liveSpectrumViewModel"), &liveSpectrumVM);
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

    // Verify Adaptive Context Workspace and visual shell placeholders exist
    QVERIFY(root->findChild<QObject*>(QStringLiteral("adaptiveContextWorkspace")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("goldAutomatchView")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("prepareRestorationView")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("goldReferenceCard")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("goldReferenceStatus")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("totalMatchAmountKnob")) != nullptr);
    QVERIFY2(root->findChild<QObject*>(QStringLiteral("matchAmountSlider")) == nullptr, "No Match Amount slider must remain in visual shell");
    QVERIFY(root->findChild<QObject*>(QStringLiteral("automatchKnob_Tonal")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("automatchKnob_Dynamics")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("automatchKnob_Stereo")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("automatchKnob_Loudness")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("sourceHealthCard")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("restorationPlanCard")) != nullptr);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("restorationControlsGroup")) != nullptr);

    // Verify no production standalone EQ tool window exists
    QVERIFY2(root->findChild<QObject*>(QStringLiteral("parametricEqToolWindow")) == nullptr,
        "No production ParametricEqEditorWindow instance must exist in Main");

    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("m12c_b1_layoutitemproxy_recovery_1184x688.png"),
        QSize{1184, 688}));
    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("m12c_b1_compact_final_polish_1184x688.png"),
        QSize{1184, 688}));
    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("m12c_b1_compact_authored_split_1184x688.png"),
        QSize{1184, 688}));
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
    (void)itemCenter;
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
    auto* transportTimeModule = root->findChild<QObject*>(
        QStringLiteral("transportTimeModule"));
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
    QVERIFY(zoomIn && zoomOut && continuousZoom && fitSource && transportTimeModule);
    QCOMPARE(zoomOut->property("width").toInt(), 32);
    QCOMPARE(zoomOut->property("height").toInt(), 32);
    QCOMPARE(zoomIn->property("width").toInt(), 32);
    QCOMPARE(fitSource->property("width").toInt(), 32);
    QCOMPARE(continuousZoom->property("width").toInt(), 160);
    QCOMPARE(stop->property("width").toInt(), 44);
    QCOMPARE(stop->property("height").toInt(), 44);
    QCOMPARE(playPause->property("width").toInt(), 68);
    QCOMPARE(playPause->property("height").toInt(), 68);
    QCOMPARE(transportTimeModule->property("width").toInt(), 246);
    QCOMPARE(transportTimeModule->property("height").toInt(), 56);
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
    auto* gainMenuItem = root->findChild<QObject*>(
        QStringLiteral("menuViewInputGain"));
    auto* eqMenuItem = root->findChild<QObject*>(
        QStringLiteral("menuViewParametricEq"));
    QVERIFY(gainMenuItem);
    QCOMPARE(gainMenuItem->property("text").toString(), QStringLiteral("Input Gain"));
    QVERIFY(gainMenuItem->property("enabled").toBool());
    QVERIFY(eqMenuItem);
    QCOMPARE(eqMenuItem->property("text").toString(), QStringLiteral("Parametric EQ"));
    QVERIFY(eqMenuItem->property("enabled").toBool());

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

    // B4.3 Multi-Module Workspace Verification
    qInfo().noquote() << "M12C_SMOKE_PHASE=dsp-chain-rows-and-input-gain-editor";
    // Repeater delegates are visual children exposed explicitly by DspChainSelector;
    // they are not guaranteed to participate in QObject::findChild() from the window root.
    auto* dspChainRow0 = qvariant_cast<QObject*>(
        dspChainSelectorObj->property("inputGainRow"));
    auto* dspChainRow1 = qvariant_cast<QObject*>(
        dspChainSelectorObj->property("parametricEqRow"));

    auto* dspChainConfigLed0 = dspChainRow0
        ? dspChainRow0->findChild<QObject*>(QStringLiteral("dspChainConfigLed_0"))
        : nullptr;
    auto* dspChainBypassBadge0 = dspChainRow0
        ? dspChainRow0->findChild<QObject*>(QStringLiteral("dspChainBypassBadge_0"))
        : nullptr;
    auto* dspChainStateText0 = dspChainRow0
        ? dspChainRow0->findChild<QObject*>(QStringLiteral("dspChainStateText_0"))
        : nullptr;

    auto* dspChainConfigLed1 = dspChainRow1
        ? dspChainRow1->findChild<QObject*>(QStringLiteral("dspChainConfigLed_1"))
        : nullptr;
    auto* dspChainBypassBadge1 = dspChainRow1
        ? dspChainRow1->findChild<QObject*>(QStringLiteral("dspChainBypassBadge_1"))
        : nullptr;
    auto* dspChainStateText1 = dspChainRow1
        ? dspChainRow1->findChild<QObject*>(QStringLiteral("dspChainStateText_1"))
        : nullptr;

    QVERIFY2(dspChainRow0 != nullptr && dspChainConfigLed0 != nullptr && dspChainStateText0 != nullptr, "Row 0 (Input Gain) components must exist");
    QVERIFY2(dspChainRow1 != nullptr && dspChainConfigLed1 != nullptr && dspChainStateText1 != nullptr, "Row 1 (Parametric EQ) components must exist");

    // Verify Compressor Editor components
    auto* compressorEditor = dspEditorHostObj->findChild<QObject*>(QStringLiteral("compressorEditor"));
    auto* dspHostModuleTitle = dspEditorHostObj->findChild<QObject*>(QStringLiteral("dspHostModuleTitle"));
    QVERIFY2(compressorEditor != nullptr, "compressorEditor must exist in dspEditorHost");
    QVERIFY2(dspHostModuleTitle != nullptr, "dspHostModuleTitle must exist in dspEditorHost");

    // Default selected module index is 0 (Input Gain)
    QCOMPARE(dspWorkspaceObj->property("selectedModuleIndex").toInt(), 0);

    // Switch to Compressor Editor (Index 2)
    QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 2));
    QTest::qWait(50);
    QCoreApplication::processEvents();
    QVERIFY2(compressorEditor->property("visible").toBool(), "compressorEditor must be visible at selectedModuleIndex = 2");
    QCOMPARE(dspHostModuleTitle->property("text").toString(), QStringLiteral("Compressor"));

    auto* abActiveBtn = dspEditorHostObj->findChild<QObject*>(QStringLiteral("abButtonActive"));
    auto* abBypassBtn = dspEditorHostObj->findChild<QObject*>(QStringLiteral("abButtonBypass"));
    QVERIFY2(abActiveBtn && abBypassBtn, "Active and Bypass buttons must exist");
    QCOMPARE(abActiveBtn->property("text").toString(), QStringLiteral("Active"));
    QCOMPARE(abBypassBtn->property("text").toString(), QStringLiteral("Bypass"));
    QCOMPARE(abActiveBtn->property("accentColor").value<QColor>(), QColor{QStringLiteral("#00C8FF")}); // Standard focus cyan!

    auto* compressorCurveCanvas = compressorEditor->findChild<QObject*>(QStringLiteral("compressorCurveCanvas"));
    QVERIFY2(compressorCurveCanvas != nullptr, "compressorCurveCanvas must exist in compressorEditor");
    QCOMPARE(compressorViewModel.transfer_curve_points().size(), 101);

    // Interactive Numeric Draft & Commit Verification
    auto* thresholdInput = find_child_by_name(compressorEditor, QStringLiteral("thresholdDbfsInput"));
    QVERIFY2(thresholdInput != nullptr, "thresholdDbfsInput control must exist");
    auto* thresholdInputItem = qobject_cast<QQuickItem*>(thresholdInput);
    QVERIFY2(thresholdInputItem != nullptr, "thresholdDbfsInput must be a QQuickItem");

    window->requestActivate();
    QTest::qWait(50);
    QCoreApplication::processEvents();
    thresholdInputItem->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY2(thresholdInputItem->hasActiveFocus(), "thresholdDbfsInput must receive active focus");

    QMetaObject::invokeMethod(thresholdInput, "selectAll");
    for (const char c : std::string_view{"-18.0"}) {
        QTest::keyClick(window, c);
    }
    QCoreApplication::processEvents();
    QCOMPARE(compressorViewModel.threshold_text(), QStringLiteral("-18.0"));
    QCOMPARE(compressorViewModel.threshold_dbfs(), -24.0); // Not committed yet!

    QTest::keyClick(window, Qt::Key_Return);
    QCoreApplication::processEvents();
    QCOMPARE(compressorViewModel.threshold_dbfs(), -18.0); // Committed on Return!

    // Invalid draft stays draft, blocks commit, and reverts on Escape
    QMetaObject::invokeMethod(thresholdInput, "selectAll");
    for (const char c : std::string_view{"99999"}) {
        QTest::keyClick(window, c);
    }
    QCoreApplication::processEvents();
    QCOMPARE(compressorViewModel.validation_field(), QStringLiteral("thresholdDbfs"));
    QCOMPARE(compressorViewModel.threshold_dbfs(), -18.0); // Committed value unchanged

    QTest::keyClick(window, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QCOMPARE(compressorViewModel.threshold_text(), QStringLiteral("-18.0"));
    QVERIFY(compressorViewModel.validation_field().isEmpty());

    // Applicability: PEAK mode disables RMS time without destroying stored value
    auto* peakBtn = find_child_by_name(compressorEditor, QStringLiteral("detectorPeakButton"));
    auto* rmsBtn = find_child_by_name(compressorEditor, QStringLiteral("detectorRmsButton"));
    QVERIFY2(peakBtn && rmsBtn, "Detector mode buttons must exist");
    QCOMPARE(rmsBtn->property("emphasizeSelectedText").toBool(), false);
    QCOMPARE(peakBtn->property("emphasizeSelectedText").toBool(), false);
    const qreal rmsWidthBefore = rmsBtn->property("width").toReal();
    const qreal peakWidthBefore = peakBtn->property("width").toReal();
    const qreal peakXBefore = qobject_cast<QQuickItem*>(peakBtn)->x();

    // Focus-out valid commit verification
    thresholdInputItem->forceActiveFocus(Qt::TabFocusReason);
    QMetaObject::invokeMethod(thresholdInput, "selectAll");
    for (const char c : std::string_view{"-12.0"}) {
        QTest::keyClick(window, c);
    }
    QCoreApplication::processEvents();
    QCOMPARE(compressorViewModel.threshold_text(), QStringLiteral("-12.0"));
    QCOMPARE(compressorViewModel.threshold_dbfs(), -18.0); // Still previous committed value

    auto* peakBtnItem = qobject_cast<QQuickItem*>(peakBtn);
    QVERIFY(peakBtnItem);
    peakBtnItem->forceActiveFocus(Qt::TabFocusReason); // Focus-out!
    QCoreApplication::processEvents();
    QCOMPARE(compressorViewModel.threshold_dbfs(), -12.0); // Committed on focus-out!
    QVERIFY(compressorViewModel.validation_field().isEmpty());

    QVERIFY(QMetaObject::invokeMethod(peakBtn, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(compressorViewModel.detector_mode(), QStringLiteral("PEAK"));
    QVERIFY(qAbs(rmsBtn->property("width").toReal() - rmsWidthBefore) <= 0.5);
    QVERIFY(qAbs(peakBtn->property("width").toReal() - peakWidthBefore) <= 0.5);
    QVERIFY(qAbs(qobject_cast<QQuickItem*>(peakBtn)->x() - peakXBefore) <= 0.5);
    QVERIFY(!compressorViewModel.rms_time_effective());
    QCOMPARE(compressorViewModel.rms_time_constant_ms(), 50.0); // Preserved!

    QVERIFY(QMetaObject::invokeMethod(rmsBtn, "clicked"));
    QCoreApplication::processEvents();
    QCOMPARE(compressorViewModel.detector_mode(), QStringLiteral("RMS"));
    QVERIFY(compressorViewModel.rms_time_effective());

    // Stereo Link controls verification
    auto* linkMaxBtn = find_child_by_name(compressorEditor, QStringLiteral("linkMaxButton"));
    auto* linkMeanBtn = find_child_by_name(compressorEditor, QStringLiteral("linkMeanButton"));
    auto* linkDualMonoBtn = find_child_by_name(compressorEditor, QStringLiteral("linkDualMonoButton"));
    QVERIFY2(linkMaxBtn && linkMeanBtn && linkDualMonoBtn, "Stereo Link buttons must exist");
    QCOMPARE(linkMaxBtn->property("emphasizeSelectedText").toBool(), false);
    QCOMPARE(linkMeanBtn->property("emphasizeSelectedText").toBool(), false);
    QCOMPARE(linkDualMonoBtn->property("emphasizeSelectedText").toBool(), false);
    const qreal linkMaxWidthBefore = linkMaxBtn->property("width").toReal();
    const qreal linkMeanWidthBefore = linkMeanBtn->property("width").toReal();
    const qreal linkDualWidthBefore = linkDualMonoBtn->property("width").toReal();
    const qreal linkMeanXBefore = qobject_cast<QQuickItem*>(linkMeanBtn)->x();
    const qreal linkDualXBefore = qobject_cast<QQuickItem*>(linkDualMonoBtn)->x();

    if (compressorViewModel.channel_link_effective()) {
        QVERIFY(QMetaObject::invokeMethod(linkMeanBtn, "clicked"));
        QCoreApplication::processEvents();
        QCOMPARE(compressorViewModel.channel_link(), QStringLiteral("LINKED_MEAN"));
        QVERIFY(qAbs(linkMaxBtn->property("width").toReal() - linkMaxWidthBefore) <= 0.5);
        QVERIFY(qAbs(linkMeanBtn->property("width").toReal() - linkMeanWidthBefore) <= 0.5);
        QVERIFY(qAbs(linkDualMonoBtn->property("width").toReal() - linkDualWidthBefore) <= 0.5);
        QVERIFY(qAbs(qobject_cast<QQuickItem*>(linkMeanBtn)->x() - linkMeanXBefore) <= 0.5);
        QVERIFY(qAbs(qobject_cast<QQuickItem*>(linkDualMonoBtn)->x() - linkDualXBefore) <= 0.5);

        QVERIFY(QMetaObject::invokeMethod(linkDualMonoBtn, "clicked"));
        QCoreApplication::processEvents();
        QCOMPARE(compressorViewModel.channel_link(), QStringLiteral("DUAL_MONO"));

        QVERIFY(QMetaObject::invokeMethod(linkMaxBtn, "clicked"));
        QCoreApplication::processEvents();
        QCOMPARE(compressorViewModel.channel_link(), QStringLiteral("LINKED_MAX"));
    }

    QVERIFY(capture_visual_evidence(window, QStringLiteral("m14_compressor_editor_1184x688.png"), QSize{1184, 688}));
    QVERIFY(capture_visual_evidence(window, QStringLiteral("m14_compressor_editor_1440x900.png"), QSize{1440, 900}));

    // Restore selected module index to 0 (Input Gain) for remaining Gain/EQ smoke steps
    QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 0));
    QTest::qWait(50);
    QCoreApplication::processEvents();

    // Initial Gain state text & LED
    QCOMPARE(dspChainStateText0->property("text").toString(), QStringLiteral("0.0 dB Default"));
    QCOMPARE(dspChainConfigLed0->property("color").value<QColor>(), QColor{QStringLiteral("#273A4D")});
    QVERIFY2(!dspChainBypassBadge0->property("visible").toBool(), "Gain BYP badge must be hidden initially");

    // Initial EQ state text & LED
    QCOMPARE(dspChainStateText1->property("text").toString(), QStringLiteral("Flat Default"));
    QCOMPARE(dspChainConfigLed1->property("color").value<QColor>(), QColor{QStringLiteral("#273A4D")});
    QVERIFY2(!dspChainBypassBadge1->property("visible").toBool(), "EQ BYP badge must be hidden initially");

    // Input Gain Editor controls
    auto* inputGainEditor = dspEditorHostObj->findChild<QObject*>(QStringLiteral("inputGainEditor"));
    QVERIFY2(inputGainEditor != nullptr, "inputGainEditor must exist in dspEditorHost");
    QVERIFY2(inputGainEditor->property("visible").toBool(), "inputGainEditor must be visible at selectedModuleIndex = 0");

    auto* gainDbDisplay = inputGainEditor->findChild<QObject*>(QStringLiteral("gainDbDisplay"));
    auto* gainDbInput = inputGainEditor->findChild<QObject*>(QStringLiteral("gainDbInput"));
    auto* gainSlider = inputGainEditor->findChild<QObject*>(QStringLiteral("gainSlider"));
    auto* resetGainButton = inputGainEditor->findChild<QObject*>(QStringLiteral("resetGainButton"));
    auto* gainValidationError = inputGainEditor->findChild<QObject*>(QStringLiteral("gainValidationError"));

    QVERIFY2(gainDbDisplay && gainDbInput && gainSlider && resetGainButton && gainValidationError, "Input Gain editor controls must exist");
    QCOMPARE(gainDbDisplay->property("text").toString(), QStringLiteral("0.0 dB"));
    QCOMPARE(gainDbInput->property("text").toString(), QStringLiteral("0.0"));
    QVERIFY(gainDbInput->property("interactionHint").toString().contains(QStringLiteral("slider")));
    auto* gainParameterLabel = inputGainEditor->findChild<QObject*>(QStringLiteral("gainParameterLabel"));
    QVERIFY2(gainParameterLabel != nullptr, "Input Gain parameter label must exist");
    QCOMPARE(gainParameterLabel->property("text").toString(), QStringLiteral("GAIN"));

    auto* dspHostWorkflowContext = dspEditorHostObj->findChild<QObject*>(QStringLiteral("dspHostWorkflowContext"));
    auto* gainHostUndoBtn = dspEditorHostObj->findChild<QObject*>(QStringLiteral("eqUndoButton"));
    auto* gainHostRedoBtn = dspEditorHostObj->findChild<QObject*>(QStringLiteral("eqRedoButton"));
    auto* dspEditorHostItem = qobject_cast<QQuickItem*>(dspEditorHostObj);
    auto* dspHostModuleTitleItem = qobject_cast<QQuickItem*>(dspHostModuleTitle);
    auto* dspHostWorkflowContextItem = qobject_cast<QQuickItem*>(dspHostWorkflowContext);
    auto* gainHostUndoItem = qobject_cast<QQuickItem*>(gainHostUndoBtn);
    auto* gainHostRedoItem = qobject_cast<QQuickItem*>(gainHostRedoBtn);
    QVERIFY(dspHostModuleTitle && dspHostWorkflowContext && gainHostUndoBtn && gainHostRedoBtn);
    QVERIFY(dspEditorHostItem && dspHostModuleTitleItem && dspHostWorkflowContextItem && gainHostUndoItem && gainHostRedoItem);
    QCOMPARE(dspHostModuleTitle->property("text").toString(), QStringLiteral("Input Gain"));
    QCOMPARE(dspHostWorkflowContext->property("text").toString(), QStringLiteral("Mastering"));
    QVERIFY(gainHostUndoBtn->property("visible").toBool());
    QVERIFY(gainHostRedoBtn->property("visible").toBool());
    QVERIFY(!gainHostUndoBtn->property("enabled").toBool());
    QVERIFY(!gainHostRedoBtn->property("enabled").toBool());

    const QPointF moduleTitlePosGain = dspHostModuleTitleItem->mapToItem(dspEditorHostItem, QPointF{0.0, 0.0});
    const QPointF workflowContextPosGain = dspHostWorkflowContextItem->mapToItem(dspEditorHostItem, QPointF{0.0, 0.0});
    const QPointF undoPosGain = gainHostUndoItem->mapToItem(dspEditorHostItem, QPointF{0.0, 0.0});
    const QPointF redoPosGain = gainHostRedoItem->mapToItem(dspEditorHostItem, QPointF{0.0, 0.0});

    const auto subtreeHasExactText = [](QObject* parent, const QString& expected) {
        if (parent->property("text").toString() == expected) {
            return true;
        }
        for (auto* child : parent->findChildren<QObject*>()) {
            if (child->property("text").toString() == expected) {
                return true;
            }
        }
        return false;
    };
    QVERIFY(!subtreeHasExactText(dspEditorHostObj, QStringLiteral("Gain Staging / Manual Mastering")));
    QVERIFY(!subtreeHasExactText(dspEditorHostObj, QStringLiteral("Manual Mastering")));
    QVERIFY(!subtreeHasExactText(inputGainEditor, QStringLiteral("GAIN STAGING")));
    QVERIFY(!subtreeHasExactText(inputGainEditor, QStringLiteral("Fixed First Mastering Stage")));

    // Modify Input Gain to +3.5 dB via ViewModel
    QVERIFY(gainViewModel.setGainDb(3.5));
    QCoreApplication::processEvents();
    QCOMPARE(gainDbDisplay->property("text").toString(), QStringLiteral("+3.5 dB"));
    QCOMPARE(dspChainStateText0->property("text").toString(), QStringLiteral("+3.5 dB Manual"));
    QCOMPARE(dspChainConfigLed0->property("color").value<QColor>(), QColor{QStringLiteral("#00D47A")});
    QVERIFY(gainHostUndoBtn->property("enabled").toBool());

    // Standard Undo/Redo shortcuts are scoped to the active Gain module.
    dspEditorHostItem->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QCOMPARE(gainViewModel.gain_db(), 0.0);
    QTest::keyClick(window, Qt::Key_Y, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QCOMPARE(gainViewModel.gain_db(), 3.5);

    // Valid gain bounds: -24.0 and +24.0 accepted
    QVERIFY(gainViewModel.setGainDb(-24.0));
    QCoreApplication::processEvents();
    QCOMPARE(gainViewModel.gain_db(), -24.0);

    QVERIFY(gainViewModel.setGainDb(24.0));
    QCoreApplication::processEvents();
    QCOMPARE(gainViewModel.gain_db(), 24.0);

    // Invalid gain text format shows validation error and preserves previous value
    QVERIFY(!gainViewModel.setGainDbText(QStringLiteral("invalid")));
    QCoreApplication::processEvents();
    QVERIFY2(!gainViewModel.validation_error().isEmpty(), "Validation error must be non-empty for invalid gain text");
    QVERIFY2(gainValidationError->property("visible").toBool(), "gainValidationError must be visible when error exists");

    // Reset button restores 0.0 dB default
    QVERIFY2(QMetaObject::invokeMethod(resetGainButton, "clicked"), "Clicking resetGainButton must succeed");
    QCoreApplication::processEvents();
    QCOMPARE(gainViewModel.gain_db(), 0.0);
    QCOMPARE(dspChainStateText0->property("text").toString(), QStringLiteral("0.0 dB Default"));
    QCOMPARE(dspChainConfigLed0->property("color").value<QColor>(), QColor{QStringLiteral("#273A4D")});
    QVERIFY2(!gainValidationError->property("visible").toBool(), "gainValidationError must be hidden after reset");

    // Escape key in editor host restores focus to active chain row
    auto* gainDbInputItem = qobject_cast<QQuickItem*>(gainDbInput);
    auto* dspChainRow0Item = qobject_cast<QQuickItem*>(dspChainRow0);
    QVERIFY(gainDbInputItem && dspChainRow0Item);
    window->requestActivate();
    QTest::qWait(50);
    QCoreApplication::processEvents();
    gainDbInputItem->forceActiveFocus();
    QVERIFY(gainDbInputItem->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QVERIFY2(dspChainRow0Item->hasActiveFocus(), "Escape key in editor host must restore focus to active chain row");

    // View Menu switching between Input Gain (0) and Parametric EQ (1)
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

    const double gainDbBeforeSwitch = gainViewModel.gain_db();
    const bool gainBypassBeforeSwitch = gainViewModel.bypass();
    const bool eqDefaultBeforeSwitch = eqViewModel.is_default();

    const auto eqMenuCenter = realEqMenuQuickItem->mapToScene(QPointF{
        realEqMenuQuickItem->width() / 2, realEqMenuQuickItem->height() / 2});
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, eqMenuCenter.toPoint());
    QTest::qWait(120);
    QCoreApplication::processEvents();

    QCOMPARE(dspWorkspaceObj->property("selectedModuleIndex").toInt(), 1);
    QCOMPARE(gainViewModel.gain_db(), gainDbBeforeSwitch);
    QCOMPARE(gainViewModel.bypass(), gainBypassBeforeSwitch);
    QCOMPARE(eqViewModel.is_default(), eqDefaultBeforeSwitch);

    QCOMPARE(dspHostModuleTitle->property("text").toString(), QStringLiteral("Parametric EQ"));
    QCOMPARE(dspHostWorkflowContext->property("text").toString(), QStringLiteral("Mastering"));
    QVERIFY(gainHostUndoBtn->property("visible").toBool());
    QVERIFY(gainHostRedoBtn->property("visible").toBool());
    QVERIFY(!gainHostUndoBtn->property("enabled").toBool());
    QVERIFY(!gainHostRedoBtn->property("enabled").toBool());
    QCOMPARE(dspHostModuleTitleItem->mapToItem(dspEditorHostItem, QPointF{0.0, 0.0}), moduleTitlePosGain);
    QCOMPARE(dspHostWorkflowContextItem->mapToItem(dspEditorHostItem, QPointF{0.0, 0.0}), workflowContextPosGain);
    QCOMPARE(gainHostUndoItem->mapToItem(dspEditorHostItem, QPointF{0.0, 0.0}), undoPosGain);
    QCOMPARE(gainHostRedoItem->mapToItem(dspEditorHostItem, QPointF{0.0, 0.0}), redoPosGain);

    qInfo().noquote() << "M12C_SMOKE_PHASE=editor-lookup";
    auto* eqEditor = dspEditorHostObj->findChild<QObject*>(QStringLiteral("parametricEqEditor"));
    QVERIFY2(eqEditor != nullptr, "parametricEqEditor must exist inside dspEditorHost");
    QVERIFY2(eqEditor->property("visible").toBool(), "parametricEqEditor must be visible at selectedModuleIndex = 1");
    auto* eqGraph = eqEditor->findChild<QObject*>(QStringLiteral("parametricEqGraph"));
    QVERIFY2(eqGraph != nullptr, "parametricEqGraph must exist inside eqEditor");
    QVERIFY2(!eqViewModel.selected_band_response_points().isEmpty(), "eqViewModel response points must not be empty");
    QCOMPARE(eqGraph->property("maxFreq").toDouble(), 19845.0); // 44.1 kHz UI Source.wav loaded endpoint (0.45 * 44100 = 19845 Hz)
    QVERIFY2(eqEditor->findChild<QObject*>(QStringLiteral("spectrumAnalyzer")) == nullptr, "No fake analyzer or spectrum item must exist");

    qInfo().noquote() << "M12C_SMOKE_PHASE=dsp-chain-row-led-and-byp";
    // Canonical Flat state -> config LED is dark (#273A4D), BYP badge is hidden
    QVERIFY2(eqViewModel.is_default(), "EQ state must be default/flat initially");
    QCOMPARE(dspChainConfigLed1->property("color").value<QColor>(), QColor{QStringLiteral("#273A4D")});
    QVERIFY2(!dspChainBypassBadge1->property("visible").toBool(), "EQ BYP badge must be hidden initially");

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

    // Add band -> EQ state becomes Manual non-default -> EQ config LED (row 1) turns Green #00D47A
    QVERIFY2(QMetaObject::invokeMethod(addBandBtn, "clicked"), "Clicking addBandButton must succeed");
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.band_count(), 2);
    QCOMPARE(eqViewModel.selected_index(), 1);
    QVERIFY2(!eqViewModel.is_default(), "EQ state must no longer be default after adding a band");
    QCOMPARE(dspChainConfigLed1->property("color").value<QColor>(), QColor{QStringLiteral("#00D47A")});
    QCOMPARE(dspChainStateText1->property("text").toString(), QStringLiteral("Manual Edit"));
    QVERIFY2(dspChainStateText1->property("visible").toBool(), "Manual Edit must be visible for a non-default EQ");
    QVERIFY(gainHostUndoBtn->property("enabled").toBool());

    // Standard Undo/Redo shortcuts are now scoped to the selected EQ module.
    const double gainBeforeEqShortcut = gainViewModel.gain_db();
    dspEditorHostItem->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.band_count(), 1);
    QCOMPARE(gainViewModel.gain_db(), gainBeforeEqShortcut);
    QTest::keyClick(window, Qt::Key_Y, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.band_count(), 2);
    QCOMPARE(gainViewModel.gain_db(), gainBeforeEqShortcut);

    // Input Gain (row 0) remains dark/default and unchanged after EQ-only edit
    QCOMPARE(dspChainConfigLed0->property("color").value<QColor>(), QColor{QStringLiteral("#273A4D")});
    QCOMPARE(dspChainStateText0->property("text").toString(), QStringLiteral("0.0 dB Default"));

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
    auto* frequencyFieldObj = find_child_by_name(eqEditor, QStringLiteral("frequencyField"));
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
    auto* undoBtn = dspEditorHostObj->findChild<QObject*>(QStringLiteral("eqUndoButton"));
    auto* redoBtn = dspEditorHostObj->findChild<QObject*>(QStringLiteral("eqRedoButton"));
    auto* resetFlatBtn = eqEditor->findChild<QObject*>(QStringLiteral("eqResetFlatButton"));
    auto* overallToggleBtn = eqEditor->findChild<QObject*>(QStringLiteral("eqOverallToggleButton"));
    auto* spectrumToggleBtn = eqEditor->findChild<QObject*>(QStringLiteral("eqSpectrumToggleButton"));

    QVERIFY2(abBypassBtn != nullptr && abActiveBtn != nullptr, "A and B buttons must exist in host header");
    QVERIFY2(undoBtn != nullptr && redoBtn != nullptr && resetFlatBtn != nullptr, "Undo, Redo, and Reset Flat buttons must exist");
    QVERIFY2(overallToggleBtn != nullptr, "Overall toggle button must exist");
    QVERIFY2(spectrumToggleBtn != nullptr, "Spectrum toggle button must exist");

    QVERIFY2(liveSpectrumVM.spectrumEnabled(), "SPECTRUM toggle must default to ON");
    auto* spectrumCanvasObj = eqGraph->findChild<QObject*>(QStringLiteral("spectrumCanvas"));
    QVERIFY2(spectrumCanvasObj != nullptr, "spectrumCanvas must exist inside ParametricEqGraph");
    QVERIFY2(spectrumCanvasObj->property("visible").toBool(), "spectrumCanvas must be visible when SPECTRUM is ON");

    const bool undoStateBeforeToggle = eqViewModel.can_undo();
    const quint64 previewGenBeforeToggle = eqViewModel.preview_generation();

    QVERIFY(QMetaObject::invokeMethod(spectrumToggleBtn, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY2(!liveSpectrumVM.spectrumEnabled(), "SPECTRUM toggle click must set spectrumEnabled to false");
    QVERIFY2(!spectrumCanvasObj->property("visible").toBool(), "spectrumCanvas must be hidden when SPECTRUM is OFF");
    QCOMPARE(eqViewModel.can_undo(), undoStateBeforeToggle);
    QCOMPARE(eqViewModel.preview_generation(), previewGenBeforeToggle);

    QVERIFY(QMetaObject::invokeMethod(spectrumToggleBtn, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY2(liveSpectrumVM.spectrumEnabled(), "SPECTRUM toggle click must restore spectrumEnabled to true");
    QVERIFY2(spectrumCanvasObj->property("visible").toBool(), "spectrumCanvas must be restored visible when SPECTRUM is ON");

    // Click Bypass -> BYP badge becomes visible on EQ DSP chain row, while config LED remains unchanged
    QVERIFY2(QMetaObject::invokeMethod(abBypassBtn, "clicked"), "Clicking abButtonBypass must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqViewModel.bypass(), "eqViewModel.bypass must be true after clicking Bypass");
    QVERIFY2(dspChainBypassBadge1->property("visible").toBool(), "dspChainBypassBadge_1 must be visible when bypassed");

    QVERIFY2(QMetaObject::invokeMethod(abActiveBtn, "clicked"), "Clicking abButtonActive must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(!eqViewModel.bypass(), "eqViewModel.bypass must be false after clicking Active");
    QVERIFY2(!dspChainBypassBadge1->property("visible").toBool(), "dspChainBypassBadge_1 must be hidden when active");

    // Reset Flat -> restores Default status -> LED turns dark #273A4D
    QVERIFY2(QMetaObject::invokeMethod(resetFlatBtn, "clicked"), "Clicking eqResetFlatButton must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqViewModel.is_default(), "EQ state must be default/flat after Reset Flat");
    QCOMPARE(dspChainConfigLed1->property("color").value<QColor>(), QColor{QStringLiteral("#273A4D")});

    // Undo -> restores 2-band non-default state -> LED turns Green #00D47A
    QVERIFY2(QMetaObject::invokeMethod(undoBtn, "clicked"), "Clicking eqUndoButton must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(!eqViewModel.is_default(), "EQ state must be non-default after Undo");
    QCOMPARE(dspChainConfigLed1->property("color").value<QColor>(), QColor{QStringLiteral("#00D47A")});

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

    // Verify the minimum-first docked composition at representative sizes.
    qInfo().noquote() << "M12C_SMOKE_PHASE=responsive-composition";

    // Offscreen QML layout harness is independent of CI physical monitor clamping.
    // There is no width/height density breakpoint: the upper strips keep stable
    // geometry and padding while waveform/editor surfaces absorb extra space.
    const auto verifyUpperPadding = [](const LayoutEvalMetrics& metrics) {
        QCOMPARE(metrics.sourceHeight, 196.0);
        QCOMPARE(metrics.controlHeight, 72.0);
        QCOMPARE(metrics.regionHeight, 72.0);
        QCOMPARE(metrics.controlContentLeft, 12.0);
        QCOMPARE(metrics.controlContentRight, 12.0);
        QCOMPARE(metrics.regionContentLeft, 12.0);
        QCOMPARE(metrics.regionContentRight, 12.0);
        QCOMPARE(metrics.regionContentTop, 6.0);
        QCOMPARE(metrics.regionContentBottom, 6.0);
    };

    const auto verifyContainmentMetrics = [](const LayoutEvalMetrics& metrics, const char* sizeLabel) {
        // Hidden-window offscreen evaluation is authoritative for the authored layout
        // allocation itself. LayoutItemProxy target reparenting/visibility is verified
        // later on the visible native window, where the proxy actually owns its target.
        QVERIFY2(metrics.compactBottomRightContained,
            qPrintable(QString("compactBottomRight must be contained in compactBottomSplit at %1").arg(sizeLabel)));
    };

    const auto res1440 = evaluate_layout_at_size(engine, 1440, 900);
    QVERIFY2(res1440.valid, qPrintable(res1440.errorMessage));
    QCOMPARE(res1440.metrics.visible, false);
    QCOMPARE(res1440.metrics.width, 1440);
    QCOMPARE(res1440.metrics.height, 900);
    verifyUpperPadding(res1440.metrics);
    verifyContainmentMetrics(res1440.metrics, "1440x900");
    QVERIFY2(res1440.metrics.adaptiveContextVisible, "Adaptive Context must remain present at 1440x900");
    QVERIFY2(res1440.metrics.eqInspectorContained, "EQ inspector must be vertically contained at 1440x900");
    QVERIFY2(res1440.metrics.waveformHeight >= 180.0, "Waveform height must be >= 180 px at 1440x900");
    QVERIFY2(res1440.metrics.hostHeight >= 400.0, "dspEditorHost must be dominant (>= 400 px) at 1440x900");

    const auto res1920Short = evaluate_layout_at_size(engine, 1920, 688);
    QVERIFY2(res1920Short.valid, qPrintable(res1920Short.errorMessage));
    verifyUpperPadding(res1920Short.metrics);
    verifyContainmentMetrics(res1920Short.metrics, "1920x688");
    QVERIFY2(res1920Short.metrics.adaptiveContextVisible, "Adaptive Context must not disappear in a wide, short window");
    QVERIFY2(res1920Short.metrics.eqInspectorContained, "EQ inspector must remain vertically contained in a wide, short window");
    QVERIFY2(res1920Short.metrics.waveformHeight >= 135.0, "Waveform must remain recognizable in a wide, short window");

    const auto res1280 = evaluate_layout_at_size(engine, 1280, 720);
    QVERIFY2(res1280.valid, qPrintable(res1280.errorMessage));
    verifyUpperPadding(res1280.metrics);
    verifyContainmentMetrics(res1280.metrics, "1280x720");

    const auto res1184 = evaluate_layout_at_size(engine, 1184, 688);
    QVERIFY2(res1184.valid, qPrintable(res1184.errorMessage));
    QCOMPARE(res1184.metrics.visible, false);
    QCOMPARE(res1184.metrics.width, 1184);
    QCOMPARE(res1184.metrics.height, 688);
    verifyUpperPadding(res1184.metrics);
    verifyContainmentMetrics(res1184.metrics, "1184x688");
    QVERIFY2(res1184.metrics.adaptiveContextVisible, "Adaptive Context must remain visible at minimum composition");
    QVERIFY2(res1184.metrics.eqInspectorContained, "EQ inspector must be vertically contained at minimum composition");
    QVERIFY2(res1184.metrics.waveformHeight >= 135.0, "Waveform height must be >= 135 px at 1184x688");
    QVERIFY2(res1184.metrics.workspaceHeight >= 300.0, "dspWorkspace must receive min 300 px height at 1184x688");

    // Regression around the removed hard breakpoints.
    const auto res1359x751 = evaluate_layout_at_size(engine, 1359, 751);
    const auto res1361x751 = evaluate_layout_at_size(engine, 1361, 751);
    QVERIFY2(res1359x751.valid, qPrintable(res1359x751.errorMessage));
    QVERIFY2(res1361x751.valid, qPrintable(res1361x751.errorMessage));
    verifyUpperPadding(res1359x751.metrics);
    verifyUpperPadding(res1361x751.metrics);
    QCOMPARE(res1359x751.metrics.sourceHeight, res1361x751.metrics.sourceHeight);
    QCOMPARE(res1359x751.metrics.controlHeight, res1361x751.metrics.controlHeight);
    QCOMPARE(res1359x751.metrics.regionHeight, res1361x751.metrics.regionHeight);

    const auto res1440x749 = evaluate_layout_at_size(engine, 1440, 749);
    const auto res1440x751 = evaluate_layout_at_size(engine, 1440, 751);
    QVERIFY2(res1440x749.valid, qPrintable(res1440x749.errorMessage));
    QVERIFY2(res1440x751.valid, qPrintable(res1440x751.errorMessage));
    verifyUpperPadding(res1440x749.metrics);
    verifyUpperPadding(res1440x751.metrics);
    QCOMPARE(res1440x749.metrics.sourceHeight, res1440x751.metrics.sourceHeight);
    QCOMPARE(res1440x749.metrics.controlHeight, res1440x751.metrics.controlHeight);
    QCOMPARE(res1440x749.metrics.regionHeight, res1440x751.metrics.regionHeight);

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
    QCOMPARE(dspChainSelectorObj->property("width").toInt(), 164);
    QCOMPARE(dspChainStateText1->property("text").toString(), QStringLiteral("Manual Edit"));
    QVERIFY2(dspChainStateText1->property("visible").toBool(),
        "Manual Edit must remain visible at the minimum supported window size");

    // Visible-window mapped geometry is the authoritative check for LayoutItemProxy
    // rehosting. The target is only guaranteed to be reparented/resized when the
    // controlling proxy is visible.
    auto* compactBottomSplitNative = qobject_cast<QQuickItem*>(
        root->findChild<QObject*>(QStringLiteral("compactBottomSplit")));
    auto* compactBottomRightNative = qobject_cast<QQuickItem*>(
        root->findChild<QObject*>(QStringLiteral("compactBottomRight")));
    auto* dspEditorHostNative = qobject_cast<QQuickItem*>(dspEditorHostObj);
    auto* eqEditorNative = qobject_cast<QQuickItem*>(eqEditor);
    auto* dspChainSelectorNative = qobject_cast<QQuickItem*>(dspChainSelectorObj);
    QVERIFY2(compactBottomSplitNative && compactBottomRightNative && dspEditorHostNative
             && eqEditorNative && dspChainSelectorNative,
        "Minimum-size containment items must be QQuickItems");
    QVERIFY2(check_item_contained_in_ancestor(compactBottomRightNative, compactBottomSplitNative),
        "compactBottomRight must stay inside compactBottomSplit at minimum size");
    QVERIFY2(check_item_contained_in_ancestor(dspEditorHostNative, compactBottomRightNative),
        "dspEditorHost must stay inside compactBottomRight at minimum size");
    QVERIFY2(check_item_contained_in_ancestor(eqEditorNative, dspEditorHostNative),
        "parametricEqEditor must stay inside dspEditorHost at minimum size");
    QVERIFY2(check_item_contained_in_ancestor(dspChainSelectorNative, compactBottomSplitNative),
        "DSP Chain selector must stay inside compactBottomSplit at minimum size");

    for (const auto& name : {QStringLiteral("abButtonActive"), QStringLiteral("abButtonBypass"),
                             QStringLiteral("eqUndoButton"), QStringLiteral("eqRedoButton")}) {
        auto* item = qobject_cast<QQuickItem*>(find_child_by_name(dspEditorHostObj, name));
        QVERIFY2(item != nullptr, qPrintable(name + QStringLiteral(" must exist")));
        QVERIFY2(check_item_contained_in_ancestor(item, dspEditorHostNative),
            qPrintable(name + QStringLiteral(" must stay inside dspEditorHost at minimum size")));
    }

    QVERIFY2(check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(addBandBtn), eqEditorNative),
        "Add Band must stay inside Parametric EQ editor at minimum size");
    QVERIFY2(check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(removeBandBtn), eqEditorNative),
        "Remove Band must stay inside Parametric EQ editor at minimum size");

    auto* mixedRoutingBadgeNative = qobject_cast<QQuickItem*>(
        find_child_by_name(eqEditor, QStringLiteral("mixedRoutingBadge")));
    if (mixedRoutingBadgeNative && mixedRoutingBadgeNative->isVisible()) {
        QVERIFY2(check_item_contained_in_ancestor(mixedRoutingBadgeNative, eqEditorNative),
            "Mixed Routing badge must stay inside Parametric EQ editor at minimum size");
    }

    // HP/LP compact slope controls must all remain usable at the minimum window size.
    // Minimum-size UI must preserve the same control scale and labels as the wide layout.
    QCOMPARE(addBandBtn->property("text").toString(), QStringLiteral("+ Add Band"));
    QCOMPARE(removeBandBtn->property("text").toString(), QStringLiteral("- Remove Band"));
    QVERIFY(addBandBtn->property("width").toReal() >= 110.0);
    QVERIFY(removeBandBtn->property("width").toReal() >= 110.0);

    for (int bandIndex = 0; bandIndex < eqViewModel.band_count(); ++bandIndex) {
        auto* bandButton = find_child_by_name(eqEditor, QString("bandSelectorButton_%1").arg(bandIndex));
        QVERIFY2(bandButton != nullptr, "Band selector must exist at minimum size");
        QVERIFY2(bandButton->property("width").toReal() >= 90.0, "Band selector must keep full width at minimum size");
        QVERIFY2(check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(bandButton), eqEditorNative),
            "Band selector must stay inside Parametric EQ editor at minimum size");
    }

    for (const auto& token : {QStringLiteral("BELL"), QStringLiteral("NOTCH"), QStringLiteral("LOW_SHELF"),
                              QStringLiteral("HIGH_SHELF"), QStringLiteral("HIGH_PASS"), QStringLiteral("LOW_PASS")}) {
        auto* filterButton = find_child_by_name(eqEditor, QStringLiteral("filterButton_") + token);
        QVERIFY2(filterButton != nullptr, "Filter button must exist at minimum size");
        QVERIFY2(filterButton->property("width").toReal() >= 88.0, "Filter button must keep the unified 88 px width at minimum size");
        QVERIFY2(check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(filterButton), eqEditorNative),
            "Filter button must stay inside Parametric EQ editor at minimum size");
    }

    for (const auto& token : {QStringLiteral("STEREO"), QStringLiteral("MID"), QStringLiteral("SIDE"),
                              QStringLiteral("LEFT"), QStringLiteral("RIGHT")}) {
        auto* routingButton = find_child_by_name(eqEditor, QStringLiteral("routingButton_") + token);
        QVERIFY2(routingButton != nullptr, "Routing button must exist at minimum size");
        QVERIFY2(routingButton->property("width").toReal() >= 80.0, "Routing button must keep full width at minimum size");
        QVERIFY2(check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(routingButton), eqEditorNative),
            "Routing button must stay inside Parametric EQ editor at minimum size");
    }

    auto* eqEditorItemForAlignment = qobject_cast<QQuickItem*>(eqEditor);
    auto* filterBellItem = qobject_cast<QQuickItem*>(filterBell);
    auto* routeStereoItem = qobject_cast<QQuickItem*>(find_child_by_name(eqEditor, QStringLiteral("routingButton_STEREO")));
    QVERIFY2(eqEditorItemForAlignment && filterBellItem && routeStereoItem,
        "EQ editor, filter and routing anchors must be QQuickItems");
    const QPointF filterBellPos = filterBellItem->mapToItem(eqEditorItemForAlignment, QPointF{0.0, 0.0});
    const QPointF routeStereoPos = routeStereoItem->mapToItem(eqEditorItemForAlignment, QPointF{0.0, 0.0});
    QVERIFY2(qAbs(filterBellPos.x() - routeStereoPos.x()) <= 0.5,
        "FILTER and ROUTING first buttons must share the same left edge at minimum size");

    auto* filterLowPassItem = qobject_cast<QQuickItem*>(find_child_by_name(eqEditor, QStringLiteral("filterButton_LOW_PASS")));
    QVERIFY2(filterLowPassItem != nullptr, "Low Pass button must exist at minimum size");
    const QPointF filterLowPassPos = filterLowPassItem->mapToItem(eqEditorItemForAlignment, QPointF{0.0, 0.0});
    QVERIFY2(filterLowPassPos.x() + filterLowPassItem->width() <= eqEditorItemForAlignment->width() - 8.0,
        "Low Pass button must stay clear of the inspector right border at minimum size");

    auto* eqFilterLabel = find_child_by_name(eqEditor, QStringLiteral("eqFilterLabel"));
    auto* eqRoutingLabel = find_child_by_name(eqEditor, QStringLiteral("eqRoutingLabel"));
    auto* eqSlopeLabel = find_child_by_name(eqEditor, QStringLiteral("eqSlopeLabel"));
    QVERIFY2(eqFilterLabel && eqRoutingLabel && eqSlopeLabel, "EQ section labels must exist");
    QCOMPARE(eqFilterLabel->property("text").toString(), QStringLiteral("FILTER"));
    QCOMPARE(eqRoutingLabel->property("text").toString(), QStringLiteral("ROUTING"));
    QCOMPARE(eqSlopeLabel->property("text").toString(), QStringLiteral("SLOPE"));
    QVERIFY(frequencyFieldObj->property("interactionHint").toString().contains(QStringLiteral("left/right")));
    QVERIFY(gainFieldObj->property("interactionHint").toString().contains(QStringLiteral("up/down")));
    QVERIFY(qFieldObj->property("interactionHint").toString().contains(QStringLiteral("Mouse wheel")));

    for (auto* numericField : {frequencyFieldObj, gainFieldObj, qFieldObj}) {
        QVERIFY2(numericField != nullptr, "Numeric field must exist at minimum size");
        QCOMPARE(numericField->property("compact").toBool(), false);
        QVERIFY2(check_item_contained_in_ancestor(qobject_cast<QQuickItem*>(numericField), eqEditorNative),
            "Numeric field must stay inside Parametric EQ editor at minimum size");
    }

    QVERIFY2(QMetaObject::invokeMethod(filterHighPass, "clicked"), "High Pass must remain selectable at minimum size");
    QCoreApplication::processEvents();
    auto* eqEditorItemCompact = qobject_cast<QQuickItem*>(eqEditor);
    QVERIFY2(eqEditorItemCompact != nullptr, "parametricEqEditor must be a QQuickItem");
    for (const int slope : std::array{6, 12, 18, 24, 36, 48}) {
        auto* slopeObj = find_child_by_name(eqEditor, QString("slopeButton_%1").arg(slope));
        auto* slopeItem = qobject_cast<QQuickItem*>(slopeObj);
        QVERIFY2(slopeItem != nullptr && slopeItem->isVisible(),
            qPrintable(QString("Slope %1 control must be visible at minimum size").arg(slope)));
        QVERIFY2(slopeObj->property("width").toReal() >= 80.0,
            qPrintable(QString("Slope %1 control must keep full width at minimum size").arg(slope)));
        QCOMPARE(slopeObj->property("text").toString(), QString::number(slope) + QStringLiteral(" dB"));
        const QPointF p = slopeItem->mapToItem(eqEditorItemCompact, QPointF{0.0, 0.0});
        QVERIFY2(p.x() >= -0.5 && p.x() + slopeItem->width() <= eqEditorItemCompact->width() + 0.5,
            qPrintable(QString("Slope %1 control must be horizontally contained at minimum size").arg(slope)));
        QVERIFY2(p.y() >= -0.5 && p.y() + slopeItem->height() <= eqEditorItemCompact->height() + 0.5,
            qPrintable(QString("Slope %1 control must be vertically contained at minimum size").arg(slope)));
    }
    QVERIFY2(QMetaObject::invokeMethod(filterBell, "clicked"), "Bell must be restorable after minimum-size slope check");
    QCoreApplication::processEvents();

    // Verify Compressor editor containment at minimum size
    QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 2));
    QTest::qWait(50);
    QCoreApplication::processEvents();
    auto* compressorEditorNative = qobject_cast<QQuickItem*>(compressorEditor);
    QVERIFY2(compressorEditorNative != nullptr && compressorEditorNative->isVisible(),
        "Compressor editor must be visible when selected at minimum size");
    QVERIFY2(check_item_contained_in_ancestor(compressorEditorNative, dspEditorHostNative),
        "Compressor editor must stay inside dspEditorHost at minimum size");
    auto* curveWellNative = qobject_cast<QQuickItem*>(find_child_by_name(compressorEditor, QStringLiteral("compressorCurveWell")));
    auto* controlsPanelNative = qobject_cast<QQuickItem*>(find_child_by_name(compressorEditor, QStringLiteral("compressorControlsPanel")));
    if (curveWellNative) {
        QVERIFY2(check_item_contained_in_ancestor(curveWellNative, compressorEditorNative),
            "Compressor curve well must stay inside Compressor editor at minimum size");
    }
    QVERIFY2(controlsPanelNative != nullptr,
        "Compressor controls panel must exist at minimum size");
    QVERIFY2(check_item_contained_in_ancestor(controlsPanelNative, compressorEditorNative),
        "Compressor controls panel must stay inside Compressor editor at minimum size");

    // Check the actual authored children, not just their clipping parent. This guards
    // the 1184x688 stable-topology contract: no hidden overflow may pass as containment.
    for (const auto& name : {
             QStringLiteral("compressorThresholdField"),
             QStringLiteral("compressorRatioField"),
             QStringLiteral("compressorKneeField"),
             QStringLiteral("compressorAttackField"),
             QStringLiteral("compressorReleaseField"),
             QStringLiteral("compressorRmsTimeField"),
             QStringLiteral("compressorLookAheadField"),
             QStringLiteral("compressorMixField"),
             QStringLiteral("compressorMakeupField")}) {
        auto* fieldItem = qobject_cast<QQuickItem*>(find_child_by_name(compressorEditor, name));
        QVERIFY2(fieldItem != nullptr, qPrintable(name + QStringLiteral(" must exist")));
        QVERIFY2(!fieldItem->property("interactionHint").toString().isEmpty(),
            qPrintable(name + QStringLiteral(" must expose an interaction tooltip hint")));
        QVERIFY2(check_item_contained_in_ancestor(fieldItem, controlsPanelNative),
            qPrintable(name + QStringLiteral(" must stay inside Compressor controls at minimum size")));
    }

    for (auto* button : {rmsBtn, peakBtn, linkMaxBtn, linkMeanBtn, linkDualMonoBtn}) {
        auto* buttonItem = qobject_cast<QQuickItem*>(button);
        QVERIFY2(buttonItem != nullptr, "Compressor selector button must be a QQuickItem");
        QVERIFY2(check_item_contained_in_ancestor(buttonItem, controlsPanelNative),
            "Detector/Stereo Link buttons must stay inside Compressor controls at minimum size");
    }

    // Verify the alternate Input Gain editor on the same visible minimum-size host,
    // then restore Parametric EQ for the final evidence capture.
    QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 0));
    // Visibility changes inside QQuickLayout are applied during the next polish/layout pass.
    // Let the visible native window settle before measuring mapped geometry.
    QTest::qWait(50);
    QCoreApplication::processEvents();
    auto* inputGainEditorNative = qobject_cast<QQuickItem*>(inputGainEditor);
    QVERIFY2(inputGainEditorNative != nullptr && inputGainEditorNative->isVisible(),
        "Input Gain editor must be visible when selected at minimum size");
    QVERIFY2(check_item_contained_in_ancestor(inputGainEditorNative, dspEditorHostNative),
        "Input Gain editor must stay inside dspEditorHost at minimum size");
    for (auto* control : {resetGainButton, gainDbDisplay, gainSlider, gainDbInput}) {
        auto* item = qobject_cast<QQuickItem*>(control);
        QVERIFY2(item != nullptr, "Input Gain control must be a QQuickItem");
        QVERIFY2(check_item_contained_in_ancestor(item, inputGainEditorNative),
            "Input Gain control must stay inside Input Gain editor at minimum size");
    }
    QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 1));
    QTest::qWait(50);
    QCoreApplication::processEvents();
    QVERIFY2(eqEditorNative->isVisible(), "Parametric EQ must be restored for minimum-size evidence");

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
