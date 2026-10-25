#include "audition_region_view_model.hpp"
#include "audition_source_selector.hpp"
#include "compressor_view_model.hpp"
#include "dsp_chain_adapter_model.hpp"
#include "true_peak_static_coordinator.hpp"
#include "eq_view_model.hpp"
#include "gain_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "live_spectrum_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"
#include "playback_transport_view_model.hpp"
#include <rgsml/analysis/live_spectrum_analyzer.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include "../unit/render/render_test_support.hpp"
#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "stereo_ms_view_model.hpp"
#include "waveform_item.hpp"
#include "waveform_presentation.hpp"
#include "../audio_golden/wav/golden_vectors.hpp"
#include "../unit/audio/wav_test_support.hpp"
#include "../unit/platform/fake_playback_service.hpp"

#include <QCoreApplication>
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QLibraryInfo>
#include <QRegularExpressionValidator>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>
#include <QWheelEvent>

#include <cmath>
#include <memory>
#include <optional>
#include <utility>

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

// Geometric regression oracle: the colored run terminates exactly at the
// WHITE handle center, at both polarities. This accounts for handle radius
// and also checks that the zero origin is at the correct normalized point.
// Do not test just a plotted percentage of the background track width.
[[nodiscard]] bool bipolar_fill_matches_handle(
    QObject* slider, const QString& trackName,
    const QString& fillName, const QString& thumbName,
    double zeroNormalized)
{
    auto* track = qobject_cast<QQuickItem*>(
        find_child_by_name(slider, trackName));
    auto* fill = qobject_cast<QQuickItem*>(
        find_child_by_name(slider, fillName));
    auto* thumb = qobject_cast<QQuickItem*>(
        find_child_by_name(slider, thumbName));
    if (!track || !fill || !thumb || track->width() <= thumb->width())
        return false;
    const double centerX = thumb->mapToItem(
        track, QPointF{thumb->width() / 2, thumb->height() / 2}).x();
    const double radius = thumb->width() / 2;
    const double zeroX = radius + zeroNormalized * (track->width() - 2 * radius);
    const double startX = std::min(centerX, zeroX);
    const double endX = std::max(centerX, zeroX);
    return std::abs(fill->x() - startX) <= 0.7
        && std::abs(fill->x() + fill->width() - endX) <= 0.7;
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
    bool eqStatusContained{false};
    bool eqStatusOverGraph{false};
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
            metrics.eqStatusContained = check_item_contained_in_ancestor(
                statusItem, eqEditorItem);
            metrics.eqStatusOverGraph = check_item_contained_in_ancestor(
                statusItem, qobject_cast<QQuickItem*>(eqGraph));
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
    void stereoMsProductDefaultDockedStartup();
    void stereoMsOptInVisualEvidence();
    void truePeakReadOnlyAnalysisVisualEvidence();
    void truePeakLimiterLayoutCandidate();
    void truePeakLimiterHostFamilyTheme();
    void genericFloatingProxyPreservesEditorIdentity();
};



void SourceMetadataPanelSmokeTest::genericFloatingProxyPreservesEditorIdentity()
{
    // Qt 6.8 platform smoke: a single QQuickItem may only be controlled
    // by ONE visible LayoutItemProxy at a time. Cross-window transfer must
    // retain the very same QObject, state and instance identity.
    QQmlEngine engine;
    QQmlComponent component{&engine};
    static constexpr auto qml=R"qml(
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import Rgsml.Ui 1.0
Window {
    id: mainWindow
    visible: false
    width: 640
    height: 460
    property bool detached: false

    // A single persistent control created ONCE, never duplicated by Window.
    Rectangle {
        id: realEditor
        objectName: "dockingProofRealEditor"
        color: "#13222F"
        property int edits: 17
        property string instanceIdentity: "module:stable:one"
        implicitWidth: 360
        implicitHeight: 250
    }
    LayoutItemProxy {
        id: dockProxy
        objectName: "dockingProofDockProxy"
        anchors.fill: parent
        visible: !mainWindow.detached
        target: realEditor
    }
    DspFloatingModuleWindow {
        id: floatingWindow
        objectName: "dockingProofFloatingWindow"
        moduleInstanceId: "module:stable:one"
        moduleTitle: "Test Module"
        familyAccent: "#4E86C6"
        editorTarget: realEditor
        detached: mainWindow.detached
        onRedockRequested: function(id) {
            if (id === "module:stable:one")
                mainWindow.detached = false
        }
    }
}
)qml";
    component.setData(QByteArray{qml},
        QUrl{QStringLiteral("qrc:/docking/cross-window-probe.qml")});
    QVERIFY2(component.status()!=QQmlComponent::Error,
        qPrintable(component.errorString()));
    QScopedPointer<QObject> owned{component.create()};
    QVERIFY2(owned,qPrintable(component.errorString()));
    auto* dockWindow=qobject_cast<QQuickWindow*>(owned.data());
    QVERIFY(dockWindow);
    auto* editor=qobject_cast<QQuickItem*>(find_child_by_name(
        owned.data(),QStringLiteral("dockingProofRealEditor")));
    auto* floating=qobject_cast<QQuickWindow*>(find_child_by_name(
        owned.data(),QStringLiteral("dockingProofFloatingWindow")));
    auto* dockProxy=qobject_cast<QQuickItem*>(find_child_by_name(
        owned.data(),QStringLiteral("dockingProofDockProxy")));
    QVERIFY(editor && floating && dockProxy);
    dockWindow->show();
    QTRY_VERIFY_WITH_TIMEOUT(editor->window()==dockWindow,5000);
    QCOMPARE(editor->property("edits").toInt(),17);
    QCOMPARE(editor->property("instanceIdentity").toString(),
             QStringLiteral("module:stable:one"));
    QVERIFY(owned->setProperty("detached",true));
    QTRY_VERIFY_WITH_TIMEOUT(floating->isVisible(),5000);
    QTRY_VERIFY_WITH_TIMEOUT(editor->window()==floating,5000);
    QCOMPARE(editor->property("edits").toInt(),17);
    QVERIFY(editor->setProperty("edits",18));
    QVERIFY(QMetaObject::invokeMethod(floating,"returnToDock"));
    QTRY_VERIFY_WITH_TIMEOUT(!floating->isVisible(),5000);
    QTRY_VERIFY_WITH_TIMEOUT(editor->window()==dockWindow,5000);
    QCOMPARE(editor->property("edits").toInt(),18);
    QCOMPARE(editor->property("instanceIdentity").toString(),
             QStringLiteral("module:stable:one"));
    QVERIFY(!owned->property("detached").toBool());
    // Regression: the first redock must not sever the parent-owned QML
    // binding. Repeated undock must again move the SAME persistent editor.
    QVERIFY(owned->setProperty("detached",true));
    QTRY_VERIFY_WITH_TIMEOUT(editor->window()==floating,5000);
    QVERIFY(QMetaObject::invokeMethod(floating,"returnToDock"));
    QTRY_VERIFY_WITH_TIMEOUT(editor->window()==dockWindow,5000);
    QCOMPARE(editor->property("edits").toInt(),18);
    QVERIFY(!owned->property("detached").toBool());
    dockWindow->close();
}

void SourceMetadataPanelSmokeTest::truePeakLimiterHostFamilyTheme()
{
    // Real DspEditorHost/StudioButton QML, with a clearly isolated visual
    // fixture. No synthetic module is installed in the productive chain.
    QQmlEngine engine;
    QQmlComponent component{&engine};
    static constexpr auto qml = R"qml(
import QtQuick
import QtQuick.Window
import Rgsml.Ui 1.0
Window {
    id: fixtureWindow
    width: 1184
    height: 688
    property bool meterMode: false
    visible: false
    color: "#06121F"
    QtObject {
        id: limiter
        objectName: "m17FixtureLimiter"
        property string editorContentKey: "TRUE_PEAK_LIMITER"
        property string displayName: "True Peak Limiter"
        property string workspaceContextLabel: "Mastering · UI fixture"
        property string familyAccent: "#C4774A"
        property bool bypass: true
        property bool bypassSupported: true
        property bool historySupported: false
        property bool previewSupported: false
        property bool canUndo: false
        property bool canRedo: false
        property var gainViewModel: null
        property var eqViewModel: null
        property var compressorViewModel: null
        function setBypass(value) { bypass = value }
    }
    QtObject {
        id: meter
        objectName: "m16FixtureMeter"
        property string editorContentKey: "TRUE_PEAK_METER"
        property string displayName: "True Peak Meter"
        property string workspaceContextLabel: "Mastering · UI fixture"
        property string familyAccent: "#6683A1"
        property bool bypass: false
        property bool bypassSupported: false
        property bool historySupported: false
        property bool previewSupported: false
        property bool canUndo: false
        property bool canRedo: false
        property var gainViewModel: null
        property var eqViewModel: null
        property var compressorViewModel: null
    }
    QtObject {
        id: fixtureModel
        property var activeModule: fixtureWindow.meterMode ? meter : limiter
    }
    Rectangle {
        id: captureSurface
        objectName: "m17HostCaptureSurface"
        width: 1184
        height: 688
        color: "#06121F"
        DspEditorHost {
            id: host
            objectName: "m17HostFixture"
            x: 196
            y: 18
            width: 972
            height: 652
            adapterModel: fixtureModel
        }
    }
}
)qml";
    component.setData(QByteArray{qml},
        QUrl{QStringLiteral("qrc:/m17/host-theme-fixture.qml")});
    QVERIFY2(component.status() != QQmlComponent::Error,
        qPrintable(component.errorString()));
    QScopedPointer<QObject> root{component.create()};
    QVERIFY2(root, qPrintable(component.errorString()));
    auto* window = qobject_cast<QQuickWindow*>(root.data());
    QVERIFY(window);
    window->show();
    QTest::qWait(150);
    QCoreApplication::processEvents();
    auto* surface = qobject_cast<QQuickItem*>(find_child_by_name(
        root.data(), QStringLiteral("m17HostCaptureSurface")));
    auto* fixture = find_child_by_name(root.data(),
        QStringLiteral("m17FixtureLimiter"));
    auto* host = qobject_cast<QQuickItem*>(find_child_by_name(
        root.data(), QStringLiteral("m17HostFixture")));
    auto* active = find_child_by_name(root.data(),
        QStringLiteral("abButtonActive"));
    auto* bypass = find_child_by_name(root.data(),
        QStringLiteral("abButtonBypass"));
    auto* editor = find_child_by_name(root.data(),
        QStringLiteral("truePeakLimiterEditor"));
    QVERIFY(surface && fixture && host && active && bypass && editor);
    auto captureHost = [surface](const QString& fileName) {
        const auto grab = surface->grabToImage();
        if (!grab)
            return false;
        QSignalSpy ready{grab.data(), &QQuickItemGrabResult::ready};
        if (ready.empty() && !ready.wait(15000))
            return false;
        const auto picture = grab->image();
        if (picture.isNull() || picture.size() != QSize{1184, 688})
            return false;
        const QString dir = qEnvironmentVariable("RGSML_GUI01_EVIDENCE_DIR");
        return dir.isEmpty() || (QDir{}.mkpath(dir)
            && picture.save(QDir{dir}.filePath(fileName), "PNG"));
    };
    QVERIFY(editor->property("visible").toBool());
    QVERIFY(active->property("selectedFamilyAccent").toBool());
    QVERIFY(bypass->property("selectedFamilyAccent").toBool());
    const auto checkTheme = [&](bool isBypass, const QString& expected) {
        if (!fixture->setProperty("bypass", isBypass)) return false;
        QCoreApplication::processEvents();
        const QColor accent{expected};
        if (host->property("activeFamilyAccent").value<QColor>() != accent) return false;
        if (active->property("accentColor").value<QColor>() != accent) return false;
        if (bypass->property("accentColor").value<QColor>() != accent) return false;
        if (!editor->property("visible").toBool()) return false;
        return true;
    };
    // Compare both module selections in their full shared DspEditorHost,
    // at the actually constrained Main-window editor heights.
    auto* meterEditor = qobject_cast<QQuickItem*>(find_child_by_name(
        host, QStringLiteral("truePeakMeterEditor")));
    auto* limiterEditor = qobject_cast<QQuickItem*>(find_child_by_name(
        host, QStringLiteral("truePeakLimiterEditor")));
    auto* actionBand = qobject_cast<QQuickItem*>(find_child_by_name(
        host, QStringLiteral("m17ExplicitPreparationControls")));
    auto* inGrProgress = find_child_by_name(
        limiterEditor, QStringLiteral("m17PreparationProgressBar"));
    QVERIFY(meterEditor && limiterEditor && actionBand && inGrProgress);
    auto hostRect = [host](QQuickItem* editor, const QString& childName)
        -> QRectF {
        auto* child=qobject_cast<QQuickItem*>(
            find_child_by_name(editor,childName));
        if(!child)return {};
        return {child->mapToItem(host,QPointF{}),child->size()};
    };
    for(const int editorHeight : {360,420,652}) {
        host->setHeight(editorHeight);
        QVERIFY(window->setProperty("meterMode",true));
        // QtQuick.Layouts applies geometry in the next polish/render
        // cycle. Comparing before that cycle uses the PREVIOUS 652px M16
        // lane against the newly resized 360px M17 lane, a false parity
        // failure. Settle BOTH selections before measuring coordinates.
        QTest::qWait(100);
        QCoreApplication::processEvents();
        QVERIFY(meterEditor->isVisible());
        QVERIFY(actionBand->isVisible());
        QCOMPARE(actionBand->height(),34.0);
        const QStringList sameAnchors={
            QStringLiteral("truePeakLeftLane"),
            QStringLiteral("truePeakRightLane"),
            QStringLiteral("truePeakRecentCard"),
            QStringLiteral("truePeakHoldCard"),
            QStringLiteral("truePeakFullResourceCard")};
        QList<QRectF> m16Rects;
        for(const auto& name:sameAnchors)
            m16Rects.append(hostRect(meterEditor,name));
        QVERIFY(window->setProperty("meterMode",false));
        QTest::qWait(100);
        QCoreApplication::processEvents();
        QVERIFY(limiterEditor->isVisible());
        QVERIFY(actionBand->isVisible());
        QCOMPARE(actionBand->height(),34.0);
        for(int i=0;i<sameAnchors.size();++i) {
            const auto m17Rect=hostRect(limiterEditor,sameAnchors[i]);
            QVERIFY2(!m16Rects[i].isEmpty() && !m17Rect.isEmpty(),
                     qPrintable(sameAnchors[i]));
            QCOMPARE(m16Rects[i],m17Rect);
        }
    }
    host->setHeight(652);
    QVERIFY(window->setProperty("meterMode",false));
    QCoreApplication::processEvents();
    QVERIFY(checkTheme(true, QStringLiteral("#6CBCCF")));
    QVERIFY2(captureHost(QStringLiteral("m17_host_family_byp_1184x688.png")),
        "Native M17 DSP host BYP screenshot must be captured");
    QVERIFY(checkTheme(false, QStringLiteral("#C4774A")));
    QVERIFY2(captureHost(QStringLiteral("m17_host_family_on_1184x688.png")),
        "Native M17 DSP host ON screenshot must be captured");
    QVERIFY(checkTheme(true, QStringLiteral("#6CBCCF")));
    window->close();
}

void SourceMetadataPanelSmokeTest::truePeakLimiterLayoutCandidate()
{
    // One physical layout for M16 and M17: TP L/R tracks, axis and
    // first three right cards remain EXACTLY fixed when selecting either DSP.
    QQmlEngine engine;
    QQmlComponent component{&engine};
    static constexpr auto qml=R"qml(
import QtQuick
import QtQuick.Window
import Rgsml.Ui 1.0
Window {
    id: fixture
    objectName: "gateMParityFixture"
    property bool bypass: true
    property string validationMessage: ""
    width: 1184
    height: 688
    visible: false
    color: "#06121F"
    QtObject {
        id: vm
        property string validationField: fixture.validationMessage.length>0 ? "ceilingDbTp" : ""
        property string validationMessage: fixture.validationMessage
        property real ceilingDbTp: -1
        property string ceilingDbTpText: "-1.00"
    }
    Rectangle {
        id: surface
        objectName: "m17CandidateCaptureSurface"
        width: 1184
        height: 688
        color: "#06121F"
        TruePeakMeterEditor {
            objectName: "gateMM16Meter"
            x: (parent.width-width)/2; y: 70
            width: parent.width-474; height: parent.height-140
        }
        TruePeakLimiterEditor {
            objectName: "m17CandidateEditor"
            x: (parent.width-width)/2; y: 70
            width: parent.width-474; height: parent.height-140
            limiterModule: fixture
            limiterViewModel: vm
        }
    }
}
)qml";
    component.setData(QByteArray{qml},
        QUrl{QStringLiteral("qrc:/m17/gate-m-meter-parity.qml")});
    QVERIFY2(component.status()!=QQmlComponent::Error,
        qPrintable(component.errorString()));
    QScopedPointer<QObject> instance{component.create()};
    QVERIFY2(instance,qPrintable(component.errorString()));
    auto* window=qobject_cast<QQuickWindow*>(instance.data());
    auto* m16=qobject_cast<QQuickItem*>(find_child_by_name(
        instance.data(),QStringLiteral("gateMM16Meter")));
    auto* m17=qobject_cast<QQuickItem*>(find_child_by_name(
        instance.data(),QStringLiteral("m17CandidateEditor")));
    auto* surface=qobject_cast<QQuickItem*>(find_child_by_name(
        instance.data(),QStringLiteral("m17CandidateCaptureSurface")));
    auto* fixture=find_child_by_name(instance.data(),
        QStringLiteral("gateMParityFixture"));
    QVERIFY(window&&m16&&m17&&surface&&fixture);
    window->show();
    QTest::qWait(150);
    QCoreApplication::processEvents();
    auto rect=[](QQuickItem* editor,const QString& name)->QRectF {
        auto* child=qobject_cast<QQuickItem*>(find_child_by_name(editor,name));
        return child?QRectF{child->mapToItem(editor,QPointF{}),child->size()}:QRectF{};
    };
    auto contained=[m17](const QString& name)->bool {
        auto* child=qobject_cast<QQuickItem*>(find_child_by_name(m17,name));
        if(!child)return false;
        const auto at=child->mapToItem(m17,QPointF{});
        return at.x()>=-1 && at.y()>=-1
            && at.x()+child->width()<=m17->width()+1
            && at.y()+child->height()<=m17->height()+1;
    };
    const QStringList stable={
        QStringLiteral("truePeakLeftLane"),
        QStringLiteral("truePeakRightLane"),
        QStringLiteral("truePeakDbTpScale"),
        QStringLiteral("truePeakRecentCard"),
        QStringLiteral("truePeakHoldCard"),
        QStringLiteral("truePeakFullResourceCard")};
    for(const auto size:{QSize{1184,688},QSize{1440,900},
                         QSize{1184,560},QSize{1440,620}}) {
        surface->setWidth(size.width());surface->setHeight(size.height());
        window->resize(size);
        QTest::qWait(130);
        QCoreApplication::processEvents();
        for(const auto& name:stable) {
            const auto a=rect(m16,name),b=rect(m17,name);
            QVERIFY2(!a.isEmpty(),qPrintable(QStringLiteral("Missing M16 ")+name));
            QCOMPARE(a,b);
        }
        QVERIFY(contained(QStringLiteral("m17CeilingCard")));
        QVERIFY(contained(QStringLiteral("m17VerificationTrim")));
        QVERIFY(contained(QStringLiteral("m17GainReductionPlot")));
        auto* gr=qobject_cast<QQuickItem*>(find_child_by_name(
            m17,QStringLiteral("m17GainReductionPlot")));
        QVERIFY(gr);
        QCOMPARE(gr->height(),54.0);
        const auto baseline=rect(m17,QStringLiteral("truePeakLeftLane"));
        QVERIFY(fixture->setProperty("bypass",false));
        QCoreApplication::processEvents();
        QCOMPARE(rect(m17,QStringLiteral("truePeakLeftLane")),baseline);
        QVERIFY(fixture->setProperty("validationMessage",
            QStringLiteral("Ceiling range invalid")));
        QCoreApplication::processEvents();
        QCOMPARE(rect(m17,QStringLiteral("truePeakLeftLane")),baseline);
        QVERIFY(fixture->setProperty("validationMessage",QString{}));
        QVERIFY(fixture->setProperty("bypass",true));
        QCoreApplication::processEvents();
        const auto grab=surface->grabToImage();
        QVERIFY(grab);
        QSignalSpy ready{grab.data(),&QQuickItemGrabResult::ready};
        QVERIFY(!ready.empty()||ready.wait(15000));
        const auto img=grab->image();
        QVERIFY(!img.isNull());
        QCOMPARE(img.size(),size);
        const auto dir=qEnvironmentVariable("RGSML_GUI01_EVIDENCE_DIR");
        if(!dir.isEmpty()) {
            QVERIFY(QDir{}.mkpath(dir));
            QVERIFY(img.save(QDir{dir}.filePath(
                QStringLiteral("gate_m_shared_meter_")
                    +QString::number(size.width())+"x"
                    +QString::number(size.height())+".png"),"PNG"));
        }
    }
    window->close();
}

