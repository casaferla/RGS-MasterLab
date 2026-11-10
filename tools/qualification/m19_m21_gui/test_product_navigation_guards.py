#!/usr/bin/env python3
"""M17→M19→M20→M21→MONITOR Product navigation source guard.

Nonrelease smoke. Does not certify numerical M19/M20/M21 Phase-6.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_visual_order_and_no_chain_mutation() -> None:
    c = read("app/src/dsp_chain_adapter_model.cpp")
    before = c.index("// Terminal instance is separate from the MANUAL")
    m17 = c.index("terminalLimiterState_ && terminalLimiterViewModel_", before)
    new = c.index("for(const int milestone:{19,20,21})", m17)
    after = c.index("if (terminalMonitorViewModel_)", new)
    assert m17 < new < after
    assert "ProductAnalysisModuleAdapter" in c
    assert "MasteringChainState::insert" not in c
    adapter = read("app/src/dsp_module_adapter.hpp")
    part = adapter[adapter.index("class ProductAnalysisModuleAdapter"):
                   adapter.index("class MonitorServiceModuleAdapter")]
    for term in ("non_processing_service() const noexcept override{return true;}",
                 "bypass_supported() const noexcept override{return false;}",
                 "void setBypass(bool) override {}",
                 "QStringLiteral(\"Not yet qualified\")"):
        assert term in part, term


def test_product_binds_status_without_demo() -> None:
    main = read("app/src/main.cpp")
    assert "bindAnalysisStatusViewModel(\n        &m21ProductStatusViewModel)" in main
    assert "m21ProductStatusViewModel" in read("ui/qml/Main.qml")
    editor = read("ui/qml/AnalysisStatusEditor.qml")
    assert "productionAuthorized === true" in editor
    assert "selectPanelSource(" in editor
    assert "fixtureMode" not in editor
    assert "AnalysisApprovedContent {" in editor
    assert 'objectName: "analysisApprovedContent"' in editor
    visuals = read("ui/qml/AnalysisApprovedContent.qml")
    # Preserve the original explicit unavailable visual gate, now owned
    # by the approved Product read-only content rather than the wrapper.
    assert "VERIFIED ANALYSIS UNAVAILABLE" in visuals
    assert "fixtureMode" not in visuals
    # Genuine development measurements now come from the one Product
    # inbox; the release Gate G property remains separately disabled.
    assert 'rawText: root.metricText(' in visuals
    assert 'dataAvailable: root.hasNumber(' in visuals
    assert 'm20StaticPsd' in visuals
    assert 'm21Rows' in visuals
    assert 'Number.isFinite(value)' in visuals
    assert 'StudioTooltipDwell {' in visuals
    assert visuals.count('StudioTooltipDwell {') >= 4
    assert 'ToolTip.visible:' in visuals
    assert 'root.metricExplanation(m21MetricRow.observed.key)' in visuals
    for metric in ("lufs", "rms", "samplePeak", "truePeak",
                   "crest", "width", "P10", "P50", "P90"):
        assert 'case "' + metric + '":' in visuals
    assert 'REFERENCE minus PREPARED or PROCESSED' in visuals
    # Distinguish absolute source metrics from signed REFERENCE−CURRENT
    # deltas; do not display rounded-away positive values as +0.00.
    assert 'function deltaText(value, key)' in visuals
    assert 'if(Number(shown) === 0)' in visuals
    assert 'return value > 0 ? "+" + shown : shown' in visuals
    assert 'root.deltaText(m21MetricRow.observed.deltaPrepared,' in visuals
    assert 'root.deltaText(m21MetricRow.observed.deltaProcessed,' in visuals
    assert 'root.metricText(m21MetricRow.observed.reference,' in visuals
    assert 'ToolTip.visible: sourceChoiceDwell.ready' in editor
    assert editor.count('StudioTooltipDwell {') >= 1
    dwell = read("ui/qml/StudioTooltipDwell.qml")
    assert 'interval: 1000' in dwell
    assert 'onPointChanged: guard.resetDwell()' in dwell
    assert 'onHoveredChanged: guard.resetDwell()' in dwell
    assert 'dataAvailable: true' not in visuals
    assert 'fixtureMode' not in visuals
    assert "m19MetricValue_" in visuals and "heroReadout: true" in visuals
    assert "m20WorkbenchSpectrum" in visuals
    assert "m20MissingSpectrumNotice" in visuals
    assert "m20MissingProfileNotice" in visuals
    assert "m21MissingComparisonNotice" in visuals
    assert "m21MetricRow_" in visuals
    # PO-approved 16px vectors: only three analysis services, never
    # inherited meter/stereo placeholders or a new M18/M22 chain row.
    selector = read("ui/qml/DspChainSelector.qml")
    vectors = read("ui/qml/AnalysisModuleIcon.qml")
    for key,kind in (("M19_ANALYSIS","analysis-levels"),
                     ("M20_ANALYSIS","analysis-spectrum"),
                     ("M21_ANALYSIS","analysis-delta")):
        assert f'case "{key}": return "{kind}"' in selector
        assert f'kind === "{kind}"' in vectors
    assert 'AnalysisModuleIcon {' in selector
    # M16 and the three nonbypassable analyzer services must share the
    # exact same row/halo behavior. Missing measurements affect only the
    # editor's value cells, NEVER dim the permanent analysis row or icon.
    assert '&& moduleAdapter.nonProcessingService' in selector
    assert 'readonly property bool permanentlyLit:' in selector
    assert 'readonly property bool isBypassed:' in selector
    assert '(moduleAdapter.bypassSupported && moduleAdapter.bypass)' in selector
    assert 'glowEnabled: (rowItem.permanentlyLit' in selector
    assert '|| !rowItem.isDefaultState) && !rowItem.isBypassed' in selector
    assert 'opacity: (rowItem.permanentlyLit ? 1.0' in selector
    assert ': rowItem.isDefaultState ? 0.58 : 1.0)' in selector
    assert '* (rowItem.isBypassed ? 0.7 : 1.0)' in selector
    assert 'opacity: rowItem.isBypassed ? 0.28 : 1.0' in selector
    assert 'Always on · Read-only analysis / service' in selector
    assert 'rowItem.permanentlyLit\n' in selector
    assert 'glowEnabled: moduleSymbol.glowEnabled' in selector
    assert 'strokeColor: moduleSymbol.strokeColor' in selector
    # The 16px slot belongs to the EXISTING StudioIcon in the selector;
    # the 24px vector master is centered in its own Shape component.
    assert 'anchors.fill: parent' in selector
    assert 'anchors.centerIn: parent' in vectors
    assert 'import QtQuick.Shapes' in vectors
    assert 'PathSvg { path: root.vectorPath }' in vectors
    assert 'readonly property bool small:' in vectors
    assert 'Math.min(root.width,root.height)/root.designSize' in vectors
    assert 'module-output-dither' in vectors
    assert 'case "M18_ANALYSIS"' not in selector
    assert 'case "M22_ANALYSIS"' not in selector
    assert "Image {" not in vectors and ".png" not in vectors
    assert "qml/AnalysisModuleIcon.qml" in read("ui/CMakeLists.txt")
    # Real Product dispatch remains ONE flight; no workers own QObjects
    # or publish private metrics into QML before Phase-6 U/G/owner gates.
    driver = read("app/src/internal/m21_private_product_analysis_coordinator.hpp")
    single_flight = read("app/src/internal/m21_private_single_flight.hpp")
    main = read("app/src/main.cpp")
    assert "M21PrivateProductAnalysisCoordinator" in main
    assert "m21AnalysisPollTimer.setInterval(250);" in main
    view_cpp = read("app/src/m21_product_status_view_model.cpp")
    bridge = read("app/src/internal/m21_product_qt_readiness_bridge.hpp")
    assert "Bridge::same_visible_frame(frame_,next)" in view_cpp
    assert "a.panels==b.panels" in bridge
    assert "a.owner_identity==b.owner_identity" in bridge
    assert "MasterOutputRateState::changed" in main
    assert "SourceSelectionViewModel::sourceChanged" in main
    assert "Qt::QueuedConnection" in main
    for token in (
        "a.routing_revision==b.routing_revision",
        "a.selected==b.selected",
        "a.identities==b.identities",
        "a.panels==b.panels",
    ):
        assert token in bridge
    smoke = read("tests/ui_smoke/test_source_metadata_panel.cpp")
    for slot in (
        "m21ReadinessFrameDetectsMasterRateIdentity",
        "m21ReadinessFrameDetectsProjectAndSourceGenerations",
        "m21ReadinessFrameDetectsReferenceScope",
        "m21ReadinessFrameDetectsQualityWithoutNumbers",
    ):
        assert slot in smoke
    assert "TerminalLimiterViewModel::acceptedStateChanged" in main
    for test_name in ("m21IdenticalReadinessFrameDoesNotRequireRepaint",
                      "m21ReadinessFrameDetectsProductMutation",
                      "m21ReadinessFrameDetectsPanelRoutingAndQuality"):
        assert test_name in smoke

    assert "Product::capture_gold(" in driver
    assert "Product::capture(" in driver
    assert "Product::measure_gold_worker(evidence)" in driver
    assert "Product::measure_render_worker(evidence,column)" in driver
    assert "inbox_->accept_gold(" in driver
    assert "inbox_->accept_render(" in driver
    assert "std::thread([state,task=std::move(task)]" in single_flight
    assert "state_->finished.load(std::memory_order_acquire)" in single_flight
    # Private M21 evidence validator is structural, not a second
    # canonical Phase-6 registry or numeric UI publication authority.
    integrity = read("app/src/internal/m21_metrics_comparison_candidate.hpp")
    for required in (
        "valid_private_spectrum(const Snapshot& s)",
        "valid_private_tonal_profile(const Snapshot& s)",
        "point.hz<=previous",
        "point.hz>nyquist",
        "s.tonal_profile[j].band_id==b.band_id",
        "!valid_private_tonal_profile(incoming)",
        "!valid_private_spectrum(incoming)",
    ):
        assert required in integrity
    native = read("tests/unit/app/test_m23_alpha_diagnostic_report.cpp")
    for required in (
        "m21PsdRejectsDuplicateAndDescending",
        "m21PsdRejectsOutOfBandAndNonFinite",
        "m21TonalRejectsDuplicateIds",
        "m21PublicationStaysStaleAfterMalformedResult",
        "m21PublicationPreservesIndependentGoldScope",
        "m21PublicationRejectsRelabeledColumnAndUnknownRole",
    ):
        assert required in native
    assert "production_authorized() const noexcept {return false;}" in read(
        "app/src/m21_product_status_view_model.hpp")
    # PO authorized implementation work, NOT Gate G. The existing status
    # ViewModel can stage authentic scalar/delta/PSD projections, but its
    # Qt release property is provably empty until canonical authorization.
    status_header = read("app/src/m21_product_status_view_model.hpp")
    status_cpp = read("app/src/m21_product_status_view_model.cpp")
    assert "Q_PROPERTY(QVariantMap phase6ReleaseReadouts" in status_header
    assert "Q_PROPERTY(QVariantMap analysisReadouts" in status_header
    assert "phase6_release_readouts() const" in status_header
    assert "analysis_readouts() const" in status_header
    assert "M21ProductStatusViewModel::phase6_release_readouts() const" in status_cpp
    assert "return production_authorized() ? analysis_readouts() : QVariantMap{};" in status_cpp
    assert "M21ProductStatusViewModel::analysis_readouts() const" in status_cpp
    assert "if(!owner_thread() || !inbox_)" in status_cpp
    assert "inbox_->view_current(" in status_cpp
    assert "metric==3U" in status_cpp
    assert "cell.quality!=m21_private::Quality::present" in status_cpp
    assert "evidence.prepared_to_gold[i]" in status_cpp
    assert "evidence.processed_to_gold[i]" in status_cpp
    assert "m20StaticPsd" in status_cpp
    assert "QVariant::fromValue(*cell.unrounded)" in status_cpp
    # PSD must satisfy the same live resource/scope requirements as
    # scalar values; the status-only bridge remains numerical-free.
    assert "s.generation_token!=0 &&!s.resource_identity.empty()" in status_cpp
    assert "s.scope_frames>0 &&s.sample_rate_hz>0" in status_cpp
    assert "s.average_spectrum.size()<=65537U" in status_cpp
    # Status-polling bridge must still copy ONLY identity/quality, not PSD.
    assert "inbox.view_current_readiness(" in bridge
    assert "inbox.view_current(" not in bridge
    status_qml = read("ui/qml/AnalysisStatusEditor.qml")
    assert "phase6ReleaseReadouts" not in status_qml
    assert "analysisReadouts" in status_qml
    assert "observedReadouts: root.observedReadouts" in status_qml
    assert 'ANALYSIS · DEVELOPMENT' in status_qml

    assert "M21PrivateSingleFlight::matches_expected(" in driver
    assert "completed.result->column==column" in single_flight
    assert "completed.result->generation_token==generation" in single_flight
    assert "completed.result->lifecycle==m21_private::Lifecycle::current" in single_flight
    assert "append_identity_field(" in driver
    # Status-only Qt consumer must never copy full PSD/tonal/Cell values.
    inbox = read("app/src/internal/m21_product_publication_inbox.hpp")
    integrity = read("app/src/internal/m21_metrics_comparison_candidate.hpp")
    bridge = read("app/src/internal/m21_product_qt_readiness_bridge.hpp")
    assert "struct ReadinessSnapshot final" in integrity
    assert "std::array<Quality,9> metric_quality" in integrity
    assert "readiness_columns() const" in integrity
    assert "view_current_readiness(" in inbox
    assert "retire_stale(latest,product,fanout,selector,gold)" in inbox
    assert "retire_stale(latest.columns,product,fanout,selector,gold)" in inbox
    assert "inbox.view_current_readiness(" in bridge
    assert "s.metric_quality[metric]" in bridge
    assert "s.metrics[metric].quality" not in bridge
    native = read("tests/unit/app/test_m23_alpha_diagnostic_report.cpp")
    for test_name in (
        "m21ReadinessReceiptIsSmallerThanNumericEvidence",
        "m21ReadinessRetainsExactRoleAndScope",
        "m21ReadinessDoesNotCloneSpectrumOrTonalProfile",
        "m21ReadinessTracksNineDistinctMetricQualities",
        "m21ReadinessRetiresStaleGenerationWithoutAffectingReference",
        "m21ReadinessRejectsInvalidReplacementWithoutCurrentStatus",
        "m21ReadinessNeverRelabelsAnUnknownColumn",
        "m21ReadinessIsOnlyCopyOfIdentityAndQuality",
    ):
        assert test_name in native
    assert "append_identity_field(" in single_flight
    # Keep the COMPLETE Product-epoch guard and add identity of immutable
    # retained render PCM: a new PREPARED/PROCESSED owner with the SAME
    # epoch must not be skipped, and the old PCM must not be pinned.
    for token in (
        "std::weak_ptr<const void> previous_",
        "prior.get()==owner.get()",
        "key_==key",
    ):
        assert token in single_flight
    assert "renderAttempts_[index].matches(key,render)" in driver
    assert "renderAttempts_[index].remember(std::move(key),render)" in driver
    native = read("tests/unit/app/test_m23_alpha_diagnostic_report.cpp")
    for required in (
        "m21RenderAttemptDeduplicatesSamePhysicalOwner",
        "m21RenderAttemptAcceptsReplacementInsideSameEpoch",
        "m21RenderAttemptDoesNotPinRetiredPcm",
        "m21RenderAttemptChangesKeysAndSupportsReset",
        "m21RenderAttemptNoOwnerIsNeverCurrent",
    ):
        assert required in native
    assert 'return false;' in read("app/src/m21_product_status_view_model.hpp")
    readout = read("ui/qml/StudioReadOnlyMeasurement.qml")
    assert "root.heroReadout ? 36 : 13" in readout
    assert "root.heroReadout ? 11 : 10" in readout
    assert "readonly property bool readOnly: true" in readout
    # Permanent M16-pattern pages: exactly one M19/M20/M21 instance per
    # host, created declaratively once and kept when a different DSP is
    # selected. The existing qualified ColumnLayout is the geometry owner;
    # hiding the page excludes it from layout allocation.
    host = read("ui/qml/DspEditorHost.qml")
    main_qml = read("ui/qml/Main.qml")
    names = (
        ("m19LevelProductAnalysisEditor", "M19_ANALYSIS", 0),
        ("m20TonalStereoProductAnalysisEditor", "M20_ANALYSIS", 1),
        ("m21MetricsDeltaProductAnalysisEditor", "M21_ANALYSIS", 2),
    )
    assert host.count("AnalysisStatusEditor {") == 3
    assert 'objectName: "m19M21DockSlotAnalysisLoader"' not in main_qml
    assert "property var productAnalysisSurface" not in host
    assert "productAnalysisFactory" not in host
    assert ".createObject(" not in host
    assert ".destroy()" not in host
    assert "onActiveModuleChanged: root.syncProductAnalysisSurface()" not in host
    assert host.index('objectName: "truePeakLimiterEditor"') < host.index(
        'objectName: "m19LevelProductAnalysisEditor"'
    )
    assert host.index('objectName: "m21MetricsDeltaProductAnalysisEditor"') < host.index(
        'objectName: "monitorEditor"'
    )
    for name, key, index in names:
        assert host.count('objectName: "' + name + '"') == 1
        start = host.index('objectName: "' + name + '"')
        end = host.index("\n        }", start)
        block = host[start:end]
        for fragment in (
            "panelIndex: " + str(index),
            'editorContentKey === "' + key + '"',
            "statusViewModel: root.analysisStatusViewModel",
            "enabled: visible",
            "Layout.fillWidth: true",
            "Layout.fillHeight: true",
            "Layout.minimumWidth: 0",
            "Layout.minimumHeight: 0",
            "Layout.preferredHeight: 0",
        ):
            assert fragment in block, (name, fragment)
    assert host.index('objectName: "m19LevelProductAnalysisEditor"') < host.index(
        'objectName: "m20TonalStereoProductAnalysisEditor"'
    )
    assert host.index('objectName: "m20TonalStereoProductAnalysisEditor"') < host.index(
        'objectName: "m21MetricsDeltaProductAnalysisEditor"'
    )
    ui = read("ui/CMakeLists.txt")
    assert "qml/AnalysisStatusEditor.qml" in ui
    assert "qml/M19M21AnalysisWorkbenchPreview.qml" not in ui
    preview = read("ui/qml/M19M21AnalysisWorkbenchPreview.qml")
    assert "fixtureMode: true" in preview


def test_export_permission_unchanged() -> None:
    qml = read("ui/qml/Main.qml")
    # FQ99 Main status-bar chrome is frozen: export diagnostics belong to
    # evidence and ViewModels, not a new layout-consuming status label.
    assert "root.appM18ExportModel.canExport" in qml
    assert 'objectName: "m18ExportEligibilityDiagnostic"' not in qml
    binder = read("app/src/internal/m18_product_complete_binding.hpp")
    assert "can_offer_verified_master(" in binder
    assert "fanout.m17_verified_epoch_current()" in binder
    assert "verified==c.candidates.staged()" in binder
    assert "export_readiness_reason(" in binder


def test_original_source_open_guard_preserved() -> None:
    qml = read("ui/qml/Main.qml")
    # The unqualified Ctrl+O Shortcut would mutate an already qualified
    # Main.qml. Retain its existing guarded Open Source button instead.
    assert 'id: sourceOpenButton' in qml
    assert 'enabled: !root.monitorExclusive' in qml
    assert "onClicked: sourceDialog.open()" in qml
    assert 'objectName: "openSourceStandardShortcut"' not in qml


if __name__ == "__main__":
    for test in (
        test_visual_order_and_no_chain_mutation,
        test_product_binds_status_without_demo,
        test_export_permission_unchanged,
        test_original_source_open_guard_preserved,
    ):
        test()
    print("M19_M21_PRODUCT_NAVIGATION_SOURCE_GUARD_NONRELEASE_PASS")