void SourceMetadataPanelSmokeTest::truePeakReadOnlyAnalysisVisualEvidence()
{
    // B4b: the actual QML host and Chain row, unlike the older 4/4 smoke,
    // must be exercised with M16's presentation-only adapter selected.
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chainId = *core::Uuid::parse(
        "fa000000-0000-4000-8000-000000000001").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("fa000000-0000-4000-8000-000000000010").value()).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("fa000000-0000-4000-8000-000000000020").value()).value();
    const auto compId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("fa000000-0000-4000-8000-000000000030").value()).value();
    auto stateResult = app::MasteringChainState::create_default(
        *registry.value(), chainId, gainId, eqId, compId);
    QVERIFY(stateResult);
    auto state = std::move(*stateResult.value());
    app::GainViewModel gain{&state};
    app::EqViewModel eq{&state};
    app::CompressorViewModel compressor{&state};
    app::DspChainAdapterModel model{
        &gain, &eq, &compressor, &state,
        QStringLiteral("Mastering"), nullptr, nullptr, true};
    QCOMPARE(state.instances().size(), std::size_t{3});
    QCOMPARE(model.modules().size(), qsizetype{4});
    model.setSelectedIndex(3);
    QCOMPARE(model.active_module()->editor_content_key(),
             QStringLiteral("TRUE_PEAK_METER"));
    QVERIFY(!model.active_module()->bypass_supported());

    app::TruePeakStaticCoordinator visualMeasurement;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(
        QStringLiteral("m16VisualAdapterModel"), &model);
    engine.rootContext()->setContextProperty(
        QStringLiteral("m16StaticVisualProvider"), &visualMeasurement);
    QQmlComponent component{&engine};
    static constexpr auto qml = R"qml(
import QtQuick
import QtQuick.Window
import Rgsml.Ui 1.0

Window {
    width: 1184
    height: 688
    visible: false
    color: "#06121F"
    Rectangle {
        objectName: "m16VisualCaptureSurface"
        width: parent.width
        height: parent.height
        color: "#06121F"
        DspChainSelector {
            objectName: "m16VisualChain"
            x: 12
            y: 16
            width: 180
            height: parent.height - 32
            adapterModel: m16VisualAdapterModel
        }
        DspEditorHost {
            objectName: "m16VisualHost"
            x: 202
            y: 16
            width: parent.width - 224
            height: parent.height - 32
            adapterModel: m16VisualAdapterModel
            // Explicitly NO source-bound measurement in this fixture:
            // no numeric true peak or fake audio source may appear.
            truePeakStaticViewModel: m16StaticVisualProvider
        }
    }
}
)qml";
    component.setData(
        QByteArray{qml},
        QUrl{QStringLiteral("qrc:/m16/read-only-visual-window.qml")});
    QVERIFY2(component.status() != QQmlComponent::Error,
             qPrintable(component.errorString()));
    QScopedPointer<QObject> componentRoot{component.create()};
    QVERIFY2(componentRoot != nullptr, qPrintable(component.errorString()));
    auto* window = qobject_cast<QQuickWindow*>(componentRoot.data());
    QVERIFY(window);
    window->show();
    QTest::qWait(150);
    QCoreApplication::processEvents();

    auto* host = find_child_by_name(
        componentRoot.data(), QStringLiteral("m16VisualHost"));
    auto* chain = find_child_by_name(
        componentRoot.data(), QStringLiteral("m16VisualChain"));
    auto* meter = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakMeterEditor"));
    auto* recent = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakRecentCard"));
    auto* hold = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakHoldCard"));
    auto* full = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakFullResourceCard"));
    auto* provenance = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakProvenanceCard"));
    auto* staticBay = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakStaticMeterBay"));
    auto* leftLane = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakLeftLane"));
    auto* rightLane = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakRightLane"));
    auto* sharedRail = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakMeterSharedRail"));
    auto* railContents = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakSharedRailContents"));
    auto* leftScale = find_child_by_name(
        componentRoot.data(), QStringLiteral("truePeakDbTpScale"));
    auto* row = find_child_by_name(
        componentRoot.data(), QStringLiteral("dspChainRow_3"));
    auto* previewBadge = find_child_by_name(
        componentRoot.data(), QStringLiteral("dspHostPreviewBadge"));
    auto* active = find_child_by_name(
        componentRoot.data(), QStringLiteral("abButtonActive"));
    auto* bypass = find_child_by_name(
        componentRoot.data(), QStringLiteral("abButtonBypass"));
    auto* undo = find_child_by_name(
        componentRoot.data(), QStringLiteral("eqUndoButton"));
    auto* redo = find_child_by_name(
        componentRoot.data(), QStringLiteral("eqRedoButton"));
    QVERIFY2(host && chain && meter && recent && hold && full && provenance
             && staticBay && leftLane && rightLane && sharedRail
             && railContents && leftScale && row
             && previewBadge && active && bypass && undo && redo,
             "M16 needs a real row/editor/cards in the standard DSP host");
    QCOMPARE(host->property("moduleTitle").toString(),
             QStringLiteral("True Peak Meter"));
    QCOMPARE(host->property("moduleContext").toString(),
             QStringLiteral("Mastering"));
    QVERIFY2(meter->property("visible").toBool(),
             "Selecting virtual True Peak must display its editor");
    QVERIFY2(!previewBadge->property("visible").toBool(),
             "Analysis-only M16 has no synthetic preview status");
    const auto* controlsParent = qobject_cast<QQuickItem*>(active)->parentItem();
    QVERIFY(controlsParent);
    QVERIFY2(!controlsParent->property("visible").toBool(),
             "All sound processing and edit controls must vanish for M16");
    QVERIFY2(!qobject_cast<QQuickItem*>(bypass)->parentItem()->property("visible").toBool(),
             "No bypass control can be interactable for M16");
    QCOMPARE(recent->property("reading").toString(),
             QStringLiteral("UNAVAILABLE"));
    QCOMPARE(hold->property("reading").toString(),
             QStringLiteral("UNAVAILABLE"));
    QCOMPARE(hold->property("available").toBool(), false);
    QCOMPARE(hold->property("heading").toString(),
             QStringLiteral("SHORT HOLD / 1 SECOND"));
    // Amber reading alone identifies HOLD; no redundant legend line.
    QVERIFY(!hold->property("showColorKey").toBool());
    auto* holdKey = find_child_by_name(
        hold, QStringLiteral("truePeakHoldLegendLine"));
    QVERIFY(holdKey);
    QVERIFY(!holdKey->property("visible").toBool());
    QCOMPARE(hold->property("valueColor"), hold->property("keyColor"));
    QCOMPARE(full->property("valueColor").value<QColor>(),
             QColor{QStringLiteral("#6CBCCF")});
    // FIX-UI-015: with no independently consumed LIVE interval, the
    // primary bars never fall back to future full-resource static maxima.
    QCOMPARE(leftLane->property("meterKind").toString(),
             QStringLiteral("RECENT 100 ms"));
    QCOMPARE(rightLane->property("meterKind").toString(),
             QStringLiteral("RECENT 100 ms"));
    QCOMPARE(full->property("reading").toString(),
             QStringLiteral("—"));
    QCOMPARE(full->property("available").toBool(), false);
    QCOMPARE(leftLane->property("reading").toString(), QStringLiteral("—"));
    QCOMPARE(rightLane->property("reading").toString(), QStringLiteral("—"));
    QVERIFY(!leftLane->property("available").toBool());
    QVERIFY(!rightLane->property("available").toBool());
    QVERIFY(!leftLane->property("aboveDigitalZero").toBool());
    QVERIFY(!rightLane->property("aboveDigitalZero").toBool());
    QCOMPARE(leftLane->property("level").toDouble(), 0.0);
    QCOMPARE(rightLane->property("level").toDouble(), 0.0);
    for (auto* lane : {leftLane, rightLane}) {
        auto* marker = find_child_by_name(
            lane, QStringLiteral("truePeakHoldLine"));
        QVERIFY2(marker, "Each physical channel track needs its own HOLD mark");
        QVERIFY2(!marker->property("visible").toBool(),
                 "Unknown 1s history must never display a HOLD mark");
        QCOMPARE(lane->property("holdAvailable").toBool(), false);
        QCOMPARE(lane->property("holdLevel").toDouble(), 0.0);
    }
    QVERIFY2(staticBay->property("width").toDouble() > 340.0,
             "The True Peak static meter well must have its own region");
    QVERIFY2(sharedRail->property("width").toDouble() >= 225.0,
             "The shared True Peak rail must remain usable");
    QVERIFY2(leftScale->property("height").toDouble() > 120.0,
             "L/R dBTP tick scale must be visible");

    // Correct physical placement of channel markers and upper +6 tick.
    const auto checkMeterAlignment = [&](QObject* lane) {
        auto* name = qobject_cast<QQuickItem*>(
            find_child_by_name(lane, QStringLiteral("truePeakChannelLabel")));
        auto* track = qobject_cast<QQuickItem*>(
            find_child_by_name(lane, QStringLiteral("truePeakTrack")));
        auto* scale = find_child_by_name(lane, QStringLiteral("truePeakDbTpScale"));
        QVERIFY(name && track && scale);
        const auto nameCenter = name->mapToScene(
            QPointF{name->width() / 2.0, name->height() / 2.0});
        const auto trackCenter = track->mapToScene(
            QPointF{track->width() / 2.0, track->height() / 2.0});
        QVERIFY2(qAbs(nameCenter.x() - trackCenter.x()) <= 1.5,
                 "L/R must center on the meter track");
        auto* plus = qobject_cast<QQuickItem*>(
            find_child_by_name(scale, QStringLiteral("truePeakPlusSixMarker")));
        QVERIFY(plus);
        const auto mark = plus->mapToScene(
            QPointF{0.0, plus->height() / 2.0});
        const auto top = track->mapToScene(QPointF{0.0, 5.0});
        QVERIFY2(qAbs(mark.y() - top.y()) <= 1.5,
                 "+6 must center on the real scale top");
    };
    checkMeterAlignment(leftLane);
    checkMeterAlignment(rightLane);
    // PO-approved: shared M16/M17 panel is vertically centred, not stretched.
    auto checkRailCenter = [&] {
        auto* railItem = qobject_cast<QQuickItem*>(sharedRail);
        auto* contentsItem = qobject_cast<QQuickItem*>(railContents);
        QVERIFY(railItem && contentsItem);
        const QPointF outerCenter = railItem->mapToScene(
            QPointF{railItem->width() / 2.0, railItem->height() / 2.0});
        const QPointF innerCenter = contentsItem->mapToScene(
            QPointF{contentsItem->width() / 2.0,
                    contentsItem->height() / 2.0});
        QVERIFY2(qAbs(outerCenter.y() - innerCenter.y()) <= 1.5,
                 "Shared True Peak information cards must be centred vertically");
        QVERIFY2(contentsItem->height() <= railItem->height() + 1.0,
                 "True Peak cards must not overflow the available rail height");
    };
    checkRailCenter();
    QCOMPARE(provenance->property("stateLabel").toString(),
             QStringLiteral("NO SOURCE"));
    QCOMPARE(provenance->property("qualification").toString(),
             QStringLiteral("NO QUALIFIED MEASUREMENT"));
    QCOMPARE(provenance->property("identity").toString(),
             QStringLiteral("NO PUBLISHED REALIZATION"));
    QVERIFY2(full->property("width").toDouble() > 100.0,
             "Readout must receive nonzero QML layout width");
    QVERIFY2(provenance->property("height").toDouble() > 0.0,
             "Provenance panel must not collapse");
    QCOMPARE(state.instances().size(), std::size_t{3});

    // B4c: first genuine native Qt captures. Grab authored-size QQuickItem
    // content; the CI virtual desktop must not clamp 1440x900 to its pixels.
    // NO SOURCE is intentional: no invented True Peak numerical reading.
    auto* captureSurface = qobject_cast<QQuickItem*>(
        find_child_by_name(componentRoot.data(),
                           QStringLiteral("m16VisualCaptureSurface")));
    QVERIFY(captureSurface);
    for (const auto size : {QSize{1184, 688}, QSize{1440, 900}}) {
        captureSurface->setWidth(size.width());
        captureSurface->setHeight(size.height());
        window->resize(size);
        QTest::qWait(120);
        QCoreApplication::processEvents();
        QCOMPARE(captureSurface->size(), QSizeF{size});
        checkRailCenter();
        QVERIFY2(check_item_contained_in_ancestor(
                     qobject_cast<QQuickItem*>(meter), captureSurface),
                 "The True Peak editor must fit the native capture scene");
        QVERIFY2(check_item_contained_in_ancestor(
                     qobject_cast<QQuickItem*>(staticBay),
                     qobject_cast<QQuickItem*>(meter)),
                 "The static meter well must remain inside M16 editor");
        QVERIFY2(check_item_contained_in_ancestor(
                     qobject_cast<QQuickItem*>(sharedRail),
                     qobject_cast<QQuickItem*>(meter)),
                 "The shared rail must remain inside M16 editor");
        const auto screenshot = captureSurface->grabToImage();
        QVERIFY2(screenshot, "M16 True Peak offscreen capture must start");
        QSignalSpy readySpy{screenshot.data(), &QQuickItemGrabResult::ready};
        if (readySpy.empty()) {
            QVERIFY2(readySpy.wait(15000),
                     "M16 offscreen scenegraph grab did not finish");
        }
        const QImage picture = screenshot->image();
        QVERIFY2(!picture.isNull(), "M16 offscreen screenshot cannot be empty");
        QCOMPARE(picture.size(), size);
        const QString evidenceDir =
            qEnvironmentVariable("RGSML_GUI01_EVIDENCE_DIR");
        if (!evidenceDir.isEmpty()) {
            QVERIFY(QDir{}.mkpath(evidenceDir));
            const QString fileName = QStringLiteral(
                "m16_true_peak_meter_b4e_centered_scale_nosource_exact_%1x%2.png")
                .arg(size.width()).arg(size.height());
            QVERIFY2(picture.save(QDir{evidenceDir}.filePath(fileName), "PNG"),
                     "Native M16 screenshot must be saved into CI artifact");
        }
    }

    // The editor owns a real, non-scrolling layout rather than a Flickable.
    auto* fixedContent = qobject_cast<QQuickItem*>(
        find_child_by_name(meter, QStringLiteral("truePeakContentLayout")));
    auto* meterItem = qobject_cast<QQuickItem*>(meter);
    QVERIFY2(fixedContent && meterItem, "True Peak must use a fixed-size layout");
    QVERIFY2(fixedContent->height() <= meterItem->height() + 1.0,
             "M16 content must not extend below its actual editor height");
    // Height-only resize regression: checks genuine vertical centring.
    captureSurface->setHeight(840);
    window->resize(QSize{1440, 840});
    QTest::qWait(100);
    QCoreApplication::processEvents();
    checkRailCenter();

    // Real, source-bound stereo case using the same production render and
    // complete-resource M16 analysis as the app. No values are hard-coded
    // into the QML fixture; live C2 remains genuinely UNAVAILABLE.
    auto sourceBuffer = audio::AudioBuffer::create(
        render_support::format(audio::ChannelLayout::STEREO_LR, 48000),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, render_support::frame_count(2048));
    QVERIFY(sourceBuffer);
    auto leftSamples = sourceBuffer.value()->mutable_view().channel(0);
    auto rightSamples = sourceBuffer.value()->mutable_view().channel(1);
    QVERIFY(leftSamples && rightSamples);
    for (std::int64_t n = 0; n < 2048; ++n) {
        (*leftSamples.value())[static_cast<std::size_t>(n)] =
            n == 11 ? 0.78 : 0.0;
        (*rightSamples.value())[static_cast<std::size_t>(n)] =
            n == 23 ? -0.42 : 0.0;
    }
    auto emptyChain = dsp::ProcessingChain::create(
        *registry.value(),
        {dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
    QVERIFY(emptyChain);
    auto renderRequest = render::RenderRequest::create(
        sourceBuffer.value()->view(), render_support::frame_range(0,2048),
        *emptyChain.value(), std::vector<dsp::ModuleExecutionBinding>{},
        render_support::frame_count(16));
    QVERIFY(renderRequest);
    auto renderOutcome = render::render_preview(
        *renderRequest.value(), *registry.value());
    QVERIFY(renderOutcome);
    auto published = std::make_shared<const render::RenderResult>(
        std::move(*renderOutcome.value()));
    auto sourceReference = core::ResourceReference::create(
        "test", "m16-b4d-real-pcm", true, false);
    QVERIFY(sourceReference);
    visualMeasurement.source_committed(
        *sourceReference.value(), app::TruePeakExpectedSource{
            render_support::frame_count(2048),
            *core::SampleRate::create(48000).value(),
            audio::ChannelLayout::STEREO_LR});
    visualMeasurement.processed_changed(
        published, core::RealizationId{4711});
    QTRY_VERIFY_WITH_TIMEOUT(
        visualMeasurement.state()
            == app::TruePeakStaticCoordinator::State::READY, 10000);
    QCoreApplication::processEvents();
    QCOMPARE(visualMeasurement.measured_channel_count(), 2);
    QVERIFY(!visualMeasurement.full_left_over_zero());
    QVERIFY(!visualMeasurement.full_right_over_zero());
    QVERIFY2(visualMeasurement.full_left_meter_level()
                 > visualMeasurement.full_right_meter_level(),
             "Real full-resource L/R peaks remain distinct and authoritative");
    // Static processing readiness is not evidence of current listening.
    QVERIFY(!leftLane->property("available").toBool());
    QVERIFY(!rightLane->property("available").toBool());
    QVERIFY(!leftLane->property("aboveDigitalZero").toBool());
    QVERIFY(!rightLane->property("aboveDigitalZero").toBool());
    QCOMPARE(leftLane->property("reading").toString(), QStringLiteral("—"));
    QCOMPARE(rightLane->property("reading").toString(), QStringLiteral("—"));
    QCOMPARE(leftLane->property("level").toDouble(), 0.0);
    QCOMPARE(rightLane->property("level").toDouble(), 0.0);
    QCOMPARE(full->property("reading").toString(),
             visualMeasurement.peak_text());
    QCOMPARE(recent->property("reading").toString(),
             QStringLiteral("UNAVAILABLE"));
    QCOMPARE(provenance->property("qualification").toString(),
             QStringLiteral("SUPPORTED · UNQUALIFIED"));

    for (const auto size : {QSize{1184, 688}, QSize{1440, 900}}) {
        captureSurface->setWidth(size.width());
        captureSurface->setHeight(size.height());
        window->resize(size);
        QTest::qWait(120);
        QCoreApplication::processEvents();
        checkRailCenter();
        const auto pictureResult = captureSurface->grabToImage();
        QVERIFY(pictureResult);
        QSignalSpy captureSpy{pictureResult.data(), &QQuickItemGrabResult::ready};
        if (captureSpy.empty()) {
            QVERIFY2(captureSpy.wait(15000),
                     "Real M16 per-channel image capture must finish");
        }
        const QImage realImage = pictureResult->image();
        QVERIFY(!realImage.isNull());
        QCOMPARE(realImage.size(), size);
        const auto evidenceDir =
            qEnvironmentVariable("RGSML_GUI01_EVIDENCE_DIR");
        if (!evidenceDir.isEmpty()) {
            QVERIFY(QDir{}.mkpath(evidenceDir));
            const auto fileName = QStringLiteral(
                "m16_true_peak_meter_b4e_centered_scale_measured_exact_%1x%2.png")
                .arg(size.width()).arg(size.height());
            QVERIFY2(realImage.save(QDir{evidenceDir}.filePath(fileName), "PNG"),
                     "M16 genuine full-resource evidence must be persisted");
        }
    }

    // Readout grammar: preserve contiguous minus/integer and fixed
    // decimal and unit positions when the integer grows to two digits.
    auto* laneNumeric = find_child_by_name(
        leftLane, QStringLiteral("truePeakLaneReading"));
    auto* cardNumeric = find_child_by_name(
        full, QStringLiteral("truePeakNumericValueText"));
    QVERIFY(laneNumeric && cardNumeric);
    // A LIVE lane must show no number without consumed playback proof.
    // The following test-only availability isolates typography from source
    // provenance; it does not publish synthetic LIVE evidence to the app.
    QVERIFY(!leftLane->property("available").toBool());
    QCOMPARE(laneNumeric->property("text").toString(), QStringLiteral("—"));
    QVERIFY(leftLane->setProperty("available", true));
    for (const auto reading : {
             QStringLiteral("-9.99 dBTP"), QStringLiteral("-10.00 dBTP"),
             QStringLiteral("9.99 dBTP"), QStringLiteral("10.00 dBTP")}) {
        QVERIFY(leftLane->setProperty("reading", reading));
        QVERIFY(full->setProperty("reading", reading));
        QCoreApplication::processEvents();
        for (auto* numeral : {laneNumeric, cardNumeric}) {
            const auto shown = numeral->property("text").toString();
            QCOMPARE(shown.indexOf(QLatin1Char('.')), 4);
            QCOMPARE(shown.indexOf(QStringLiteral("dBTP")), 9);
            QVERIFY(shown.contains(reading.left(reading.indexOf(QLatin1Char('.')))
                                   + QStringLiteral(".")));
        }
    }
    // Restore fail-closed LIVE semantics after the typography fixture.
    QVERIFY(leftLane->setProperty("available", false));
    QVERIFY(leftLane->setProperty("reading", QStringLiteral("—")));
    QCoreApplication::processEvents();
    QCOMPARE(laneNumeric->property("text").toString(), QStringLiteral("—"));

    // Switching to an actual processing module restores the original
    // host actions without changing the audio ProcessingChain.
    model.setSelectedIndex(1);
    QCoreApplication::processEvents();
    QVERIFY(!meter->property("visible").toBool());
    QVERIFY(controlsParent->property("visible").toBool());
    model.setSelectedIndex(3);
    QCoreApplication::processEvents();
    QVERIFY(meter->property("visible").toBool());
    QVERIFY(!controlsParent->property("visible").toBool());
    QCOMPARE(state.instances().size(), std::size_t{3});
    window->close();
}

void SourceMetadataPanelSmokeTest::stereoMsProductDefaultDockedStartup()
{
    // The actual NEW-product factory must publish a selectable, real fourth
    // DSP editor at startup (initial BYP), never a passive placeholder.
    // Historical three-module factories must remain exactly three nodes.
    const auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chainId = *core::Uuid::parse(
        "e5000000-0000-4000-8000-000000000001").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("e5000000-0000-4000-8000-000000000010").value()).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("e5000000-0000-4000-8000-000000000020").value()).value();
    const auto compId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("e5000000-0000-4000-8000-000000000030").value()).value();
    const auto stereoId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("e5000000-0000-4000-8000-000000000040").value()).value();

    auto productResult = app::MasteringChainState::create_product_default(
        *registry.value(), chainId, gainId, eqId, compId, stereoId);
    QVERIFY(productResult);
    auto product = std::move(*productResult.value());
    app::MasteringPreviewController preview{&product};
    app::GainViewModel gain{&product, &preview};
    app::EqViewModel eq{&product, &preview};
    app::CompressorViewModel compressor{&product, &preview};
    app::StereoMsViewModel stereo{&product, &preview};
    app::DspChainAdapterModel model{
        &gain, &eq, &compressor, &product,
        QStringLiteral("Mastering"), nullptr, &stereo};
    QCOMPARE(model.modules().size(), qsizetype{4});
    QCOMPARE(product.module_count(), std::size_t{4});
    const auto instances = product.instances();
    QCOMPARE(instances[0].module_type_id(), std::string_view("rgsml.dsp.gain"));
    QCOMPARE(instances[1].module_type_id(), std::string_view("rgsml.dsp.parametric-eq"));
    QCOMPARE(instances[2].module_type_id(), std::string_view("rgsml.dsp.compressor"));
    QCOMPARE(instances[3].module_type_id(), std::string_view("rgsml.dsp.stereo-ms"));
    QCOMPARE(instances[3].instance_id(), stereoId);
    const auto defaultParameters = dsp::StereoMsParameters::create_default();
    QVERIFY(defaultParameters);
    QCOMPARE(product.stereo_ms_parameters(), std::optional{*defaultParameters.value()});
    auto historical = app::MasteringChainState::create_default(
        *registry.value(), chainId, gainId, eqId, compId);
    QVERIFY(historical);
    QCOMPARE(historical.value()->module_count(), std::size_t{3});
    QVERIFY(!historical.value()->stereo_ms_instance_id());

    model.selectModuleByInstanceId(QString::fromStdString(stereoId.to_string()));
    QCOMPARE(model.selected_index(), 3);
    QVERIFY(model.active_module());
    QCOMPARE(model.active_module()->editor_content_key(), QStringLiteral("STEREO_MS"));
    QVERIFY(model.active_module()->bypass());

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(
        QStringLiteral("m15ProductAdapterModel"), &model);
    QQmlComponent component{&engine};
    static constexpr auto qml = R"qml(
import QtQuick
import QtQuick.Window
import Rgsml.Ui 1.0
Window {
    objectName: "m15ProductDockedWindow"
    width: 1184
    height: 688
    color: "#06121F"
    visible: false
    Row {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8
        DspChainSelector {
            id: realChain
            objectName: "m15ProductChainSelector"
            width: 180
            height: parent.height
            adapterModel: m15ProductAdapterModel
        }
        DspEditorHost {
            objectName: "m15ProductEditorHost"
            width: parent.width - 188
            height: parent.height
            adapterModel: m15ProductAdapterModel
            activeChainRow: realChain.activeRow
        }
    }
}
)qml";
    component.setData(QByteArray{qml},
        QUrl{QStringLiteral("qrc:/m15/product-default-docked.qml")});
    QVERIFY2(component.status() != QQmlComponent::Error,
        qPrintable(component.errorString()));
    QScopedPointer<QObject> created{component.create()};
    QVERIFY2(created != nullptr, qPrintable(component.errorString()));
    auto* window = qobject_cast<QQuickWindow*>(created.data());
    QVERIFY(window);
    window->show();
    QTRY_VERIFY_WITH_TIMEOUT(!window->grabWindow().isNull(), 5000);

    auto* chainRow = qobject_cast<QQuickItem*>(find_child_by_name(
        created.data(), QStringLiteral("dspChainRow_3")));

    auto* editor = qobject_cast<QQuickItem*>(find_child_by_name(
        created.data(), QStringLiteral("stereoMsEditor")));
    auto* host = qobject_cast<QQuickItem*>(find_child_by_name(
        created.data(), QStringLiteral("m15ProductEditorHost")));
    auto* controls = qobject_cast<QQuickItem*>(find_child_by_name(
        created.data(), QStringLiteral("stereoMsControlsRegion")));
    auto* activeButton = find_child_by_name(
        created.data(), QStringLiteral("abButtonActive"));
    auto* bypassButton = find_child_by_name(
        created.data(), QStringLiteral("abButtonBypass"));
    QVERIFY(chainRow && editor && host && controls
        && activeButton && bypassButton);
    QVERIFY(chainRow->isVisible());
    QVERIFY(editor->isVisible());
    QVERIFY(chainRow->property("isBypassed").toBool());
    QVERIFY(!find_child_by_name(created.data(), QStringLiteral("dspChainBypassBadge_3")));
    QVERIFY(bypassButton->property("selected").toBool());
    QVERIFY(!activeButton->property("selected").toBool());
    QVERIFY(!product.stereo_ms_instance_id().has_value()
        || *product.stereo_ms_instance_id() == stereoId);
    auto bypass = product.is_bypassed(stereoId);
    QVERIFY(bypass);
    QVERIFY(*bypass.value());
    QVERIFY2(capture_visual_evidence(window,
        QStringLiteral("m15_product_default_docked_byp_1184x688.png"),
        QSize{1184, 688}), "New product must render live M15 in initial BYP");

    // The same REAL typed DSP can be enabled and re-bypassed. Its controls
    // and host do not shift; parameters, IDs and three predecessors persist.
    QVERIFY2(!window->grabWindow().isNull(), "M15 baseline render required");
    const QRectF hostBefore{host->mapToScene(QPointF{}), host->size()};
    const QRectF controlsBefore{controls->mapToScene(QPointF{}), controls->size()};
    model.active_module()->setBypass(false);
    QCoreApplication::processEvents();
    QVERIFY2(!window->grabWindow().isNull(), "M15 ON render required");
    bypass = product.is_bypassed(stereoId);
    QVERIFY(bypass);
    QVERIFY(!*bypass.value());
    QVERIFY(!chainRow->property("isBypassed").toBool());
    QVERIFY(activeButton->property("selected").toBool());
    QVERIFY(!bypassButton->property("selected").toBool());
    QCOMPARE(QRectF(host->mapToScene(QPointF{}), host->size()), hostBefore);
    QCOMPARE(QRectF(controls->mapToScene(QPointF{}), controls->size()), controlsBefore);
    QVERIFY2(capture_visual_evidence(window,
        QStringLiteral("m15_product_default_docked_on_1184x688.png"),
        QSize{1184, 688}), "Product M15 ON must render without layout reflow");

    model.active_module()->setBypass(true);
    QCoreApplication::processEvents();
    QVERIFY2(!window->grabWindow().isNull(), "M15 restored BYP render required");
    bypass = product.is_bypassed(stereoId);
    QVERIFY(bypass);
    QVERIFY(*bypass.value());
    QVERIFY(chainRow->property("isBypassed").toBool());
    QVERIFY(!find_child_by_name(created.data(), QStringLiteral("dspChainBypassBadge_3")));
    QVERIFY(bypassButton->property("selected").toBool());
    QCOMPARE(product.stereo_ms_parameters(), std::optional{*defaultParameters.value()});
    QCOMPARE(product.instances()[3].instance_id(), stereoId);
    QCOMPARE(QRectF(host->mapToScene(QPointF{}), host->size()), hostBefore);
    QCOMPARE(QRectF(controls->mapToScene(QPointF{}), controls->size()), controlsBefore);
    window->close();
    QCoreApplication::processEvents();
}

void SourceMetadataPanelSmokeTest::stereoMsOptInVisualEvidence()
{
    // M15 isolated 4-node product chain: no startup/default project promotion.
    // This proves the REAL QML editor host can instantiate and render on Windows,
    // without claiming a dynamic telemetry source or .rgsml roundtrip.
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chainId = *core::Uuid::parse(
        "f1000000-0000-4000-8000-000000000001").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("f1000000-0000-4000-8000-000000000010").value()).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("f1000000-0000-4000-8000-000000000020").value()).value();
    const auto compId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("f1000000-0000-4000-8000-000000000030").value()).value();
    const auto stereoId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("f1000000-0000-4000-8000-000000000040").value()).value();
    auto parameters = dsp::StereoMsParameters::create_default();
    QVERIFY(parameters);
    auto stateResult = app::MasteringChainState::create_with_stereo_ms(
        *registry.value(), chainId, gainId, eqId, compId,
        stereoId, *parameters.value(), false);
    QVERIFY(stateResult);
    auto state = std::move(*stateResult.value());
    app::MasteringPreviewController preview{&state};
    app::GainViewModel gain{&state, &preview};
    app::EqViewModel eq{&state, &preview};
    app::CompressorViewModel compressor{&state, &preview};
    app::StereoMsViewModel stereo{&state, &preview};

    // Real prepared-source format is supplied explicitly for this isolated
    // fixture, never invented by production source/session initialization.
    stereo.setSignalFormat(48000.0, 2);
    QVERIFY(stereo.setDraftMonoBassMode(QStringLiteral("LR24")));
    QVERIFY(stereo.setDraftMonoBassCutoffHz(125.0));
    QVERIFY(stereo.setDraftLowBandWidthPercent(35.0));
    QVERIFY(stereo.setDraftWidthPercent(140.0));
    QVERIFY(stereo.commitDraft());
    QCOMPARE(stereo.width_response_status(), QStringLiteral("READY"));
    QCOMPARE(stereo.width_response_points().size(), qsizetype{129});

    // Real DSP/output-stage M/S data drive this visual fixture. No QML
    // fabricated points and no silent frame may illuminate the origin.
    auto dspChain = dsp::ProcessingChain::create(
        *registry.value(),
        {dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
    QVERIFY(dspChain);
    QVERIFY(dspChain.value()->add(stereoId, "rgsml.dsp.stereo-ms", 0));
    constexpr std::int64_t kFrames = 24'000;
    constexpr double kTwoPi = 6.2831853071795864769;
    std::vector<double> left(static_cast<std::size_t>(kFrames));
    std::vector<double> right(static_cast<std::size_t>(kFrames));
    for (std::int64_t f = 0; f < kFrames; ++f) {
        const double time = static_cast<double>(f) / 48'000.0;
        left[static_cast<std::size_t>(f)] =
            0.46 * std::sin(kTwoPi * 840.0 * time)
            + 0.13 * std::sin(kTwoPi * 110.0 * time);
        right[static_cast<std::size_t>(f)] =
            0.38 * std::sin(kTwoPi * 840.0 * (time - 31.0 / 48'000.0))
            + 0.18 * std::sin(kTwoPi * 250.0 * time);
    }
    using namespace render_support;
    auto input = make_buffer(audio::ChannelLayout::STEREO_LR, 0, left, right);
    QVERIFY(input);
    auto request = render::RenderRequest::create(
        input.value()->view(), frame_range(0, kFrames),
        *dspChain.value(),
        {dsp::ModuleExecutionBinding{stereoId, *state.stereo_ms_parameters()}},
        frame_count(127));
    QVERIFY(request);
    auto renderResult = render::render_preview(*request.value(), *registry.value());
    QVERIFY(renderResult);
    const core::RealizationId auditedId{902};
    QVERIFY(renderResult.value()->bind_stereo_ms_stage_output_realization_id(auditedId));
    const auto& stages = renderResult.value()->stereo_ms_stage_output_sidecars();
    QCOMPARE(stages.size(), std::size_t{1});
    QVERIFY(!stages.front().density_buckets.empty());
    std::uint64_t visibleSamples = 0;
    for (const auto& b : stages.front().density_buckets) {
        for (const auto n : b.occupancy) visibleSamples += n;
        visibleSamples -= b.zero_vector_count;
    }
    QVERIFY(visibleSamples > 0U);

    auto accepted = std::make_shared<const render::RenderResult>(
        std::move(*renderResult.value()));
    core::PlaybackSnapshot audible;
    audible.state = core::PlaybackState::PLAYING;
    audible.position = core::FrameIndex{0};
    audible.traversalSerial = 1;
    audible.audibleRealization.phase = core::AudibleHandoffPhase::NEW;
    audible.audibleRealization.realizationId = auditedId;
    bool auditionProcessed = true;
    stereo.setTelemetryProviders(
        [accepted, auditedId]() -> app::StereoMsViewModel::AcceptedRenderEvidence {
            return {accepted, auditedId};
        },
        [&audible]() {
            return core::Result<core::PlaybackSnapshot>::success(audible);
        },
        [&auditionProcessed] { return auditionProcessed; });
    QCOMPARE(stereo.telemetry_status(), QStringLiteral("ACTIVE"));
    QVERIFY(stereo.telemetry_density_buckets().isEmpty());
    audible.position = core::FrameIndex{kFrames};
    stereo.refreshTelemetry();
    QCOMPARE(stereo.telemetry_status(), QStringLiteral("ACTIVE"));
    QVERIFY(!stereo.telemetry_density_buckets().isEmpty());

    app::DspChainAdapterModel model{
        &gain, &eq, &compressor, &state,
        QStringLiteral("Mastering"), nullptr, &stereo};
    QCOMPARE(model.modules().size(), qsizetype{4});
    model.setSelectedIndex(3);
    QCOMPARE(model.active_module()->editor_content_key(), QStringLiteral("STEREO_MS"));

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("m15VisualAdapterModel"), &model);
    QQmlComponent component{&engine};
    static constexpr auto qml = R"qml(
import QtQuick
import QtQuick.Window
import Rgsml.Ui 1.0

Window {
    objectName: "m15StereoMsVisualWindow"
    width: 1184
    height: 688
    color: "#06121F"
    visible: false
    // This item is larger than the Windows CI virtual monitor. Capturing
    // its own scenegraph offscreen preserves authored logical resolution.
    Rectangle {
        objectName: "m15StereoMsExactCaptureSurface"
        width: 1184
        height: 688
        color: "#06121F"
        DspEditorHost {
            objectName: "m15StereoMsVisualHost"
            anchors.fill: parent
            anchors.margins: 18
            adapterModel: m15VisualAdapterModel
        }
    }
}
)qml";
    component.setData(QByteArray{qml}, QUrl{QStringLiteral("qrc:/m15/visual-window.qml")});
    QVERIFY2(component.status() != QQmlComponent::Error,
             qPrintable(component.errorString()));
    QScopedPointer<QObject> componentRoot{component.create()};
    QVERIFY2(componentRoot != nullptr, qPrintable(component.errorString()));
    auto* window = qobject_cast<QQuickWindow*>(componentRoot.data());
    QVERIFY(window);

    window->show();
    QTest::qWait(150);
    auto* captureSurface = qobject_cast<QQuickItem*>(
        find_child_by_name(componentRoot.data(),
                           QStringLiteral("m15StereoMsExactCaptureSurface")));
    QVERIFY2(captureSurface != nullptr, "Must find exact-size offscreen capture surface");
    auto* editor = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsEditor"));
    auto* curve = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsResponseCurve"));
    auto* widthHandle = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsWidthHandle"));
    auto* cutoffHandle = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsCutoffHandle"));
    auto* lowHandle = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsLowWidthHandle"));
    auto* dynamicWell = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsDynamicWell"));
    auto* title = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsGoniometerTitle"));
    auto* telemetryStatus = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsTelemetryStatus"));
    auto* axes = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsStaticAxes"));
    auto* midAxis = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsMidAxis"));
    auto* sideAxis = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsSideAxis"));
    auto* midLabel = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsMidAxisLabel"));
    auto* sideLabel = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsSideAxisLabel"));
    auto* cloud = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsGoniometerCloud"));
    auto* corrReadout = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsCorrelationReadout"));
    auto* sideLowReadout = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsSideLowReadout"));
    auto* widthControl = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsWidthControl"));
    auto* cutoffControl = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsCutoffControl"));
    auto* lowControl = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsLowWidthControl"));
    auto* midControl = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsMidGainControl"));
    auto* sideControl = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsSideGainControl"));
    auto* monoMode = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsMonoBassModeControls"));
    auto* monoLabel = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsMonoBassModeLabel"));
    auto* monoButtons = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsMonoBassModeButtons"));
    QVERIFY2(monoMode && monoLabel && monoButtons,
             "Mono Bass mode caption/buttons must be one vertically aligned control");
    QVERIFY2(widthControl && cutoffControl && lowControl && midControl && sideControl,
             "Every editable M15 numeric field must own a canonical slider pair");
    auto* widthSlider = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsWidthSlider"));
    auto* cutoffSlider = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsCutoffSlider"));
    auto* lowSlider = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsLowWidthSlider"));
    auto* midSlider = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsMidGainSlider"));
    auto* sideSlider = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsSideGainSlider"));
    auto* midField = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsMidGainField"));
    auto* sideField = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsSideGainField"));
    auto* widthField = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsWidthField"));
    auto* cutoffField = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsCutoffField"));
    auto* lowField = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsLowWidthField"));
    auto* telemetry = find_child_by_name(componentRoot.data(), QStringLiteral("stereoMsTelemetryPlaceholder"));
    QVERIFY2(editor && curve && widthHandle && cutoffHandle && lowHandle
             && dynamicWell && title && telemetryStatus && axes && cloud
             && midAxis && sideAxis && midLabel && sideLabel
             && corrReadout && sideLowReadout && widthSlider && cutoffSlider
             && lowSlider && midSlider && sideSlider && midField && sideField
             && widthField && cutoffField && lowField && telemetry,
             "M15 complete active/dynamic editor must load");
    QVERIFY(editor->property("visible").toBool());
    QVERIFY(curve->property("visible").toBool());
    QVERIFY(widthHandle->property("visible").toBool());
    QVERIFY(cutoffHandle->property("visible").toBool());
    QVERIFY(lowHandle->property("visible").toBool());
    QVERIFY2(cloud->property("visible").toBool(),
             "Real post-M15 Grid20 density must illuminate the cloud");
    QVERIFY2(cloud->property("occupiedCells").toInt() > 0,
             "No synthetic or exclusively silent density is accepted");
    QCOMPARE(cloud->property("renderStyle").toString(),
             QStringLiteral("SOFT_RADIAL_DENSITY"));
    // Secondary MID/SIDE controls have no default colored halo. Their
    // field/slider carriers use exact frozen DSP parameter ranges.
    QCOMPARE(midSlider->property("focusAccentOnly").toBool(), true);
    QCOMPARE(sideSlider->property("focusAccentOnly").toBool(), true);
    QCOMPARE(midSlider->property("hasSemanticAccent").toBool(), false);
    QCOMPARE(sideSlider->property("hasSemanticAccent").toBool(), false);
    QCOMPARE(midSlider->property("accentActive").toBool(), false);
    QCOMPARE(sideSlider->property("accentActive").toBool(), false);
    QCOMPARE(midSlider->property("from").toDouble(), -12.0);
    QCOMPARE(midSlider->property("to").toDouble(), 12.0);
    QCOMPARE(sideSlider->property("from").toDouble(), -24.0);
    QCOMPARE(sideSlider->property("to").toDouble(), 12.0);
    // M15 tooltip completion: every editable field and its companion slider
    // must expose a real, nonempty interaction hint without entering layout
    // flow or changing the accepted compact field/slider geometry.
    for (const auto& fieldAndSlider : {
             std::pair{widthField, widthSlider},
             std::pair{cutoffField, cutoffSlider},
             std::pair{lowField, lowSlider},
             std::pair{midField, midSlider},
             std::pair{sideField, sideSlider}}) {
        const auto fieldHint = fieldAndSlider.first->property("interactionHint").toString();
        const auto sliderHint = fieldAndSlider.second->property("interactionHint").toString();
        QVERIFY2(!fieldHint.trimmed().isEmpty(),
                 "Each M15 numeric field needs an actionable tooltip");
        QVERIFY2(!sliderHint.trimmed().isEmpty(),
                 "Each M15 slider needs an actionable tooltip");
    }
    // Reuse Compressor MAKE-UP's existing bipolar fillFromOrigin
    // contract. Both tracks are filled from ZERO, at 38% opacity.
    // The asymmetrical -24..+12 SIDE range means zero is exactly
    // two thirds along the track, not visually centered.
    for (auto* slider : {midSlider, sideSlider}) {
        QVERIFY(slider->property("fillFromOrigin").toBool());
        QCOMPARE(slider->property("positionFillOrigin").toDouble(), 0.0);
        const QColor fill = slider->property("positionFillColor").value<QColor>();
        QVERIFY(fill.isValid());
        QVERIFY(std::abs(fill.alphaF() - 0.38) <= 0.006);
    }
    const QColor midFill = midSlider->property("positionFillColor").value<QColor>();
    const QColor sideFill = sideSlider->property("positionFillColor").value<QColor>();
    QCOMPARE(midFill.red(), 0x4A);
    QCOMPARE(midFill.green(), 0x9A);
    QCOMPARE(midFill.blue(), 0x88);
    QCOMPARE(sideFill.red(), 0x65);
    QCOMPARE(sideFill.green(), 0x9C);
    QCOMPARE(sideFill.blue(), 0xAF);
    const auto zeroNorm = [](QObject* slider) {
        const double from = slider->property("from").toDouble();
        const double to = slider->property("to").toDouble();
        return (0.0 - from) / (to - from);
    };
    QVERIFY(std::abs(zeroNorm(midSlider) - 0.5) < 1e-12);
    QVERIFY(std::abs(zeroNorm(sideSlider) - 2.0 / 3.0) < 1e-12);
    const auto checkMidFill = [&]() {
        return bipolar_fill_matches_handle(
            midSlider, QStringLiteral("studioSliderTrack"),
            QStringLiteral("studioSliderTrackFill"),
            QStringLiteral("studioSliderThumb"), 0.5);
    };
    const auto checkSideFill = [&]() {
        return bipolar_fill_matches_handle(
            sideSlider, QStringLiteral("studioSliderTrack"),
            QStringLiteral("studioSliderTrackFill"),
            QStringLiteral("studioSliderThumb"), 2.0 / 3.0);
    };
    QVERIFY2(checkMidFill() && checkSideFill(),
             "MID/SIDE zero-origin fills must terminate at white knob centers");
    QCOMPARE(midLabel->property("text").toString(), QStringLiteral("M"));
    QCOMPARE(sideLabel->property("text").toString(), QStringLiteral("S"));
    QCOMPARE(telemetryStatus->property("text").toString(),
             QStringLiteral("ACTIVE · POST M/S OUTPUT"));

    // The M/S axes must remain square at compact and taller window sizes.
    for (const auto size : {QSize{1184, 688}, QSize{1440, 900}}) {
        // The native window may be clamped by Hyper-V (1044-px desktop).
        // The item's OWN dimensions, not a scaled output target, determine
        // the scene layout and image size for this offscreen grab.
        captureSurface->setWidth(size.width());
        captureSurface->setHeight(size.height());
        window->resize(size);
        QTest::qWait(120);
        QCoreApplication::processEvents();
        QCOMPARE(captureSurface->size(), QSizeF{size});
        const auto side = axes->property("width").toDouble();
        QVERIFY(side > 50);
        QVERIFY(std::abs(side - axes->property("height").toDouble()) <= 0.5);
        // Orthogonal strokes stop equally short at both ends; neither
        // letter is printed over its stroke. Diagonals remain unchanged.
        const double inset = axes->property("orthogonalInsetPx").toDouble();
        const double vStart = midAxis->property("y").toDouble();
        const double vEnd = vStart + midAxis->property("height").toDouble();
        const double hStart = sideAxis->property("x").toDouble();
        const double hEnd = hStart + sideAxis->property("width").toDouble();
        QVERIFY2(inset >= 20.0, "Labels require deliberate axis clearance");
        QVERIFY(std::abs(vStart - inset) <= 0.5);
        QVERIFY(std::abs((side - vEnd) - inset) <= 0.5);
        QVERIFY(std::abs(hStart - inset) <= 0.5);
        QVERIFY(std::abs((side - hEnd) - inset) <= 0.5);
        QVERIFY2(vStart > midLabel->property("height").toDouble() + 3.0,
                 "Vertical M line must end before the M label");
        QVERIFY2(side - hEnd > sideLabel->property("width").toDouble() + 3.0,
                 "Horizontal S line must end before the S label");
        const double bandTop = title->property("y").toDouble()
            + title->property("height").toDouble() + 8.0;
        const double bandBottom = telemetryStatus->property("y").toDouble() - 8.0;
        const double axesTop = axes->property("y").toDouble();
        const double axesBottom = axesTop + axes->property("height").toDouble();
        QVERIFY2(axesTop >= bandTop - 1.0, "Goniometer must clear its fixed header");
        QVERIFY2(axesBottom <= bandBottom + 1.0, "Goniometer must clear footer telemetry");
        QVERIFY2(std::abs((axesTop - bandTop) - (bandBottom - axesBottom)) <= 1.0,
                 "Goniometer must be vertically centered between header and footer");
        QCOMPARE(title->property("y").toDouble(), 8.0);
        QVERIFY(qobject_cast<QQuickItem*>(editor)->height() > 250);
        // The Mono Bass group and MID/SIDE gain captions have exactly
        // the same top alignment, and the selector buttons line up
        // vertically with the numeric input rectangles at BOTH sizes.
        auto* editorItemForControls = qobject_cast<QQuickItem*>(editor);
        auto* monoLabelItem = qobject_cast<QQuickItem*>(monoLabel);
        auto* monoButtonsItem = qobject_cast<QQuickItem*>(monoButtons);
        auto* midControlItem = qobject_cast<QQuickItem*>(midControl);
        auto* sideControlItem = qobject_cast<QQuickItem*>(sideControl);
        auto* midFieldItem = qobject_cast<QQuickItem*>(midField);
        auto* sideFieldItem = qobject_cast<QQuickItem*>(sideField);
        QVERIFY(editorItemForControls && monoLabelItem && monoButtonsItem &&
                midControlItem && sideControlItem && midFieldItem && sideFieldItem);
        const auto topInEditor = [editorItemForControls](QQuickItem* item) {
            return item->mapToItem(editorItemForControls, QPointF{}).y();
        };
        QVERIFY(std::abs(topInEditor(midControlItem) -
                         topInEditor(sideControlItem)) <= 0.5);
        QVERIFY(std::abs(topInEditor(monoLabelItem) -
                         topInEditor(midControlItem)) <= 0.5);
        QVERIFY(std::abs(topInEditor(monoLabelItem) -
                         topInEditor(sideControlItem)) <= 0.5);
        // Numeric field: 14 px caption then 2 px gap before the box.
        // Mono selector buttons begin at that same height beneath the caption.
        QVERIFY(std::abs(topInEditor(monoButtonsItem) -
                         (topInEditor(midFieldItem) + 16.0)) <= 1.0);
        QVERIFY(std::abs(topInEditor(monoButtonsItem) -
                         (topInEditor(sideFieldItem) + 16.0)) <= 1.0);
        // In published editors the slider track spans precisely its
        // numeric rectangle; the unit suffix is outside this span.
        // Check both endpoints at each native logical window size.
        for (const auto& fieldAndSlider : {
                 std::pair{widthField, widthSlider},
                 std::pair{cutoffField, cutoffSlider},
                 std::pair{lowField, lowSlider},
                 std::pair{midField, midSlider},
                 std::pair{sideField, sideSlider}}) {
            auto* box = qobject_cast<QQuickItem*>(fieldAndSlider.first);
            auto* slider = qobject_cast<QQuickItem*>(fieldAndSlider.second);
            QVERIFY(box && slider);
            const double numericBoxWidth =
                fieldAndSlider.first->property("fieldWidth").toDouble();
            QCOMPARE(numericBoxWidth, 86.0);
            QVERIFY(std::abs(slider->width() - numericBoxWidth) <= 0.5);
            const QPointF boxOrigin = box->mapToItem(
                qobject_cast<QQuickItem*>(editor), QPointF{});
            const QPointF sliderOrigin = slider->mapToItem(
                qobject_cast<QQuickItem*>(editor), QPointF{});
            QVERIFY2(std::abs(boxOrigin.x() - sliderOrigin.x()) <= 0.5,
                     "Slider START must coincide with numeric box left edge");
            QVERIFY2(std::abs(boxOrigin.x() + numericBoxWidth
                              - sliderOrigin.x() - slider->width()) <= 0.5,
                     "Slider END must coincide with numeric box right edge");
        }
        auto* dynamicItem = qobject_cast<QQuickItem*>(dynamicWell);
        auto* editorItem = qobject_cast<QQuickItem*>(editor);
        QVERIFY(check_item_contained_in_ancestor(editorItem, captureSurface));
        QVERIFY(check_item_contained_in_ancestor(dynamicItem, editorItem));

        // Keep the original native-window screenshot for comparison; its
        // actual resolution may be clamped and is intentionally NOT asserted.
        QVERIFY(capture_visual_evidence(
            window,
            QStringLiteral("m15_stereo_ms_%1x%2_lr24_width140.png")
                .arg(size.width()).arg(size.height()),
            size));

        // Qt renders the QQuickItem subtree into a distinct full-size
        // offscreen scenegraph texture, independently of desktop geometry.
        const auto imageResult = captureSurface->grabToImage();
        QVERIFY2(imageResult, "Qt offscreen scenegraph grab failed");
        QSignalSpy readySpy{imageResult.data(), &QQuickItemGrabResult::ready};
        QTRY_VERIFY_WITH_TIMEOUT(readySpy.size() > 0, 15000);
        const QImage image = imageResult->image();
        QVERIFY2(!image.isNull(), "Offscreen rendering yielded an empty image");
        QCOMPARE(image.size(), size);
        const QString evidenceDirectory = qEnvironmentVariable("RGSML_GUI01_EVIDENCE_DIR");
        if (!evidenceDirectory.isEmpty()) {
            QVERIFY(QDir{}.mkpath(evidenceDirectory));
            const QString fileName =
                QStringLiteral("m15_stereo_ms_exact_item_%1x%2_lr24_width140.png")
                    .arg(size.width()).arg(size.height());
            QVERIFY2(image.save(QDir{evidenceDirectory}.filePath(fileName), "PNG"),
                     "Failed to write exact-size Qt offscreen screenshot");
        }
    }

    // Frozen audible history: a pause must neither clear nor animate the
    // completed post-M15 Grid20 cloud. The rendered cloud is dimmed while
    // its underlying occupancy and metrics remain unchanged.
    const auto heardAtPause = stereo.telemetry_density_buckets();
    QVERIFY(!heardAtPause.isEmpty());
    audible.state = core::PlaybackState::PAUSED;
    stereo.refreshTelemetry();
    QCOMPARE(stereo.telemetry_status(), QStringLiteral("PAUSED"));
    QVERIFY(!stereo.telemetry_active());
    QCOMPARE(stereo.telemetry_density_buckets(), heardAtPause);
    QCoreApplication::processEvents();
    QVERIFY(cloud->property("visible").toBool());
    QCOMPARE(cloud->property("opacity").toDouble(), 0.42);
    QVERIFY(cloud->property("occupiedCells").toInt() > 0);
    QCOMPARE(telemetryStatus->property("text").toString(),
             QStringLiteral("PAUSED"));

    // Take an offscreen authored-size frozen-state evidence, not a
    // desktop-dependent screenshot that could be clipped by virtual pixels.
    captureSurface->setWidth(1184);
    captureSurface->setHeight(688);
    window->resize(QSize{1184, 688});
    QCoreApplication::processEvents();
    const auto captureState = [&](const QString& fileName) {
        const auto grabbed = captureSurface->grabToImage();
        if (!grabbed) return false;
        QSignalSpy spy{grabbed.data(), &QQuickItemGrabResult::ready};
        if (spy.empty() && !spy.wait(15000)) return false;
        const QImage image = grabbed->image();
        if (image.isNull() || image.size() != QSize{1184, 688})
            return false;
        const QString dir = qEnvironmentVariable("RGSML_GUI01_EVIDENCE_DIR");
        return dir.isEmpty() || (QDir{}.mkpath(dir)
             && image.save(QDir{dir}.filePath(fileName), "PNG"));
    };
    QVERIFY2(captureState(QStringLiteral(
                "m15_stereo_ms_exact_item_1184x688_paused.png")),
             "PAUSED real-cloud evidence must be captured");

    // STOP must make historical data non-live and invisible without
    // silently presenting old buckets as a currently heard cloud.
    audible.state = core::PlaybackState::STOPPED;
    stereo.refreshTelemetry();
    QCOMPARE(stereo.telemetry_status(), QStringLiteral("STOPPED / END"));
    QCoreApplication::processEvents();
    QVERIFY(!cloud->property("visible").toBool());
    QCOMPARE(cloud->property("occupiedCells").toInt(), 0);

    // PREPARED / GOLD selections always suppress M15, even if a
    // previously Processed realization remains cached and available.
    auditionProcessed = false;
    stereo.refreshTelemetry();
    QCOMPARE(stereo.telemetry_status(), QStringLiteral("NOT AUDITIONED"));
    QVERIFY(stereo.telemetry_density_buckets().isEmpty());
    QVERIFY(stereo.telemetry_correlation().isEmpty());
    QVERIFY(stereo.telemetry_side_low().isEmpty());
    QCoreApplication::processEvents();
    QVERIFY(!cloud->property("visible").toBool());
    QCOMPARE(cloud->property("occupiedCells").toInt(), 0);
    QVERIFY2(captureState(QStringLiteral(
                "m15_stereo_ms_exact_item_1184x688_not_auditioned.png")),
             "NOT AUDITIONED must not display a phantom M/S cloud");

    // True bipolar gain slider travel at both polarities; visuals are
    // explicitly NOT AUDITIONED, not confused with an audible result.
    // Draft changes must NEVER mutate the committed DSP parameters.
    const auto beforeGainPreview = state.stereo_ms_parameters();
    QVERIFY(beforeGainPreview);
    QVERIFY(stereo.setDraftMidGainDb(-6.0));
    QVERIFY(stereo.setDraftSideGainDb(6.0));
    QCoreApplication::processEvents();
    QCOMPARE(midSlider->property("value").toDouble(), -6.0);
    QCOMPARE(sideSlider->property("value").toDouble(), 6.0);
    QVERIFY2(checkMidFill() && checkSideFill(),
             "MID negative and SIDE positive fills must reach the handles");
    QVERIFY2(captureState(QStringLiteral(
                "m15_bipolar_gain_mid_minus6_side_plus6_1184x688.png")),
             "Bipolar MID-/SIDE+ filled-track evidence missing");
    QVERIFY(stereo.setDraftMidGainDb(6.0));
    QVERIFY(stereo.setDraftSideGainDb(-12.0));
    QCoreApplication::processEvents();
    QCOMPARE(midSlider->property("value").toDouble(), 6.0);
    QCOMPARE(sideSlider->property("value").toDouble(), -12.0);
    QVERIFY2(checkMidFill() && checkSideFill(),
             "MID positive and SIDE negative fills must reach the handles");
    QVERIFY2(captureState(QStringLiteral(
                "m15_bipolar_gain_mid_plus6_side_minus12_1184x688.png")),
             "Bipolar MID+/SIDE- filled-track evidence missing");
    QCOMPARE(state.stereo_ms_parameters(), beforeGainPreview);
    stereo.cancelDraft();
    QCoreApplication::processEvents();

    // Gate S: stored OFF-mode numbers remain visible but cannot be edited.
    QVERIFY(stereo.setDraftMonoBassMode(QStringLiteral("OFF")));
    QCoreApplication::processEvents();
    QVERIFY(widthControl->property("enabled").toBool());
    QVERIFY(!cutoffControl->property("enabled").toBool());
    QVERIFY(!lowControl->property("enabled").toBool());
    QCOMPARE(stereo.mono_bass_cutoff_hz(), 125.0);
    QCOMPARE(stereo.low_band_width_percent(), 35.0);
    QVERIFY2(captureState(QStringLiteral(
                "m15_gate_s_1184x688_mono_bass_off.png")),
             "Gate S OFF must provide PO-visible disabled-control evidence");
    stereo.cancelDraft();
    QCoreApplication::processEvents();
    QVERIFY(cutoffControl->property("enabled").toBool());
    QVERIFY(lowControl->property("enabled").toBool());

    // Mono and bypass must NEVER display stale active response points.
    stereo.setSignalFormat(48000.0, 1);
    QCoreApplication::processEvents();
    QVERIFY(!curve->property("visible").toBool());
    QVERIFY(!widthHandle->property("visible").toBool());
    for (auto* c : {widthControl, cutoffControl, lowControl, midControl, sideControl})
        QVERIFY(!c->property("enabled").toBool());
    QVERIFY2(captureState(QStringLiteral(
                "m15_gate_s_1184x688_mono_input.png")),
             "Gate S mono must show all M/S editing controls non-effective");
    stereo.setSignalFormat(48000.0, 2);
    stereo.setBypass(true);
    QCoreApplication::processEvents();
    QVERIFY(!curve->property("visible").toBool());
    for (auto* c : {widthControl, cutoffControl, lowControl, midControl, sideControl})
        QVERIFY(!c->property("enabled").toBool());
    QVERIFY2(captureState(QStringLiteral(
                "m15_gate_s_1184x688_bypassed.png")),
             "Gate S bypass must show disabled spatial controls");
    stereo.setBypass(false);
    QCoreApplication::processEvents();
    QVERIFY(widthControl->property("enabled").toBool());

    // Exercise Compressor through the SAME native DspEditorHost at exact
    // scenegraph dimensions; no second fake QML surface or product change.
    model.setSelectedIndex(2);
    QCoreApplication::processEvents();
    auto* controlsPanel = find_child_by_name(
        componentRoot.data(), QStringLiteral("compressorControlsPanel"));
    auto* controlsViewport = find_child_by_name(
        componentRoot.data(), QStringLiteral("compressorControlsViewport"));
    auto* controlsContent = find_child_by_name(
        componentRoot.data(), QStringLiteral("compressorControlsContent"));
    auto* compressorEditor = find_child_by_name(
        componentRoot.data(), QStringLiteral("compressorEditor"));
    QVERIFY2(controlsPanel && controlsViewport && controlsContent && compressorEditor,
             "Compressor responsive controls must load in the shared host");
    QVERIFY(compressorEditor->property("visible").toBool());
    double compactStackY = 0.0;
    for (const QSize size : {QSize{1184, 688}, QSize{1440, 900}}) {
        captureSurface->setWidth(size.width());
        captureSurface->setHeight(size.height());
        window->resize(size);
        QTest::qWait(130);
        QCoreApplication::processEvents();
        QCOMPARE(captureSurface->size(), QSizeF{size});
        const double viewportHeight =
            controlsViewport->property("availableHeight").toDouble();
        const double contentHeight =
            controlsContent->property("implicitHeight").toDouble();
        const double contentY = controlsContent->property("y").toDouble();
        const double surplus = viewportHeight - contentHeight;
        QVERIFY(contentHeight > 100.0);
        QVERIFY(viewportHeight > 100.0);
        QVERIFY(std::abs(contentY - std::max(0.0, surplus / 2.0)) <= 1.0);
        if (surplus <= 0.0) {
            QVERIFY(std::abs(contentY) <= 1.0);
        } else {
            QVERIFY(std::abs(contentY - (viewportHeight
                        - (contentY + contentHeight))) <= 1.0);
        }
        if (size.width() == 1184) compactStackY = contentY;
        else QVERIFY(contentY >= compactStackY - 1.0);
        QVERIFY(check_item_contained_in_ancestor(
            qobject_cast<QQuickItem*>(compressorEditor), captureSurface));
        QVERIFY(check_item_contained_in_ancestor(
            qobject_cast<QQuickItem*>(controlsPanel),
            qobject_cast<QQuickItem*>(compressorEditor)));

        const auto result = captureSurface->grabToImage();
        QVERIFY2(result, "Compressor Qt scenegraph capture failed");
        QSignalSpy readySpy{result.data(), &QQuickItemGrabResult::ready};
        QTRY_VERIFY_WITH_TIMEOUT(readySpy.size() > 0, 15000);
        const QImage image = result->image();
        QVERIFY(!image.isNull());
        QCOMPARE(image.size(), size);
        const QString evidenceDirectory =
            qEnvironmentVariable("RGSML_GUI01_EVIDENCE_DIR");
        if (!evidenceDirectory.isEmpty()) {
            QVERIFY(QDir{}.mkpath(evidenceDirectory));
            QVERIFY(image.save(QDir{evidenceDirectory}.filePath(
                QStringLiteral("m15_compressor_centered_exact_%1x%2.png")
                    .arg(size.width()).arg(size.height())), "PNG"));
        }
    }
    window->close();
    QCoreApplication::processEvents();
}

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
    compressorViewModel.setTelemetryProviders(
        [&auditionSelector]()
            -> std::shared_ptr<const render::CompressorTelemetrySidecar> {
            const auto processed =
                auditionSelector.processed_realization_snapshot();
            if (processed == nullptr) {
                return {};
            }
            const auto& sidecar =
                processed->compressor_telemetry_sidecar();
            if (!sidecar.has_value() || !sidecar->valid) {
                return {};
            }
            return std::shared_ptr<const render::CompressorTelemetrySidecar>(
                processed, &*sidecar);
        },
        [&playbackTransport]() {
            return playbackTransport.playback_snapshot();
        },
        [&auditionSelector]() {
            const auto target = auditionSelector.active_target();
            if (!target.has_value()) {
                return render::AuditionTarget::PREPARED;
            }
            switch (*target) {
            case app::AuditionTarget::PREPARED:
                return render::AuditionTarget::PREPARED;
            case app::AuditionTarget::PROCESSED:
                return render::AuditionTarget::PROCESSED;
            case app::AuditionTarget::GOLD:
                return render::AuditionTarget::GOLD;
            }
            return render::AuditionTarget::PREPARED;
        });
    app::StereoMsViewModel stereoMsViewModel{&masteringChainState, &previewController};
    app::DspChainAdapterModel dspChainAdapterModel{
        &gainViewModel, &eqViewModel, &compressorViewModel,
        &masteringChainState, QStringLiteral("Mastering"),
        nullptr, &stereoMsViewModel};

    app::GoldSelectionViewModel goldSelection{&auditionSelector};
    app::ProjectSessionViewModel projectSession{
        &model, &goldSelection, &auditionRegion, &playbackTransport};
    analysis::LiveSpectrumAnalyzer spectrumAnalyzer;
    spectrumAnalyzer.start();
    app::LiveSpectrumViewModel liveSpectrumVM{&spectrumAnalyzer};
    playbackTransport.set_pcm_prepare_handler(
        [observedPlayback](
            audio::AudioBufferView view,
            std::shared_ptr<const void>,
            std::optional<core::RealizationId>) {
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
#ifdef RGSML_BUILD_PROVENANCE
    engine.rootContext()->setContextProperty(
        QStringLiteral("buildProvenance"), QStringLiteral(RGSML_BUILD_PROVENANCE));
#endif
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
    auto* headerAppIcon = root->findChild<QObject*>(QStringLiteral("headerAppIcon"));
    QVERIFY2(headerAppIcon != nullptr, "headerAppIcon must exist in applicationHeader");
    QCOMPARE(headerAppIcon->property("width").toInt(), 40);
    QCOMPARE(headerAppIcon->property("height").toInt(), 40);
    auto* desktopMenu = root->findChild<QObject*>(QStringLiteral("desktopMenuBar"));
    QVERIFY(desktopMenu);
    QVERIFY(root->findChild<QObject*>(QStringLiteral("controlStripElasticCenter")));
    auto* statusBar = root->findChild<QObject*>(QStringLiteral("statusBar"));
    auto* statusBarMessage = root->findChild<QObject*>(QStringLiteral("statusBarMessage"));
    auto* statusIndicator = root->findChild<QObject*>(QStringLiteral("statusReadyIndicator"));
    auto* statusBuildProvenanceLabel = root->findChild<QObject*>(QStringLiteral("statusBuildProvenanceLabel"));
    QVERIFY(statusBar && statusBarMessage && statusIndicator && statusBuildProvenanceLabel);
    QCOMPARE(root->property("statusSeverity").toString(), QStringLiteral("INFO"));
    QCOMPARE(statusBar->property("height").toInt(), 24);
    QCOMPARE(statusIndicator->property("width").toInt(), 8);
    QCOMPARE(statusIndicator->property("height").toInt(), 8);
    QCOMPARE(statusIndicator->property("color").value<QColor>(), QColor{QStringLiteral("#00E6E6")});
    QVERIFY2(!statusBuildProvenanceLabel->property("text").toString().isEmpty(), "statusBuildProvenanceLabel text must be non-empty");

    // Verify Docked DSP Architecture components exist
    auto* dspWorkspaceObj = root->findChild<QObject*>(QStringLiteral("dspWorkspace"));
    auto* dspChainSelectorObj = root->findChild<QObject*>(QStringLiteral("dspChainSelector"));
    auto* dspEditorHostObj = root->findChild<QObject*>(QStringLiteral("dspEditorHost"));
    QVERIFY2(dspWorkspaceObj != nullptr, "dspWorkspace must exist in Main");
    QVERIFY2(dspChainSelectorObj != nullptr, "dspChainSelector must exist in dspWorkspace");
    QVERIFY2(dspEditorHostObj != nullptr, "dspEditorHost must exist in dspWorkspace");

    // REAL Main-window proof: the selected Input Gain editor is rehosted,
    // never cloned. A second selected DSP returns to the existing dock.
    auto* nativeDockedEditor=qobject_cast<QQuickItem*>(dspEditorHostObj);
    auto* floatingModule=qobject_cast<QQuickWindow*>(
        find_child_by_name(root,QStringLiteral("dspFloatingModuleWindow")));
    QVERIFY2(nativeDockedEditor && floatingModule,
        "Generic docking window must be registered in the real Main QML");
    auto* dockGrip=qobject_cast<QQuickItem*>(
        find_child_by_name(dspEditorHostObj,
                           QStringLiteral("dspHostTearOffGrip")));
    auto* floatingGrip=qobject_cast<QQuickItem*>(
        find_child_by_name(root,QStringLiteral("dspFloatingDragGrip")));
    auto* dropWell=qobject_cast<QQuickItem*>(
        find_child_by_name(root,QStringLiteral("compactEditorDockDropZone")));
    QVERIFY2(dockGrip && floatingGrip && dropWell,
             "Both drag grips and the real Main fixed dock target are required");
    QCOMPARE(dockGrip->width(),22.0);
    // A hidden native QQuickWindow need not have polished its RowLayout.
    // Assert its actual grip hitbox AFTER the real Window becomes visible.
    const auto* originalEditorIdentity=nativeDockedEditor;
    QVERIFY(!root->property("editorDetached").toBool());
    QVERIFY(QMetaObject::invokeMethod(root,"detachSelectedModule"));
    QTRY_VERIFY_WITH_TIMEOUT(root->property("editorDetached").toBool(),5000);
    QTRY_VERIFY_WITH_TIMEOUT(floatingModule->isVisible(),5000);
    QTRY_VERIFY_WITH_TIMEOUT(nativeDockedEditor->window()==floatingModule,5000);
    QTRY_COMPARE_WITH_TIMEOUT(floatingGrip->width(),28.0,5000);
    QTRY_COMPARE_WITH_TIMEOUT(floatingGrip->height(),32.0,5000);
    QCOMPARE(floatingGrip->window(),floatingModule);
    QCOMPARE(nativeDockedEditor,originalEditorIdentity);
    QVERIFY(!root->property("detachedModuleInstanceId").toString().isEmpty());
    QVERIFY(QMetaObject::invokeMethod(floatingModule,"returnToDock"));
    QTRY_VERIFY_WITH_TIMEOUT(!root->property("editorDetached").toBool(),5000);
    QTRY_VERIFY_WITH_TIMEOUT(!floatingModule->isVisible(),5000);
    QTRY_VERIFY_WITH_TIMEOUT(nativeDockedEditor->window()==window,5000);
    QVERIFY(nativeDockedEditor==originalEditorIdentity);
    QVERIFY(QMetaObject::invokeMethod(root,"detachSelectedModule"));
    QTRY_VERIFY_WITH_TIMEOUT(nativeDockedEditor->window()==floatingModule,5000);
    QTRY_COMPARE_WITH_TIMEOUT(floatingGrip->width(),28.0,5000);
    QTRY_COMPARE_WITH_TIMEOUT(floatingGrip->height(),32.0,5000);
    QVERIFY(QMetaObject::invokeMethod(root,"returnCurrentEditorToDock"));
    QTRY_VERIFY_WITH_TIMEOUT(nativeDockedEditor->window()==window,5000);
    QVERIFY(nativeDockedEditor==originalEditorIdentity);
    // Evidence MUST come from this actual Main QQuickWindow, never an
    // illustration. Docked editor, genuine family icons and 22px grip
    // are visible together at the PO minimum 1184x688 geometry.
    QVERIFY(!root->property("editorDetached").toBool());
    QVERIFY(dockGrip->isVisible());
    QVERIFY2(capture_visual_evidence(
        window,QStringLiteral("fq80_real_main_chain_family_icons_docked_grip_1184x688.png"),
        QSize{1184,688}),"FQ80 real Main docked family-icon visual evidence");

    // Verify global screen-coordinate drop admission at the ACTUAL dock
    // rather than fabricating a QML DropArea across native Qt windows.
    QVERIFY(QMetaObject::invokeMethod(root,"detachSelectedModule"));
    QTRY_VERIFY_WITH_TIMEOUT(nativeDockedEditor->window()==floatingModule,5000);
    const auto dropPoint=dropWell->mapToGlobal(
        QPointF{dropWell->width()*0.5,dropWell->height()*0.5});
    auto evaluateDockAction=[&](const QString& script)->QVariant {
        QQmlExpression expression{engine.rootContext(),root,script};
        const auto result=expression.evaluate();
        if(expression.hasError()) {
            qWarning().noquote()<<"Dock Qt expression:"<<expression.error().toString();
        }
        if(expression.hasError())return QVariant{};
        return result;
    };
    const auto dockArgs=QStringLiteral("%1,%2").arg(
        QString::number(dropPoint.x(),'f',3),
        QString::number(dropPoint.y(),'f',3));
    QVERIFY(evaluateDockAction(
        QStringLiteral("pointInsideEditorDock(")+dockArgs
            +QStringLiteral(")")).toBool());
    evaluateDockAction(QStringLiteral("beginDockDrag(")+dockArgs
        +QStringLiteral(")"));
    QVERIFY(root->property("dockDropCandidate").toBool());
    evaluateDockAction(QStringLiteral("finishDockDrag(-50,-50)"));
    QVERIFY(root->property("editorDetached").toBool());
    QVERIFY(!root->property("dockDropCandidate").toBool());
    evaluateDockAction(QStringLiteral("finishDockDrag(")+dockArgs
        +QStringLiteral(")"));
    QTRY_VERIFY_WITH_TIMEOUT(!root->property("editorDetached").toBool(),5000);
    QTRY_VERIFY_WITH_TIMEOUT(nativeDockedEditor->window()==window,5000);
    QVERIFY(nativeDockedEditor==originalEditorIdentity);

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
    QCOMPARE(playPause->property("width").toInt(), 56);
    QCOMPARE(playPause->property("height").toInt(), 56);
    const int focusEnvelope = playPause->property("width").toInt() + playPause->property("focusHaloExtra").toInt();
    QVERIFY2(focusEnvelope <= 68, "Play/Pause complete external focus envelope must be <= 68 logical px");
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
    // Qt's native window focus is asynchronous on Windows runners.
    // The application-level F10 Shortcut requires an activated window;
    // wait for that actual condition, then verify the real focus transfer.
    window->raise();
    window->requestActivate();
    QTRY_VERIFY_WITH_TIMEOUT(window->isActive(),5000);
    QTest::keyClick(window, Qt::Key_F10);
    QTRY_VERIFY_WITH_TIMEOUT(
        desktopMenu->property("activeFocus").toBool(),5000);
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

    // FIX-UI-002: Verify View entries are generated from the live editor
    // adapter list and include Compressor without a hard-coded menu slot.
    auto* compressorMenuItem = root->findChild<QObject*>(
        QStringLiteral("menuViewCompressor"));
    auto* viewEditorInstantiator = root->findChild<QObject*>(
        QStringLiteral("viewEditorInstantiator"));
    auto* cleanupViewMenu = root->findChild<QObject*>(
        QStringLiteral("desktopViewMenu"));
    QVERIFY2(compressorMenuItem && viewEditorInstantiator && cleanupViewMenu,
        "Dynamic View menu must expose the available Compressor editor");
    QCOMPARE(compressorMenuItem->property("text").toString(),
        QStringLiteral("Compressor"));
    QVERIFY(compressorMenuItem->property("enabled").toBool());
    QCOMPARE(viewEditorInstantiator->property("count").toInt(),
        static_cast<int>(dspChainAdapterModel.modules().size()));
    QCOMPARE(viewEditorInstantiator->property("count").toInt(), 3);
    // Four waveform actions, two separators and one Dock/Undock command
    // follow the generated editors; do not weaken count checking.
    auto* dockingMenuItem = root->findChild<QObject*>(
        QStringLiteral("menuDockUndockSelectedModule"));
    QVERIFY2(dockingMenuItem, "View must expose generic Dock/Undock");
    QCOMPARE(dockingMenuItem->property("text").toString(),
             QStringLiteral("Undock selected module"));
    QVERIFY(dockingMenuItem->property("enabled").toBool());
    QCOMPARE(cleanupViewMenu->property("count").toInt(),
        viewEditorInstantiator->property("count").toInt() + 7);

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

    // Establish the minimum supported geometry before opening the
    // menu so that resizing for a screenshot cannot dismiss the popup.
    window->resize(QSize{1184, 688});
    QTest::qWait(150);
    auto* cleanupViewMenuLabel = root->findChild<QObject*>(
        QStringLiteral("desktopMenuBarLabel_View"));
    QVERIFY(cleanupViewMenuLabel && cleanupViewMenuLabel->parent());
    auto* viewBarItem = qobject_cast<QQuickItem*>(cleanupViewMenuLabel->parent());
    QVERIFY(viewBarItem);
    const QPointF cleanupViewMenuCenter = viewBarItem->mapToScene(
        QPointF{viewBarItem->width() / 2.0, viewBarItem->height() / 2.0});
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
        cleanupViewMenuCenter.toPoint());
    QTest::qWait(120);
    QVERIFY2(cleanupViewMenu->property("visible").toBool(),
        "View menu must show dynamically populated editors");
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("minor_fix_002_view_menu_editors_1184x688.png"),
        QSize{1184, 688}));
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


    auto* dspChainStateText0 = dspChainRow0
        ? dspChainRow0->findChild<QObject*>(QStringLiteral("dspChainStateText_0"))
        : nullptr;


    auto* dspChainStateText1 = dspChainRow1
        ? dspChainRow1->findChild<QObject*>(QStringLiteral("dspChainStateText_1"))
        : nullptr;

    QVERIFY2(dspChainRow0 != nullptr && dspChainStateText0 != nullptr, "Row 0 (Input Gain) components must exist");
    QVERIFY2(dspChainRow1 != nullptr && dspChainStateText1 != nullptr, "Row 1 (Parametric EQ) components must exist");

    // Icon is a stable slot independent of the active/BYP state.
    auto* chainGainIcon = qobject_cast<QQuickItem*>(
        dspChainRow0->findChild<QObject*>(
            QStringLiteral("dspChainModuleIcon_0")));
    auto* chainEqIcon = qobject_cast<QQuickItem*>(
        dspChainRow1->findChild<QObject*>(
            QStringLiteral("dspChainModuleIcon_1")));
    QVERIFY2(chainGainIcon && chainEqIcon,
             "One fixed semantic icon replaces the obsolete configuration LED");
    QVERIFY(!find_child_by_name(dspChainRow0, QStringLiteral("dspChainConfigLed_0")));
    QVERIFY(!find_child_by_name(dspChainRow1, QStringLiteral("dspChainConfigLed_1")));
    QCOMPARE(chainGainIcon->property("kind").toString(),
             QStringLiteral("module-gain"));
    QCOMPARE(chainEqIcon->property("kind").toString(),
             QStringLiteral("module-eq"));
    QCOMPARE(chainGainIcon->property("strokeColor").value<QColor>(),
             QColor{QStringLiteral("#6F9FB3")});
    QCOMPARE(chainEqIcon->property("strokeColor").value<QColor>(),
             QColor{QStringLiteral("#4E86C6")});
    QCOMPARE(chainGainIcon->width(),16.0);
    QCOMPARE(chainEqIcon->width(),16.0);
    const qreal eqIconXBefore = chainEqIcon->mapToItem(
        qobject_cast<QQuickItem*>(dspChainRow1),QPointF{}).x();

    // 12px title in 164px sidebar: BYP badge (and 28px spacer) removed.
    auto* chainEqTitle = qobject_cast<QQuickItem*>(
        dspChainRow1->findChild<QObject*>(
            QStringLiteral("dspChainModuleTitle_1")));
    QVERIFY(chainEqTitle);
    QCOMPARE(chainEqTitle->property("text").toString(),
             QStringLiteral("Parametric EQ"));
    window->resize(QSize{1184, 688});
    QTest::qWait(80);
    QVERIFY2(!chainEqTitle->property("truncated").toBool(),
             "FIX-UI-004: Parametric EQ title must not be elided");
    QVERIFY2(chainEqTitle->implicitWidth() <= chainEqTitle->width() + 0.5,
             "FIX-UI-004: full 12px title must fit without scaling");

    // Verify Compressor Editor components
    auto* compressorEditor = dspEditorHostObj->findChild<QObject*>(QStringLiteral("compressorEditor"));
    auto* dspHostModuleTitle = dspEditorHostObj->findChild<QObject*>(QStringLiteral("dspHostModuleTitle"));
    QVERIFY2(compressorEditor != nullptr, "compressorEditor must exist in dspEditorHost");
    QVERIFY2(dspHostModuleTitle != nullptr, "dspHostModuleTitle must exist in dspEditorHost");

    // FIX-UI-002: generated item dispatch targets the live instance ID.
    QVERIFY(QMetaObject::invokeMethod(compressorMenuItem, "triggered"));
    QCoreApplication::processEvents();
    QCOMPARE(dspChainAdapterModel.selected_index(), 2);
    QVERIFY(QMetaObject::invokeMethod(gainMenuItem, "triggered"));
    QCoreApplication::processEvents();
    QCOMPARE(dspChainAdapterModel.selected_index(), 0);

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
    QCOMPARE(compressorViewModel.transfer_curve_points().size(), 191);
    QCOMPARE(compressorCurveCanvas->property("plotXMinDbfs").toDouble(), -60.0);
    QCOMPARE(compressorCurveCanvas->property("plotXMaxDbfs").toDouble(), 6.0);
    QCOMPARE(compressorCurveCanvas->property("plotYMinDbfs").toDouble(), -60.0);
    QCOMPARE(compressorCurveCanvas->property("plotYMaxDbfs").toDouble(), 6.0);

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

    // Extreme but valid curve states must auto-fit instead of losing interactive handles.
    compressorViewModel.setDraftFieldValue(QStringLiteral("thresholdDbfs"), -120.0);
    compressorViewModel.setDraftFieldValue(QStringLiteral("makeupGainDb"), -24.0);
    QCoreApplication::processEvents();
    QVERIFY(compressorCurveCanvas->property("plotXMinDbfs").toDouble() <= -120.0);
    QVERIFY(compressorCurveCanvas->property("plotYMinDbfs").toDouble() < -60.0);
    QVERIFY(compressorViewModel.validation_field().isEmpty());
    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("m14_compressor_curve_extreme_negative_autofit_1184x688.png"),
        QSize{1184, 688}));
    compressorViewModel.cancelDraft();

    compressorViewModel.setDraftFieldValue(QStringLiteral("makeupGainDb"), 24.0);
    QCoreApplication::processEvents();
    QVERIFY(compressorCurveCanvas->property("plotYMaxDbfs").toDouble() > 6.0);
    QVERIFY(compressorViewModel.validation_field().isEmpty());
    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("m14_compressor_curve_extreme_positive_autofit_1184x688.png"),
        QSize{1184, 688}));
    compressorViewModel.cancelDraft();
    QCoreApplication::processEvents();

    QVERIFY(capture_visual_evidence(window, QStringLiteral("m14_compressor_editor_1184x688.png"), QSize{1184, 688}));
    QVERIFY(capture_visual_evidence(window, QStringLiteral("m14_compressor_editor_1440x900.png"), QSize{1440, 900}));

    // Restore selected module index to 0 (Input Gain) for remaining Gain/EQ smoke steps
    QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 0));
    QTest::qWait(50);
    QCoreApplication::processEvents();

    // Initial Gain state and dim family pictogram
    QCOMPARE(dspChainStateText0->property("text").toString(), QStringLiteral("0.0 dB Default"));
    QCOMPARE(chainGainIcon->opacity(), 0.37);
    QVERIFY2(!dspChainRow0->property("isBypassed").toBool(), "Gain should initially process audio");
    QVERIFY(!find_child_by_name(dspChainRow0, QStringLiteral("dspChainBypassBadge_0")));

    // Initial EQ state and dim family pictogram
    QCOMPARE(dspChainStateText1->property("text").toString(), QStringLiteral("Flat Default"));
    QCOMPARE(chainEqIcon->opacity(), 0.37);
    QVERIFY2(!dspChainRow1->property("isBypassed").toBool(), "EQ should initially process audio");
    QVERIFY(!find_child_by_name(dspChainRow1, QStringLiteral("dspChainBypassBadge_1")));

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
    const auto gainFillTracksKnob = [&]() {
        return bipolar_fill_matches_handle(
            gainSlider, QStringLiteral("gainSliderTrack"),
            QStringLiteral("gainSliderTrackFill"),
            QStringLiteral("gainSliderThumb"), 0.5);
    };
    QVERIFY2(gainFillTracksKnob(),
             "Input Gain ZERO track must terminate at centered white knob");
    auto* gainFill = find_child_by_name(
        gainSlider, QStringLiteral("gainSliderTrackFill"));
    QVERIFY(gainFill);
    const QColor gainTrackColor = gainFill->property("color").value<QColor>();
    QCOMPARE(gainTrackColor.red(), 0);
    QCOMPARE(gainTrackColor.green(), 200);
    QCOMPARE(gainTrackColor.blue(), 255);
    QVERIFY(std::abs(gainTrackColor.alphaF() - 0.38) <= 0.006);
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

    // FIX-UI-003: assert invariant chain-title/subtitle/semantic-icon geometry
    // between Input Gain Default and a representative Manual value.
    window->resize(QSize{1184, 688});
    QTest::qWait(150);
    auto* chainRowItem0 = qobject_cast<QQuickItem*>(dspChainRow0);
    auto* chainTitleItem0 = qobject_cast<QQuickItem*>(
        dspChainRow0->findChild<QObject*>(
            QStringLiteral("dspChainModuleTitle_0")));
    auto* chainStateItem0 = qobject_cast<QQuickItem*>(dspChainStateText0);
    auto* chainSymbolItem0 = chainGainIcon;
    QVERIFY(chainRowItem0 && chainTitleItem0 && chainStateItem0 && chainSymbolItem0);
    const QPointF chainTitleDefault = chainTitleItem0->mapToItem(
        chainRowItem0, QPointF{0.0, 0.0});
    const QPointF chainStateDefault = chainStateItem0->mapToItem(
        chainRowItem0, QPointF{0.0, 0.0});
    const QPointF chainSymbolDefault = chainSymbolItem0->mapToItem(
        chainRowItem0, QPointF{0.0, 0.0});
    QCOMPARE(chainTitleItem0->property("text").toString(),
        QStringLiteral("Input Gain"));
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("minor_fix_003_gain_default_1184x688.png"),
        QSize{1184, 688}));

    // Modify Input Gain to +3.5 dB via ViewModel
    QVERIFY(gainViewModel.setGainDb(3.5));
    QCoreApplication::processEvents();
    QCOMPARE(gainDbDisplay->property("text").toString(), QStringLiteral("+3.5 dB"));
    QVERIFY2(gainFillTracksKnob(),
             "Positive Input Gain track must end at the actual knob center");
    QCOMPARE(dspChainStateText0->property("text").toString(), QStringLiteral("+3.5 dB Manual"));
    QCOMPARE(chainGainIcon->opacity(), 1.0);
    QCOMPARE(chainTitleItem0->mapToItem(
        chainRowItem0, QPointF{0.0, 0.0}).x(), chainTitleDefault.x());
    QCOMPARE(chainStateItem0->mapToItem(
        chainRowItem0, QPointF{0.0, 0.0}).x(), chainStateDefault.x());
    QCOMPARE(chainSymbolItem0->mapToItem(
        chainRowItem0, QPointF{0.0, 0.0}).x(), chainSymbolDefault.x());
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("minor_fix_003_gain_manual_1184x688.png"),
        QSize{1184, 688}));
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
    QVERIFY2(gainFillTracksKnob(),
             "Negative Input Gain track must end at the actual knob center");

    QVERIFY(gainViewModel.setGainDb(24.0));
    QCoreApplication::processEvents();
    QCOMPARE(gainViewModel.gain_db(), 24.0);
    QVERIFY(gainFillTracksKnob());

    // Invalid gain text format shows validation error and preserves previous value
    QVERIFY(!gainViewModel.setGainDbText(QStringLiteral("invalid")));
    QCoreApplication::processEvents();
    QVERIFY2(!gainViewModel.validation_error().isEmpty(), "Validation error must be non-empty for invalid gain text");
    // PO: the application's shared status bar owns validation presentation.
    // Input Gain keeps only the local invalid field outline; no inline footer
    // may push the editor. Isolated editor fixtures still use local feedback.
    QVERIFY2(!qobject_cast<QQuickItem*>(gainValidationError)->isVisible(),
        "Full application must suppress duplicate local Gain error message");
    QVERIFY2(root->property("statusDspErrorText").toString().contains(
        QStringLiteral("Input Gain")),
        "Selected Gain validation should be routed to global status");
    QCOMPARE(root->property("statusSeverity").toString(), QStringLiteral("ERROR"));
    const auto gainStatusMessage = statusBarMessage->property("text").toString();
    QVERIFY2(gainStatusMessage.contains(QStringLiteral("Input Gain"))
        && gainStatusMessage.contains(QStringLiteral("invalid"), Qt::CaseInsensitive),
        "Global footer must name the offending DSP and preserve error detail");
    QCOMPARE(statusIndicator->property("color").value<QColor>(),
        QColor{QStringLiteral("#F27683")});
    QCOMPARE(statusBarMessage->property("color").value<QColor>(),
        QColor{QStringLiteral("#F27683")});
    QCOMPARE(statusBar->property("height").toInt(), 24);
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("m17_global_status_gain_invalid_1184x688.png"),
        QSize{1184, 688}));

    // Reset button restores 0.0 dB default
    QVERIFY2(QMetaObject::invokeMethod(resetGainButton, "clicked"), "Clicking resetGainButton must succeed");
    QCoreApplication::processEvents();
    QCOMPARE(gainViewModel.gain_db(), 0.0);
    QVERIFY(gainFillTracksKnob());

    // UX-002: real QWheelEvent delivery at left, center and right
    // of the SAME full-width slider, not just over the white thumb.
    auto* gainSliderItem = qobject_cast<QQuickItem*>(gainSlider);
    QVERIFY(gainSliderItem);
    auto* gainWheelArea = qobject_cast<QQuickItem*>(
        find_child_by_name(gainSlider, QStringLiteral("inputGainWheelArea")));
    QVERIFY2(gainWheelArea, "Full-width Input Gain wheel surface required");
    QCOMPARE(gainWheelArea->width(), gainSliderItem->width());
    QCOMPARE(gainWheelArea->height(), gainSliderItem->height());
    QCOMPARE(gainWheelArea->property("acceptedButtons").toInt(),
             static_cast<int>(Qt::NoButton));
    // Stabilize the native-window event route after capture, resize and
    // Ctrl+Z/Reset. Do not retry or fake any wheel value/change.
    window->requestActivate();
    QTRY_VERIFY_WITH_TIMEOUT(gainSliderItem->isVisible()
        && gainSliderItem->isEnabled()
        && gainWheelArea->isVisible()
        && gainWheelArea->isEnabled()
        && gainSliderItem->width() > 50
        && gainSliderItem->height() > 0, 2000);
    QTest::qWait(75);
    QCoreApplication::processEvents();
    const auto sendGainWheel = [&](double fraction, int delta) {
        const QPointF wheelPos = gainSliderItem->mapToScene(
            QPointF{gainSliderItem->width() * fraction,
                    gainSliderItem->height() / 2.0});
        QTest::mouseMove(window, wheelPos.toPoint());
        QCoreApplication::processEvents(); // settle hover/hit-test before wheel
        QWheelEvent wheelEvent{
            wheelPos, window->mapToGlobal(wheelPos.toPoint()),
            QPoint{}, QPoint{0, delta}, Qt::NoButton,
            Qt::NoModifier, Qt::NoScrollPhase, false};
        wheelEvent.ignore();
        QCoreApplication::sendEvent(window, &wheelEvent);
        QCoreApplication::processEvents();
        if (!wheelEvent.isAccepted()) {
            const QPointF areaOrigin = gainWheelArea->mapToScene(QPointF{0, 0});
            qWarning().noquote()
                << "UX002_WHEEL_UNACCEPTED"
                << "fraction" << fraction << "delta" << delta
                << "window" << window->size()
                << "active" << window->isActive()
                << "sliderVisible" << gainSliderItem->isVisible()
                << "sliderEnabled" << gainSliderItem->isEnabled()
                << "slider" << gainSliderItem->width() << gainSliderItem->height()
                << "wheelArea" << areaOrigin
                << gainWheelArea->width() << gainWheelArea->height()
                << "sceneWheel" << wheelPos
                << "valueDb" << gainViewModel.gain_db();
        }
        return wheelEvent.isAccepted();
    };
    QVERIFY(sendGainWheel(0.03, 120));
    QCOMPARE(gainViewModel.gain_db(), 0.1);
    QCOMPARE(gainDbDisplay->property("text").toString(), QStringLiteral("+0.1 dB"));
    QVERIFY(sendGainWheel(0.50, 120));
    QCOMPARE(gainViewModel.gain_db(), 0.2);
    QVERIFY(sendGainWheel(0.97, 120));
    QCOMPARE(gainViewModel.gain_db(), 0.3);
    QVERIFY(sendGainWheel(0.97, -120));
    QCOMPARE(gainViewModel.gain_db(), 0.2);
    QVERIFY(sendGainWheel(0.50, -120));
    QCOMPARE(gainViewModel.gain_db(), 0.1);
    QVERIFY(sendGainWheel(0.03, -120));
    QCOMPARE(gainViewModel.gain_db(), 0.0);
    // Partial high-resolution wheel motion must NOT commit.
    QVERIFY(sendGainWheel(0.50, 60));
    QCOMPARE(gainViewModel.gain_db(), 0.0);
    QVERIFY(sendGainWheel(0.50, 60));
    QCOMPARE(gainViewModel.gain_db(), 0.1);
    gainViewModel.undo();
    QCOMPARE(gainViewModel.gain_db(), 0.0);
    QVERIFY(gainFillTracksKnob());
    QCOMPARE(dspChainStateText0->property("text").toString(), QStringLiteral("0.0 dB Default"));
    QCOMPARE(chainGainIcon->opacity(), 0.37);
    QVERIFY2(!qobject_cast<QQuickItem*>(gainValidationError)->isVisible(),
        "Gain local error display must remain suppressed after correction");
    QVERIFY(root->property("statusDspErrorText").toString().isEmpty());
    QCOMPARE(root->property("statusSeverity").toString(), QStringLiteral("INFO"));
    QCOMPARE(statusIndicator->property("color").value<QColor>(),
        QColor{QStringLiteral("#00E6E6")});
    QCOMPARE(statusBar->property("height").toInt(), 24);
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("m17_global_status_gain_corrected_1184x688.png"),
        QSize{1184, 688}));

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
    // Canonical Flat state -> family symbol is dim and bypass row style is absent.
    QVERIFY2(eqViewModel.is_default(), "EQ state must be default/flat initially");
    QCOMPARE(chainEqIcon->opacity(), 0.37);
    QVERIFY2(!dspChainRow1->property("isBypassed").toBool(), "EQ should initially process audio");
    QVERIFY(!find_child_by_name(dspChainRow1, QStringLiteral("dspChainBypassBadge_1")));

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

    // Add band -> EQ state becomes Manual non-default -> EQ family symbol becomes vivid without changing hue
    QVERIFY2(QMetaObject::invokeMethod(addBandBtn, "clicked"), "Clicking addBandButton must succeed");
    QCoreApplication::processEvents();
    QCOMPARE(eqViewModel.band_count(), 2);
    QCOMPARE(eqViewModel.selected_index(), 1);
    QVERIFY2(!eqViewModel.is_default(), "EQ state must no longer be default after adding a band");
    QCOMPARE(chainEqIcon->opacity(), 1.0);
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
    QCOMPARE(chainGainIcon->opacity(), 0.37);
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
    auto* eqFreqSlider = find_child_by_name(eqEditor, QStringLiteral("eqFrequencySlider"));
    auto* eqGainSlider = find_child_by_name(eqEditor, QStringLiteral("eqGainSlider"));
    auto* eqQSlider = find_child_by_name(eqEditor, QStringLiteral("eqQSlider"));
    auto* eqShelfSlider = find_child_by_name(eqEditor, QStringLiteral("eqShelfSlopeSlider"));
    auto* eqInspector = qobject_cast<QQuickItem*>(find_child_by_name(
        eqEditor, QStringLiteral("eqInspectorRegion")));
    QVERIFY2(eqFreqSlider && eqGainSlider && eqQSlider && eqShelfSlider && eqInspector,
             "All applicable EQ scalar fields require an actual slider");
    QCOMPARE(eqFreqSlider->property("from").toDouble(), 20.0);
    QCOMPARE(eqFreqSlider->property("to").toDouble(), 19845.0);
    QCOMPARE(eqGainSlider->property("from").toDouble(), -18.0);
    QCOMPARE(eqGainSlider->property("to").toDouble(), 18.0);
    QCOMPARE(eqQSlider->property("from").toDouble(), 0.10);
    QCOMPARE(eqQSlider->property("to").toDouble(), 12.0);
    QVERIFY(eqFreqSlider->property("logarithmic").toBool());
    QVERIFY(eqQSlider->property("logarithmic").toBool());
    QVERIFY(eqGainSlider->property("fillFromOrigin").toBool());
    QCOMPARE(eqGainSlider->property("positionFillOrigin").toDouble(), 0.0);
    for (auto* slider : {eqFreqSlider, eqGainSlider, eqQSlider, eqShelfSlider}) {
        QVERIFY(slider->property("hasSemanticAccent").toBool());
        QVERIFY(slider->property("haloOnFocus").toBool());
        const QColor c = slider->property("positionFillColor").value<QColor>();
        QCOMPARE(c.red(), 196);
        QCOMPARE(c.green(), 119);
        QCOMPARE(c.blue(), 74);
        QVERIFY(std::abs(c.alphaF() - 0.38) < 0.01);
    }

    QVERIFY2(filterBell && filterNotch && filterLowShelf && filterHighPass, "Filter buttons must exist");
    QVERIFY2(gainFieldObj->property("visible").toBool(), "Gain field must be visible for Bell filter");
    QVERIFY2(qFieldObj->property("visible").toBool(), "Q field must be visible for Bell filter");

    QVERIFY2(QMetaObject::invokeMethod(filterNotch, "clicked"), "Clicking filterButton_NOTCH must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(!gainFieldObj->property("visible").toBool(), "Gain field must be hidden for Notch filter");

    QVERIFY2(QMetaObject::invokeMethod(filterLowShelf, "clicked"), "Clicking filterButton_LOW_SHELF must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(shelfSlopeFieldObj->property("visible").toBool(), "Shelf slope field must be visible for Low Shelf filter");
    QVERIFY(eqShelfSlider->property("visible").toBool());
    QVERIFY(!eqQSlider->property("visible").toBool());

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
    QVERIFY2(valMsgText && !valMsgText->property("visible").toBool(),
        "Global status bar must own the visible validation message, not EQ graph");
    auto* transientError = qobject_cast<QQuickItem*>(find_child_by_name(
        eqEditor, QStringLiteral("eqStatusRegion")));
    QVERIFY(transientError && !transientError->isVisible());
    QCOMPARE(root->property("statusSeverity").toString(), QStringLiteral("ERROR"));
    QVERIFY2(statusBarMessage->property("text").toString().contains(
        QStringLiteral("Parametric"), Qt::CaseInsensitive),
        "EQ validation must be present in the global status bar");
    QCOMPARE(statusIndicator->property("color").value<QColor>(),
        QColor{QStringLiteral("#F27683")});
    QVERIFY2(check_item_contained_in_ancestor(
        transientError, qobject_cast<QQuickItem*>(eqGraph)),
        "Hidden EQ validation overlay must retain its original fixed geometry");
    QCOMPARE(statusBar->property("height").toInt(), 24);
    QCOMPARE(statusBarMessage->property("color").value<QColor>(),
        QColor{QStringLiteral("#F27683")});
    QVERIFY2(capture_visual_evidence(window,
        QStringLiteral("m17_global_status_eq_invalid_1184x688.png"),
        QSize{1184, 688}),
        "Native Qt evidence of centralized EQ error must be captured");

    QTest::keyClick(window, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QCOMPARE(freqInput->property("text").toString(), QStringLiteral("1000"));
    QCOMPARE(eqViewModel.frequency_text(), QStringLiteral("1000"));
    QVERIFY2(eqViewModel.validation_field().isEmpty(), "Validation field must be empty after Escape key cancel");
    QVERIFY2(!transientError->isVisible(),
        "Global-bar mode must keep local EQ overlay hidden");
    QVERIFY(root->property("statusDspErrorText").toString().isEmpty());
    QCOMPARE(root->property("statusSeverity").toString(), QStringLiteral("INFO"));
    QCOMPARE(statusIndicator->property("color").value<QColor>(),
        QColor{QStringLiteral("#00E6E6")});
    QCOMPARE(statusBar->property("height").toInt(), 24);
    QVERIFY2(capture_visual_evidence(window,
        QStringLiteral("m17_global_status_eq_corrected_1184x688.png"),
        QSize{1184, 688}),
        "Native Qt evidence of centralized EQ correction must be captured");

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

    // FIX-UI-001: compare positions and sizes at identical window geometry
    // across ON -> OFF -> ON, including unaffected sibling controls.
    auto* eqToolbar = qobject_cast<QQuickItem*>(
        find_child_by_name(eqEditor, QStringLiteral("eqTopToolbarRegion")));
    auto* resetItem = qobject_cast<QQuickItem*>(resetFlatBtn);
    auto* overallItem = qobject_cast<QQuickItem*>(overallToggleBtn);
    auto* spectrumItem = qobject_cast<QQuickItem*>(spectrumToggleBtn);
    auto* addItem = qobject_cast<QQuickItem*>(
        eqEditor->findChild<QObject*>(QStringLiteral("addBandButton")));
    QVERIFY(eqToolbar && resetItem && overallItem && spectrumItem && addItem);
    window->resize(QSize{1184, 688});
    QTest::qWait(150);
    const auto toolbarGeometry = [eqToolbar](QQuickItem* item) {
        const auto pos = item->mapToItem(eqToolbar, QPointF{0.0, 0.0});
        return QRectF{pos, QSizeF{item->width(), item->height()}};
    };
    // The capture renders/shows the native Qt Quick window and can
    // complete a pending RowLayout reflow. Snapshot the *rendered* geometry.
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("minor_fix_001_eq_spectrum_on_1184x688.png"),
        QSize{1184, 688}));
    const QRectF resetOn = toolbarGeometry(resetItem);
    const QRectF overallOn = toolbarGeometry(overallItem);
    const QRectF spectrumOn = toolbarGeometry(spectrumItem);
    const QRectF addOn = toolbarGeometry(addItem);

    const bool undoStateBeforeToggle = eqViewModel.can_undo();
    const quint64 previewGenBeforeToggle = eqViewModel.preview_generation();

    QVERIFY(QMetaObject::invokeMethod(spectrumToggleBtn, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY2(!liveSpectrumVM.spectrumEnabled(), "SPECTRUM toggle click must set spectrumEnabled to false");
    QVERIFY2(!spectrumCanvasObj->property("visible").toBool(), "spectrumCanvas must be hidden when SPECTRUM is OFF");
    QCOMPARE(toolbarGeometry(resetItem), resetOn);
    QCOMPARE(toolbarGeometry(overallItem), overallOn);
    QCOMPARE(toolbarGeometry(spectrumItem), spectrumOn);
    QCOMPARE(toolbarGeometry(addItem), addOn);
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("minor_fix_001_eq_spectrum_off_1184x688.png"),
        QSize{1184, 688}));
    QCOMPARE(eqViewModel.can_undo(), undoStateBeforeToggle);
    QCOMPARE(eqViewModel.preview_generation(), previewGenBeforeToggle);

    QVERIFY(QMetaObject::invokeMethod(spectrumToggleBtn, "clicked"));
    QCoreApplication::processEvents();
    QVERIFY2(liveSpectrumVM.spectrumEnabled(), "SPECTRUM toggle click must restore spectrumEnabled to true");
    QVERIFY2(spectrumCanvasObj->property("visible").toBool(), "spectrumCanvas must be restored visible when SPECTRUM is ON");

    QCOMPARE(toolbarGeometry(resetItem), resetOn);
    QCOMPARE(toolbarGeometry(overallItem), overallOn);
    QCOMPARE(toolbarGeometry(spectrumItem), spectrumOn);
    QCOMPARE(toolbarGeometry(addItem), addOn);

    // Click Bypass -> row skin and subtitle change without any badge/width shift; manual/default icon state remains independent.
    QVERIFY2(QMetaObject::invokeMethod(abBypassBtn, "clicked"), "Clicking abButtonBypass must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqViewModel.bypass(), "eqViewModel.bypass must be true after clicking Bypass");
    QVERIFY2(dspChainRow1->property("isBypassed").toBool(), "EQ chain row must reflect bypass without a badge");
    QCOMPARE(chainEqIcon->width(),16.0);
    QCOMPARE(chainEqIcon->mapToItem(
        qobject_cast<QQuickItem*>(dspChainRow1),QPointF{}).x(),eqIconXBefore);

    QVERIFY2(QMetaObject::invokeMethod(abActiveBtn, "clicked"), "Clicking abButtonActive must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(!eqViewModel.bypass(), "eqViewModel.bypass must be false after clicking Active");
    QVERIFY2(!dspChainRow1->property("isBypassed").toBool(), "EQ chain row must recover active visuals");

    // Reset Flat -> restores Default status -> family symbol dims
    QVERIFY2(QMetaObject::invokeMethod(resetFlatBtn, "clicked"), "Clicking eqResetFlatButton must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(eqViewModel.is_default(), "EQ state must be default/flat after Reset Flat");
    QCOMPARE(chainEqIcon->opacity(), 0.37);

    // Undo -> restores 2-band non-default state -> family symbol brightens
    QVERIFY2(QMetaObject::invokeMethod(undoBtn, "clicked"), "Clicking eqUndoButton must succeed");
    QCoreApplication::processEvents();
    QVERIFY2(!eqViewModel.is_default(), "EQ state must be non-default after Undo");
    QCOMPARE(chainEqIcon->opacity(), 1.0);

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
    // One selected-band ring identity shared by all three controls,
    // with uniform secondary copper tracks. Switching bands must not
    // preview or commit any parameter and band six needs a separator.
    const quint64 previewBeforeColorSwitch = eqViewModel.preview_generation();
    const std::array expectedSliderBands{
        QColor{QStringLiteral("#2ED3FF")},
        QColor{QStringLiteral("#2FD98F")},
        QColor{QStringLiteral("#FFD84A")},
        QColor{QStringLiteral("#FF6B6B")},
        QColor{QStringLiteral("#4F7CFF")},
        QColor{QStringLiteral("#F5F8FC")}
    };
    for (std::size_t n = 0; n < expectedSliderBands.size(); ++n) {
        eqViewModel.selectBand(static_cast<int>(n));
        QCoreApplication::processEvents();
        QCOMPARE(eqFreqSlider->property("semanticAccent").value<QColor>(),
                 expectedSliderBands[n]);
        QCOMPARE(eqGainSlider->property("semanticAccent").value<QColor>(),
                 expectedSliderBands[n]);
        QCOMPARE(eqQSlider->property("semanticAccent").value<QColor>(),
                 expectedSliderBands[n]);
        QCOMPARE(eqFreqSlider->property("lightAccentSeparator").toBool(),
                 n == 5U);
    }
    QCOMPARE(eqViewModel.preview_generation(), previewBeforeColorSwitch);

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
    QVERIFY2(res1440.metrics.eqStatusContained && res1440.metrics.eqStatusOverGraph,
        "EQ validation overlay must remain inside graph at 1440x900");
    QVERIFY2(res1440.metrics.waveformHeight >= 180.0, "Waveform height must be >= 180 px at 1440x900");
    QVERIFY2(res1440.metrics.hostHeight >= 400.0, "dspEditorHost must be dominant (>= 400 px) at 1440x900");

    const auto res1920Short = evaluate_layout_at_size(engine, 1920, 688);
    QVERIFY2(res1920Short.valid, qPrintable(res1920Short.errorMessage));
    verifyUpperPadding(res1920Short.metrics);
    verifyContainmentMetrics(res1920Short.metrics, "1920x688");
    QVERIFY2(res1920Short.metrics.adaptiveContextVisible, "Adaptive Context must not disappear in a wide, short window");
    QVERIFY2(res1920Short.metrics.eqInspectorContained,
        qPrintable(QStringLiteral(
            "EQ inspector overflow at 1920x688: editor=%1, inspector top=%2, height=%3, bottom=%4")
            .arg(res1920Short.metrics.eqEditorHeight)
            .arg(res1920Short.metrics.eqInspectorY)
            .arg(res1920Short.metrics.eqInspectorHeight)
            .arg(res1920Short.metrics.eqInspectorY + res1920Short.metrics.eqInspectorHeight)));
    QVERIFY2(res1920Short.metrics.eqStatusContained,
        "EQ transient status must remain inside editor at 1920x688");
    QVERIFY2(res1920Short.metrics.eqStatusOverGraph,
        "EQ validation strip must overlay graph, not reserve permanent footer");
    QVERIFY2(res1920Short.metrics.eqStatusY + res1920Short.metrics.eqStatusHeight
             <= res1920Short.metrics.eqInspectorY + 0.5,
        "EQ validation overlay must not overlap inspector controls");
    QVERIFY2(res1920Short.metrics.eqGraphHeight >= 102.0,
        "EQ graph must stay at least 102px tall");
    QCOMPARE(res1920Short.metrics.eqStatusHeight, 16.0);
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
    QVERIFY2(res1184.metrics.eqStatusContained && res1184.metrics.eqStatusOverGraph,
        "EQ validation overlay must remain inside graph at 1184x688");
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

    // The added slider row must stay INSIDE the inspector at the smallest
    // supported window, without spilling into EQ validation/status strips.
    // The 164px field width is unchanged and sliders align with the boxes.
    for (const auto& names : {
             std::pair{QStringLiteral("frequencyField"), QStringLiteral("eqFrequencySlider")},
             std::pair{QStringLiteral("gainField"), QStringLiteral("eqGainSlider")},
             std::pair{QStringLiteral("qField"), QStringLiteral("eqQSlider")},
             std::pair{QStringLiteral("shelfSlopeField"), QStringLiteral("eqShelfSlopeSlider")}}) {
        auto* fieldItem = qobject_cast<QQuickItem*>(find_child_by_name(eqEditor, names.first));
        auto* sliderItem = qobject_cast<QQuickItem*>(find_child_by_name(eqEditor, names.second));
        QVERIFY(fieldItem && sliderItem);
        QCOMPARE(fieldItem->property("fieldWidth").toInt(), 164);
        if (sliderItem->isVisible()) {
            QVERIFY(std::abs(sliderItem->width() - 164.0) <= 0.6);
            const auto fieldX = fieldItem->mapToItem(eqInspector, QPointF{}).x();
            const auto sliderX = sliderItem->mapToItem(eqInspector, QPointF{}).x();
            QVERIFY(std::abs(fieldX - sliderX) <= 0.6);
            const auto bottom = sliderItem->mapToItem(eqInspector, QPointF{}).y()
                + sliderItem->height();
            QVERIFY2(bottom <= eqInspector->height() - 2.0,
                     "EQ slider must fit vertically within the inspector at min size");
        }
    }
    QVERIFY(capture_visual_evidence(window,
        QStringLiteral("m15_eq_selected_band_sliders_1184x688.png"),
        QSize{1184, 688}));

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
    // Preserve the legacy graph/keyboard/wheel guidance AND advertise the
    // new third input modality. Regression caught by exact CI #396.
    QVERIFY(frequencyFieldObj->property("interactionHint").toString().contains(QStringLiteral("left/right")));
    QVERIFY(gainFieldObj->property("interactionHint").toString().contains(QStringLiteral("up/down")));
    QVERIFY(qFieldObj->property("interactionHint").toString().contains(QStringLiteral("Mouse wheel")));
    for (auto* field : {frequencyFieldObj, gainFieldObj, qFieldObj, shelfSlopeFieldObj}) {
        const auto hint = field->property("interactionHint").toString();
        QVERIFY2(hint.contains(QStringLiteral("slider"), Qt::CaseInsensitive),
                 "An EQ numeric field with a slider must document that modality");
    }
    QVERIFY(qFieldObj->property("interactionHint").toString().contains(QStringLiteral("Shift + wheel")));
    QVERIFY(shelfSlopeFieldObj->property("interactionHint").toString().contains(QStringLiteral("Shift + wheel")));

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
    // Qt Quick can retain the previous editor's 403px intermediate height
    // until its scene is rendered. A frame (not another fixed wait) makes
    // the minimum-size LayoutItemProxy geometry authoritative.
    auto* visualWindow = qobject_cast<QQuickWindow*>(window);
    QVERIFY2(visualWindow != nullptr, "Compressor containment requires a native QQuickWindow");
    QVERIFY2(!visualWindow->grabWindow().isNull(),
        "Compressor containment requires a completed Qt Quick frame");
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

    // Global 24lp status bar regression for Compressor. Errors in the
    // selected module must replace ordinary status without reflow or a
    // duplicate red message inside the controls panel. Errors belonging to
    // an inactive module must not leak into the selected module's footer.
    {
        auto* localCompressorError = qobject_cast<QQuickItem*>(
            find_child_by_name(compressorEditor,
                QStringLiteral("compressorValidationStatusRegion")));
        auto* thresholdField = qobject_cast<QQuickItem*>(
            find_child_by_name(compressorEditor,
                QStringLiteral("compressorThresholdField")));
        auto* statusBarItem = qobject_cast<QQuickItem*>(statusBar);
        QVERIFY(localCompressorError && thresholdField && statusBarItem);
        QVERIFY(!localCompressorError->isVisible());
        const auto beforeControl = QRectF{
            thresholdField->mapToItem(compressorEditorNative, QPointF{}),
            thresholdField->size()};
        const auto beforePanel = QRectF{
            controlsPanelNative->mapToItem(compressorEditorNative, QPointF{}),
            controlsPanelNative->size()};
        const auto beforeFooter = QRectF{
            statusBarItem->mapToScene(QPointF{}),
            statusBarItem->size()};
        compressorViewModel.setDraftFieldText(
            QStringLiteral("thresholdDbfs"), QStringLiteral("not-a-number"));
        QCoreApplication::processEvents();
        QVERIFY(!compressorViewModel.validation_message().isEmpty());
        QCOMPARE(compressorViewModel.validation_field(),
            QStringLiteral("thresholdDbfs"));
        QVERIFY2(!localCompressorError->isVisible(),
            "Full app must not show a duplicate in-panel Compressor error");
        QVERIFY2(root->property("statusDspErrorText").toString().contains(
            QStringLiteral("Compressor")),
            "Compressor validation must reach the global status bar");
        QCOMPARE(root->property("statusSeverity").toString(), QStringLiteral("ERROR"));
        QCOMPARE(statusIndicator->property("color").value<QColor>(),
            QColor{QStringLiteral("#F27683")});
        QCOMPARE(statusBarMessage->property("color").value<QColor>(),
            QColor{QStringLiteral("#F27683")});
        QCOMPARE(statusBar->property("height").toInt(), 24);
        QCOMPARE(QRectF(
            thresholdField->mapToItem(compressorEditorNative, QPointF{}),
            thresholdField->size()), beforeControl);
        QCOMPARE(QRectF(
            controlsPanelNative->mapToItem(compressorEditorNative, QPointF{}),
            controlsPanelNative->size()), beforePanel);
        QCOMPARE(QRectF(
            statusBarItem->mapToScene(QPointF{}),
            statusBarItem->size()), beforeFooter);
        QVERIFY(capture_visual_evidence(window,
            QStringLiteral("m17_global_status_compressor_invalid_1184x688.png"),
            QSize{1184, 688}));

        QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 0));
        QCoreApplication::processEvents();
        QVERIFY(root->property("statusDspErrorText").toString().isEmpty());
        QVERIFY2(!statusBarMessage->property("text").toString().contains(
            QStringLiteral("Compressor")),
            "Inactive DSP errors must not replace the active module's status");
        QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 2));
        QTest::qWait(80);
        QCoreApplication::processEvents();
        QVERIFY(root->property("statusDspErrorText").toString().contains(
            QStringLiteral("Compressor")));
        compressorViewModel.cancelDraft();
        QCoreApplication::processEvents();
        QVERIFY(compressorViewModel.validation_message().isEmpty());
        QVERIFY(root->property("statusDspErrorText").toString().isEmpty());
        QVERIFY(!localCompressorError->isVisible());
        QCOMPARE(QRectF(
            thresholdField->mapToItem(compressorEditorNative, QPointF{}),
            thresholdField->size()), beforeControl);
        QCOMPARE(QRectF(
            controlsPanelNative->mapToItem(compressorEditorNative, QPointF{}),
            controlsPanelNative->size()), beforePanel);
        QCOMPARE(QRectF(
            statusBarItem->mapToScene(QPointF{}),
            statusBarItem->size()), beforeFooter);
        QCOMPARE(statusBar->property("height").toInt(), 24);
        QVERIFY(capture_visual_evidence(window,
            QStringLiteral("m17_global_status_compressor_corrected_1184x688.png"),
            QSize{1184, 688}));
    }

    // Verify Live GR Ribbon UI Smoke Assertions inside curveWell
    auto* grRibbonSectionObj = find_child_by_name(compressorEditor, QStringLiteral("compressorGrRibbonSection"));
    auto* grRibbonCanvasObj = find_child_by_name(compressorEditor, QStringLiteral("compressorGrRibbonCanvas"));
    auto* grStateLabelObj = find_child_by_name(compressorEditor, QStringLiteral("compressorGrStateLabel"));
    QVERIFY2(grRibbonSectionObj != nullptr, "compressorGrRibbonSection must exist");
    QVERIFY2(grRibbonCanvasObj != nullptr, "compressorGrRibbonCanvas must exist");
    QVERIFY2(grStateLabelObj != nullptr, "compressorGrStateLabel must exist");

    auto* grRibbonSectionNative = qobject_cast<QQuickItem*>(grRibbonSectionObj);
    auto* grRibbonCanvasNative = qobject_cast<QQuickItem*>(grRibbonCanvasObj);
    auto* curveCanvasNative = qobject_cast<QQuickItem*>(compressorCurveCanvas);
    QVERIFY2(grRibbonSectionNative && grRibbonCanvasNative && curveCanvasNative && curveWellNative,
        "Curve well elements must be QQuickItems");
    QVERIFY2(check_item_contained_in_ancestor(grRibbonSectionNative, curveWellNative),
        "Live GR Ribbon section must be contained inside compressorCurveWell");
    QVERIFY2(curveCanvasNative->height() > grRibbonCanvasNative->height(),
        "Static transfer curve must remain visually dominant over Live GR ribbon");
    const QPointF curveBottomInWell = curveCanvasNative->mapToItem(
        curveWellNative, QPointF{0.0, curveCanvasNative->height()});
    const QPointF ribbonTopInWell = grRibbonSectionNative->mapToItem(
        curveWellNative, QPointF{0.0, 0.0});
    QVERIFY2(curveBottomInWell.y() <= ribbonTopInWell.y() + 0.5,
        "Transfer-curve editing surface and GR history band must not overlap");

    // Qualify ACTIVE WET from an explicit non-bypassed Compressor state.
    // Set authority directly to avoid scheduling an unrelated asynchronous preview.
    QVERIFY(masteringChainState.set_user_bypass(
        masteringChainState.compressor_instance_id(), false));
    compressorViewModel.refreshFromAuthority();
    QVERIFY(!compressorViewModel.bypass());

    // Feed deterministic Stage 3B2 sidecar telemetry through the SAME
    // production provider -> resolver -> CompressorViewModel presentation path.
    // The visible Audition Target is switched to PROCESSED as well: visual
    // evidence must never show PREPARED/GOLD while claiming ACTIVE WET.
    QTRY_VERIFY_WITH_TIMEOUT(
        auditionSelector.processed_available(), 5000);
    QVERIFY(auditionSelector.switch_to(
        app::AuditionTarget::PROCESSED));
    QCoreApplication::processEvents();

    const auto teleRealizationId = core::RealizationId{101};
    const auto teleCompId = *dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse(
            "44444444-4444-4444-4444-444444444444").value()).value();
    render::CompressorTelemetrySidecar smokeSidecar{
        teleCompId, teleRealizationId};
    smokeSidecar.valid = true;
    smokeSidecar.status = render::CompressorTelemetryStatus::OK;
    smokeSidecar.channel_layout = audio::ChannelLayout::STEREO_LR;
    smokeSidecar.sample_rate_hz = 48000;
    smokeSidecar.chain_revision = 1;

    render::CompressorTelemetryLane smokeLane0;
    constexpr int kSmokeBucketFrames = 240; // 5 ms at 48 kHz.
    constexpr int kSmokeBucketCount = 780;  // 3.9 s of visible history.
    for (int bIdx = 0; bIdx < kSmokeBucketCount; ++bIdx) {
        render::CompressorTelemetryBucket b{
            smokeSidecar.module_instance_id,
            smokeSidecar.realization_id};
        b.begin_frame = bIdx * kSmokeBucketFrames;
        b.end_frame = (bIdx + 1) * kSmokeBucketFrames;
        b.frame_count = kSmokeBucketFrames;
        b.peak_offset_frames = kSmokeBucketFrames / 2;
        b.attenuated_frame_count = kSmokeBucketFrames;
        const double phase = bIdx * 0.055;
        b.mean_reduction_db =
            2.0 + 1.35 * std::sin(phase)
                + 0.35 * std::sin(phase * 0.23);
        b.end_reduction_db = b.mean_reduction_db;
        b.peak_reduction_db =
            b.mean_reduction_db
                + 1.15 + 0.35 * std::cos(phase * 0.61);
        b.valid = true;
        smokeLane0.buckets.push_back(b);
    }
    smokeSidecar.channel_lanes.push_back(smokeLane0);

    std::shared_ptr<const render::CompressorTelemetrySidecar>
        activeTelemetrySidecar =
            std::make_shared<render::CompressorTelemetrySidecar>(
                smokeSidecar);

    observedPlayback->state = core::PlaybackState::PLAYING;
    observedPlayback->position =
        core::FrameIndex{
            kSmokeBucketCount * kSmokeBucketFrames};
    observedPlayback->duration =
        *core::FrameCount::create(192000).value();
    observedPlayback->audibleRealization.phase =
        core::AudibleHandoffPhase::NEW;
    observedPlayback->audibleRealization.realizationId =
        teleRealizationId;
    observedPlayback->audibleRealization.handoffEndFrame = 0;

    auto targetProvider = [&auditionSelector] {
        const auto target = auditionSelector.active_target();
        if (!target.has_value())
            return render::AuditionTarget::PREPARED;
        switch (*target) {
        case app::AuditionTarget::PREPARED:
            return render::AuditionTarget::PREPARED;
        case app::AuditionTarget::PROCESSED:
            return render::AuditionTarget::PROCESSED;
        case app::AuditionTarget::GOLD:
            return render::AuditionTarget::GOLD;
        }
        return render::AuditionTarget::PREPARED;
    };

    auto installTelemetryProviders = [&] {
        compressorViewModel.setTelemetryProviders(
            [&activeTelemetrySidecar]() {
                return activeTelemetrySidecar;
            },
            [&playbackTransport]() {
                return playbackTransport.playback_snapshot();
            },
            targetProvider);
    };
    installTelemetryProviders();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    QCOMPARE(
        auditionSelector.active_target(),
        std::optional<app::AuditionTarget>{
            app::AuditionTarget::PROCESSED});
    QCOMPARE(
        compressorViewModel.telemetry_status(),
        QStringLiteral("ACTIVE WET"));
    QVERIFY(compressorViewModel.telemetry_valid());
    QVERIFY(!compressorViewModel.telemetry_history_lanes().isEmpty());
    QCOMPARE(
        grStateLabelObj->property("text").toString(),
        QStringLiteral("ACTIVE WET"));

    // Capture Mandatory Visual Evidence Artifact.
    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral("compressor_gr_ribbon_1184x688.png"),
        QSize{1184, 688}));

    // Test DUAL_MONO 2-lane truthful presentation.
    const auto dualRealizationId = core::RealizationId{102};
    render::CompressorTelemetrySidecar dualSidecar{
        teleCompId, dualRealizationId};
    dualSidecar.valid = true;
    dualSidecar.status = render::CompressorTelemetryStatus::OK;
    dualSidecar.channel_layout = audio::ChannelLayout::STEREO_LR;
    dualSidecar.sample_rate_hz = 48000;
    dualSidecar.chain_revision = 1;

    auto dualLane0 = smokeLane0;
    for (auto& bucket : dualLane0.buckets) {
        bucket.realization_id = dualRealizationId;
    }
    auto dualLane1 = dualLane0;
    for (std::size_t i = 0; i < dualLane1.buckets.size(); ++i) {
        auto& bucket = dualLane1.buckets[i];
        bucket.mean_reduction_db =
            std::max(
                0.0,
                bucket.mean_reduction_db
                    - 0.55 + 0.18 * std::sin(i * 0.09));
        bucket.end_reduction_db = bucket.mean_reduction_db;
        bucket.peak_reduction_db =
            std::max(
                bucket.mean_reduction_db,
                bucket.peak_reduction_db - 0.35);
    }
    dualSidecar.channel_lanes.push_back(std::move(dualLane0));
    dualSidecar.channel_lanes.push_back(std::move(dualLane1));

    activeTelemetrySidecar =
        std::make_shared<render::CompressorTelemetrySidecar>(
            std::move(dualSidecar));
    observedPlayback->audibleRealization.realizationId =
        dualRealizationId;
    observedPlayback->audibleRealization.handoffEndFrame = 0;

    const auto setEvidenceChannelLink =
        [&masteringChainState, &compressorViewModel](
            dsp::CompressorChannelLink link) {
            const auto& current =
                masteringChainState.compressor_parameters();
            auto updated = dsp::CompressorParameters::create(
                current.detector_mode(),
                link,
                current.threshold_dbfs(),
                current.ratio(),
                current.knee_db(),
                current.attack_ms(),
                current.release_ms(),
                current.rms_time_constant_ms(),
                current.look_ahead_ms(),
                current.mix_percent(),
                current.makeup_gain_db());
            QVERIFY(updated);
            QVERIFY(masteringChainState.set_compressor_parameters(
                *updated.value()));
            compressorViewModel.refreshFromAuthority();
        };

    setEvidenceChannelLink(dsp::CompressorChannelLink::DUAL_MONO);
    QCOMPARE(
        compressorViewModel.channel_link(),
        QStringLiteral("DUAL_MONO"));
    installTelemetryProviders();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    QCOMPARE(compressorViewModel.telemetry_num_lanes(), 2);
    const auto liveHistory =
        compressorViewModel.telemetry_history_lanes();
    QVERIFY(!liveHistory.isEmpty());
    QVERIFY(capture_visual_evidence(
        window,
        QStringLiteral(
            "compressor_gr_ribbon_dual_mono_1184x688.png"),
        QSize{1184, 688}));

    // Presentation seam must freeze, not fabricate, outside active wet
    // audition. Stage 3B2 owns the canonical state transitions.
    observedPlayback->state = core::PlaybackState::PAUSED;
    QTest::qWait(50);
    QCoreApplication::processEvents();
    QCOMPARE(
        compressorViewModel.telemetry_status(),
        QStringLiteral("PAUSED"));
    QCOMPARE(
        compressorViewModel.telemetry_history_lanes(),
        liveHistory);

    observedPlayback->state = core::PlaybackState::PLAYING;
    QVERIFY(auditionSelector.switch_to(
        app::AuditionTarget::PREPARED));
    QTest::qWait(50);
    QCoreApplication::processEvents();
    QCOMPARE(
        compressorViewModel.telemetry_status(),
        QStringLiteral("NOT AUDITIONED"));
    QCOMPARE(
        compressorViewModel.telemetry_history_lanes(),
        liveHistory);

    QVERIFY(auditionSelector.switch_to(
        app::AuditionTarget::PROCESSED));
    observedPlayback->state = core::PlaybackState::PLAYING;
    observedPlayback->position =
        core::FrameIndex{
            kSmokeBucketCount * kSmokeBucketFrames};
    observedPlayback->audibleRealization.phase =
        core::AudibleHandoffPhase::NEW;
    observedPlayback->audibleRealization.realizationId =
        dualRealizationId;
    observedPlayback->audibleRealization.handoffEndFrame = 0;
    QTest::qWait(50);
    QCoreApplication::processEvents();

    QVERIFY(masteringChainState.set_user_bypass(
        masteringChainState.compressor_instance_id(), true));
    compressorViewModel.refreshFromAuthority();
    // Authoritative bypass is immediate; the presentation telemetry is
    // refreshed by a 33 ms Qt timer. A fixed 50 ms sleep can miss a tick on
    // loaded Windows CI runners. Fail within a bounded deadline if the
    // resolver truly does not converge; do not weaken the BYPASS assertion.
    QVERIFY(compressorViewModel.bypass());
    QTRY_COMPARE_WITH_TIMEOUT(
        compressorViewModel.telemetry_status(),
        QStringLiteral("BYPASS"),
        1000);
    QCOMPARE(
        compressorViewModel.telemetry_history_lanes(),
        liveHistory);

    QVERIFY(masteringChainState.set_user_bypass(
        masteringChainState.compressor_instance_id(), false));
    compressorViewModel.refreshFromAuthority();
    setEvidenceChannelLink(
        dsp::CompressorChannelLink::LINKED_MAX);
    QVERIFY(auditionSelector.switch_to(
        app::AuditionTarget::PREPARED));

    // Compressor horizontal elasticity assertions
    const double curveWellWidth1184 = curveWellNative ? curveWellNative->width() : 0.0;
    const double controlsPanelWidth1184 = controlsPanelNative ? controlsPanelNative->width() : 0.0;
    QVERIFY2(curveWellWidth1184 >= 280.0, "curveWell width must be >= 280 px at 1184x688");
    QVERIFY2(controlsPanelWidth1184 >= 320.0, "controlsPanel width must be >= 320 px at 1184x688");

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

    for (const auto& name : {
             QStringLiteral("compressorThresholdSlider"),
             QStringLiteral("compressorRatioSlider"),
             QStringLiteral("compressorKneeSlider"),
             QStringLiteral("compressorAttackSlider"),
             QStringLiteral("compressorReleaseSlider"),
             QStringLiteral("compressorRmsTimeSlider"),
             QStringLiteral("compressorLookAheadSlider"),
             QStringLiteral("compressorMixSlider"),
             QStringLiteral("compressorMakeupSlider")}) {
        auto* sliderItem = qobject_cast<QQuickItem*>(find_child_by_name(compressorEditor, name));
        QVERIFY2(sliderItem != nullptr, qPrintable(name + QStringLiteral(" must exist")));
        QVERIFY2(check_item_contained_in_ancestor(sliderItem, controlsPanelNative),
            qPrintable(name + QStringLiteral(" must stay inside Compressor controls at minimum size")));
    }

    // Graph-linked semantic slider optics & track fill decoupling assertions
    auto* thresholdSlider = find_child_by_name(compressorEditor, QStringLiteral("compressorThresholdSlider"));
    auto* ratioSlider = find_child_by_name(compressorEditor, QStringLiteral("compressorRatioSlider"));
    auto* kneeSlider = find_child_by_name(compressorEditor, QStringLiteral("compressorKneeSlider"));
    auto* makeupSlider = find_child_by_name(compressorEditor, QStringLiteral("compressorMakeupSlider"));
    QVERIFY2(thresholdSlider && ratioSlider && kneeSlider && makeupSlider, "Compressor graph-linked sliders must exist");

    QVERIFY2(thresholdSlider->property("hasSemanticAccent").toBool(), "Threshold slider must have hasSemanticAccent = true");
    QVERIFY2(ratioSlider->property("hasSemanticAccent").toBool(), "Ratio slider must have hasSemanticAccent = true");
    QVERIFY2(kneeSlider->property("hasSemanticAccent").toBool(), "Knee slider must have hasSemanticAccent = true");
    QVERIFY2(makeupSlider->property("hasSemanticAccent").toBool(), "Make-up slider must have hasSemanticAccent = true");

    QCOMPARE(thresholdSlider->property("semanticAccent").value<QColor>(), QColor{QStringLiteral("#2ED3FF")});
    QCOMPARE(ratioSlider->property("semanticAccent").value<QColor>(), QColor{QStringLiteral("#2FD98F")});
    QCOMPARE(kneeSlider->property("semanticAccent").value<QColor>(), QColor{QStringLiteral("#FFD84A")});
    QCOMPARE(makeupSlider->property("semanticAccent").value<QColor>(), QColor{QStringLiteral("#FF6B6B")});

    // Non-graph sliders must NOT have persistent semantic halos
    auto* attackSlider = find_child_by_name(compressorEditor, QStringLiteral("compressorAttackSlider"));
    auto* releaseSlider = find_child_by_name(compressorEditor, QStringLiteral("compressorReleaseSlider"));
    auto* rmsTimeSlider = find_child_by_name(compressorEditor, QStringLiteral("compressorRmsTimeSlider"));
    auto* lookAheadSlider = find_child_by_name(compressorEditor, QStringLiteral("compressorLookAheadSlider"));
    auto* mixSlider = find_child_by_name(compressorEditor, QStringLiteral("compressorMixSlider"));
    QVERIFY2(attackSlider && releaseSlider && rmsTimeSlider && lookAheadSlider && mixSlider, "Compressor non-graph sliders must exist");

    QVERIFY2(!attackSlider->property("hasSemanticAccent").toBool(), "Attack slider must have hasSemanticAccent = false");
    QVERIFY2(!releaseSlider->property("hasSemanticAccent").toBool(), "Release slider must have hasSemanticAccent = false");
    QVERIFY2(!rmsTimeSlider->property("hasSemanticAccent").toBool(), "RMS Time slider must have hasSemanticAccent = false");
    QVERIFY2(!lookAheadSlider->property("hasSemanticAccent").toBool(), "Lookahead slider must have hasSemanticAccent = false");
    QVERIFY2(!mixSlider->property("hasSemanticAccent").toBool(), "Mix slider must have hasSemanticAccent = false");

    const QColor copperFill = thresholdSlider->property("positionFillColor").value<QColor>();
    QCOMPARE(ratioSlider->property("positionFillColor").value<QColor>(), copperFill);
    QCOMPARE(kneeSlider->property("positionFillColor").value<QColor>(), copperFill);
    QCOMPARE(makeupSlider->property("positionFillColor").value<QColor>(), copperFill);
    QCOMPARE(attackSlider->property("positionFillColor").value<QColor>(), copperFill);

    QVERIFY2(copperFill != thresholdSlider->property("semanticAccent").value<QColor>(),
        "Local semanticAccent and track fill must be decoupled");
    QCOMPARE(copperFill.red(), 196);
    QCOMPARE(copperFill.green(), 119);
    QCOMPARE(copperFill.blue(), 74);
    QVERIFY2(qAbs(copperFill.alphaF() - 0.38) <= 0.01, "Track fill alpha must correspond to FQ #43 0.38");

    QVERIFY2(makeupSlider->property("fillFromOrigin").toBool(), "Make-up slider fillFromOrigin must be true");
    QCOMPARE(makeupSlider->property("positionFillOrigin").toDouble(), 0.0);
    // Shared geometry correction is ALSO audited on Compressor MAKE-UP,
    // not only on the new M15 editor.
    QVERIFY2(bipolar_fill_matches_handle(
        makeupSlider, QStringLiteral("studioSliderTrack"),
        QStringLiteral("studioSliderTrackFill"),
        QStringLiteral("studioSliderThumb"), 0.5),
        "Compressor MAKE-UP centered fill must use the same handle geometry");

    QCOMPARE(compressorViewModel.transfer_curve_handles().size(), 4);
    auto* thresholdFieldCheck = find_child_by_name(compressorEditor, QStringLiteral("compressorThresholdField"));
    QVERIFY(thresholdFieldCheck && thresholdFieldCheck->property("interactionHint").toString().contains(QStringLiteral("curve point")));

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

    // Interaction qualification intentionally runs AFTER all visual evidence.
    // A real handle drag requests a new preview by design; keeping it last
    // prevents that expected side effect from contaminating evidence PNGs.
    QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 2));
    QTest::qWait(50);
    QCoreApplication::processEvents();

    // Test MAKE-UP handle direct pointer manipulation in the separated editing surface
    compressorViewModel.setMakeupGainDb(-18.0);
    QCoreApplication::processEvents();
    QCOMPARE(compressorViewModel.makeup_gain_db(), -18.0);

    const auto handles = compressorViewModel.transfer_curve_handles();
    QVariantMap makeupHandleMap;
    for (const auto& h : handles) {
        if (h.toMap()[QStringLiteral("id")].toString() == QStringLiteral("makeup")) {
            makeupHandleMap = h.toMap();
            break;
        }
    }
    QVERIFY2(!makeupHandleMap.isEmpty(), "MAKE-UP handle metadata must exist");
    const double makeupInDbfs = makeupHandleMap[QStringLiteral("inputDbfs")].toDouble();
    const double makeupOutDbfs = makeupHandleMap[QStringLiteral("outputDbfs")].toDouble();

    const double minX = compressorCurveCanvas->property("plotXMinDbfs").toDouble();
    const double maxX = compressorCurveCanvas->property("plotXMaxDbfs").toDouble();
    const double minY = compressorCurveCanvas->property("plotYMinDbfs").toDouble();
    const double maxY = compressorCurveCanvas->property("plotYMaxDbfs").toDouble();

    const double hx = (makeupInDbfs - minX) / (maxX - minX) * curveCanvasNative->width();
    const double hy = curveCanvasNative->height() - ((makeupOutDbfs - minY) / (maxY - minY) * curveCanvasNative->height());

    const QPointF handleScenePos = curveCanvasNative->mapToScene(QPointF{hx, hy});

    window->requestActivate();
    QTest::qWait(20);
    QCoreApplication::processEvents();

    const QPoint pressPoint = handleScenePos.toPoint();
    const QPoint dragPoint = pressPoint + QPoint(0, -30);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, pressPoint);
    QTest::qWait(20);
    QCoreApplication::processEvents();

    QTest::mouseMove(window, dragPoint);
    QTest::qWait(20);
    QCoreApplication::processEvents();

    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, dragPoint);
    QTest::qWait(20);
    QCoreApplication::processEvents();

    QVERIFY2(compressorViewModel.makeup_gain_db() > -18.0,
        "Pointer drag over MAKE-UP handle must modify makeupGainDb in the separated transfer-curve surface");

    compressorViewModel.setMakeupGainDb(0.0);
    QCoreApplication::processEvents();

    // View's menu is driven by the exact optional topology, NOT hardcoded
    // as a permanently enabled fourth slot. Prove it is absent for legacy
    // projects, then becomes enabled/selectable on real 4-node adoption.
    QVERIFY(!root->findChild<QObject*>(
        QStringLiteral("menuViewStereoMs")));
    const auto stereoUuid = *core::Uuid::parse(
        "55555555-5555-4555-8555-555555555555").value();
    const auto stereoId = *dsp::ModuleInstanceId::from_uuid(stereoUuid).value();
    auto stereoParams = dsp::StereoMsParameters::create_default();
    QVERIFY(stereoParams);
    auto optIn = app::MasteringChainState::create_with_stereo_ms(
        *moduleRegistry.value(), chainUuid, gainId, eqId, compId,
        stereoId, *stereoParams.value(), false);
    QVERIFY(optIn);
    masteringChainState = std::move(*optIn.value());
    dspChainAdapterModel.refreshFromAuthority();
    QTRY_COMPARE_WITH_TIMEOUT(
        viewEditorInstantiator->property("count").toInt(), 4, 5000);
    auto* stereoMenuItem = root->findChild<QObject*>(
        QStringLiteral("menuViewStereoMs"));
    QVERIFY2(stereoMenuItem,
        "View must expose Stereo/M-S when the fourth module is opted in");
    QVERIFY2(stereoMenuItem->property("enabled").toBool(),
        "The fourth View action must be selectable, not a placeholder");
    QVERIFY2(stereoMenuItem->property("text").toString().contains(
        QStringLiteral("Stereo")),
        "The fourth View entry must label the live Stereo/M-S adapter");
    QCOMPARE(cleanupViewMenu->property("count").toInt(),
             viewEditorInstantiator->property("count").toInt() + 7);
    QVERIFY2(QMetaObject::invokeMethod(stereoMenuItem, "triggered"),
        "The fourth View entry must dispatch selection");
    QCoreApplication::processEvents();
    QCOMPARE(dspChainAdapterModel.selected_index(), 3);
    auto* stereoEditorFromMenu = dspEditorHostObj->findChild<QObject*>(
        QStringLiteral("stereoMsEditor"));
    QVERIFY2(stereoEditorFromMenu && stereoEditorFromMenu->property("visible").toBool(),
        "Selecting Stereo/M-S from View must open its real editor");

    // M15 was previously opt-in and is now proposed for new-product startup:
    // verify actual ViewModel error routing from the fourth selected DSP
    // through the SINGLE fixed global bar, without a duplicate header message
    // or a panel resize. No fabricated audible data are used.
    stereoMsViewModel.refreshFromAuthority();
    QCoreApplication::processEvents();
    auto* stereoEditorItem = qobject_cast<QQuickItem*>(stereoEditorFromMenu);
    auto* localMsError = qobject_cast<QQuickItem*>(
        find_child_by_name(stereoEditorFromMenu,
            QStringLiteral("stereoMsValidationFeedbackText")));
    auto* msControls = qobject_cast<QQuickItem*>(
        find_child_by_name(stereoEditorFromMenu,
            QStringLiteral("stereoMsControlsRegion")));
    auto* globalStatusBarItem = qobject_cast<QQuickItem*>(statusBar);
    QVERIFY(stereoEditorItem && localMsError && msControls && globalStatusBarItem);
    QVERIFY(!localMsError->isVisible());
    // capture_visual_evidence() changes the native window size to its
    // requested geometry. Normalize BEFORE the baseline rectangle so a
    // resize is never mistaken for an error-induced DSP layout shift.
    window->resize(QSize{1184, 688});
    QTest::qWait(120);
    QCoreApplication::processEvents();
    // Qt Quick's deferred polish can finish after a native resize even
    // when processEvents() has returned. Render the actual host window,
    // then require successive painted-frame rectangles to settle BEFORE
    // establishing the no-reflow baseline. Do not assert an absolute width
    // or Y coordinate: the Windows CI HyperVMonitor can clamp a requested
    // 1184px top-level window to 1028px. The exact geometry equality
    // checks across error and correction below remain strict.
    auto* paintedWindow = qobject_cast<QQuickWindow*>(window);
    QVERIFY(paintedWindow);
    QRectF lastPaintedControls;
    int sameFrameGeometryCount = 0;
    QTRY_VERIFY_WITH_TIMEOUT(([&] {
        if (paintedWindow->grabWindow().isNull())
            return false;
        QCoreApplication::processEvents();
        const QRectF current{
            msControls->mapToScene(QPointF{}), msControls->size()};
        if (current.isEmpty()) {
            sameFrameGeometryCount = 0;
            return false;
        }
        sameFrameGeometryCount =
            current == lastPaintedControls ? sameFrameGeometryCount + 1 : 0;
        lastPaintedControls = current;
        return sameFrameGeometryCount >= 2;
    })(), 5000);
    const QRectF controlsBefore{
        msControls->mapToScene(QPointF{}), msControls->size()};
    const QRectF footerBefore{
        globalStatusBarItem->mapToScene(QPointF{}), globalStatusBarItem->size()};
    const auto msReadoutBefore = stereoMsViewModel.mid_gain_db();
    QVERIFY2(!stereoMsViewModel.setDraftFieldText(
        QStringLiteral("midGainDb"), QStringLiteral("not-a-number")),
        "An invalid M15 draft must be rejected without changing sonic state");
    QCoreApplication::processEvents();
    QVERIFY2(!stereoMsViewModel.validation_message().isEmpty(),
        "Stereo/M-S typed ViewModel must report the invalid field");
    QCOMPARE(stereoMsViewModel.mid_gain_db(), msReadoutBefore);
    // Only the owning numeric field is marked invalid; the other four M15
    // scalar inputs stay neutral. Expose the complete diagnostic in the
    // invalid field's tooltip even when the global footer text is elided.
    auto* invalidMidField = find_child_by_name(
        stereoEditorFromMenu, QStringLiteral("stereoMsMidGainField"));
    QVERIFY(invalidMidField);
    QCOMPARE(stereoMsViewModel.validation_field(), QStringLiteral("midGainDb"));
    QVERIFY(invalidMidField->property("isInvalid").toBool());
    QCOMPARE(invalidMidField->property("validationTooltip").toString(),
        stereoMsViewModel.validation_message());
    for (const auto& unaffected : {
             QStringLiteral("stereoMsWidthField"),
             QStringLiteral("stereoMsCutoffField"),
             QStringLiteral("stereoMsLowWidthField"),
             QStringLiteral("stereoMsSideGainField")}) {
        auto* field = find_child_by_name(stereoEditorFromMenu, unaffected);
        QVERIFY2(field, qPrintable(unaffected + QStringLiteral(" missing")));
        QVERIFY2(!field->property("isInvalid").toBool(),
            qPrintable(unaffected + QStringLiteral(" must not turn coral")));
    }
    QCOMPARE(root->property("statusSeverity").toString(),
        QStringLiteral("ERROR"));
    QVERIFY2(root->property("statusDspErrorText").toString().contains(
        QStringLiteral("Stereo"), Qt::CaseInsensitive),
        "Selected M15 validation must reach the global bottom status bar");
    QCOMPARE(statusIndicator->property("color").value<QColor>(),
        QColor{QStringLiteral("#F27683")});
    QCOMPARE(statusBarMessage->property("color").value<QColor>(),
        QColor{QStringLiteral("#F27683")});
    QVERIFY(!localMsError->isVisible());
    QCOMPARE(QRectF(msControls->mapToScene(QPointF{}), msControls->size()),
        controlsBefore);
    QCOMPARE(QRectF(globalStatusBarItem->mapToScene(QPointF{}),
        globalStatusBarItem->size()), footerBefore);
    QCOMPARE(statusBar->property("height").toInt(), 24);
    QVERIFY2(capture_visual_evidence(window,
        QStringLiteral("m15_global_status_stereo_invalid_1184x688.png"),
        QSize{1184, 688}),
        "Capture real Qt Stereo/M-S invalid-field global status");

    // Compare the actual validation diagnostic, not the generic word
    // "Stereo": a stereo Source legitimately appears in the Ready footer.
    // A retained M15 invalid draft MUST NOT contaminate another DSP's
    // global footer; returning to Stereo/M-S must restore that exact
    // unapplied error without mutating its audible parameters.
    const QString m15Diagnostic = root->property("statusDspErrorText").toString();
    QVERIFY(!m15Diagnostic.isEmpty());
    QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 0));
    QCoreApplication::processEvents();
    QCOMPARE(dspChainAdapterModel.selected_index(), 0);
    QVERIFY(root->property("statusDspErrorText").toString().isEmpty());
    const QString selectedGainFooter = statusBarMessage->property("text").toString();
    QCOMPARE(selectedGainFooter, root->property("statusText").toString());
    QVERIFY2(!selectedGainFooter.contains(m15Diagnostic),
        "Inactive Stereo/M-S validation diagnostic must not replace the selected Gain status");
    QVERIFY2(!selectedGainFooter.contains(stereoMsViewModel.validation_message()),
        "Inactive M15 validation message must not leak into the Source/Ready footer");
    QCOMPARE(root->property("statusSeverity").toString(), QStringLiteral("INFO"));
    QCOMPARE(QRectF(globalStatusBarItem->mapToScene(QPointF{}),
                     globalStatusBarItem->size()), footerBefore);
    QVERIFY(dspWorkspaceObj->setProperty("selectedModuleIndex", 3));
    QCoreApplication::processEvents();
    QCOMPARE(dspChainAdapterModel.selected_index(), 3);
    QVERIFY(root->property("statusDspErrorText").toString().contains(
        QStringLiteral("Stereo"), Qt::CaseInsensitive));
    QVERIFY(invalidMidField->property("isInvalid").toBool());
    QCOMPARE(stereoMsViewModel.mid_gain_db(), msReadoutBefore);
    QCOMPARE(QRectF(msControls->mapToScene(QPointF{}), msControls->size()),
             controlsBefore);
    QCOMPARE(QRectF(globalStatusBarItem->mapToScene(QPointF{}),
                     globalStatusBarItem->size()), footerBefore);
    stereoMsViewModel.cancelDraft();
    QCoreApplication::processEvents();
    QCOMPARE(stereoMsViewModel.validation_message(), QString{});
    QVERIFY(!invalidMidField->property("isInvalid").toBool());
    QVERIFY(invalidMidField->property("validationTooltip").toString().isEmpty());
    QVERIFY(root->property("statusDspErrorText").toString().isEmpty());
    QCOMPARE(root->property("statusSeverity").toString(),
        QStringLiteral("INFO"));
    QCOMPARE(statusIndicator->property("color").value<QColor>(),
        QColor{QStringLiteral("#00E6E6")});
    QCOMPARE(QRectF(msControls->mapToScene(QPointF{}), msControls->size()),
        controlsBefore);
    QCOMPARE(QRectF(globalStatusBarItem->mapToScene(QPointF{}),
        globalStatusBarItem->size()), footerBefore);
    QVERIFY2(capture_visual_evidence(window,
        QStringLiteral("m15_global_status_stereo_corrected_1184x688.png"),
        QSize{1184, 688}),
        "Capture real Qt Stereo/M-S corrected global status");

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
