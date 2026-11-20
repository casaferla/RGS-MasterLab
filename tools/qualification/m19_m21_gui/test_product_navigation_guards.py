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
                 "QStringLiteral(\"Read-only analysis\")"):
        assert term in part, term


def test_product_binds_status_without_demo() -> None:
    main = read("app/src/main.cpp")
    assert "bindAnalysisStatusViewModel(\n        &m21ProductStatusViewModel)" in main
    assert "m21ProductStatusViewModel" in read("ui/qml/Main.qml")
    editor = read("ui/qml/AnalysisStatusEditor.qml")
    host = read("ui/qml/DspEditorHost.qml")
    assert "productionAuthorized === true" in editor
    assert "selectPanelSource(" in host
    assert "fixtureMode" not in editor
    assert "AnalysisApprovedContent {" in editor
    assert 'objectName: "analysisApprovedContent"' in editor
    visuals = read("ui/qml/AnalysisApprovedContent.qml")
    # Preserve the original explicit unavailable visual gate, now owned
    # by the approved Product read-only content rather than the wrapper.
    assert "NO MEASUREMENTS" in editor
    assert "VERIFIED ANALYSIS UNAVAILABLE" not in editor
    assert "ANALYSIS · DEVELOPMENT" not in editor
    assert 'objectName: "analysisVerificationStatus"' in editor
    assert 'text: approvedContent.panelHasData' in editor
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
    assert 'ToolTip.visible: headerAnalysisDwell.ready' in host
    assert host.count('StudioTooltipDwell {') >= 1
    dwell = read("ui/qml/StudioTooltipDwell.qml")
    assert 'interval: 1000' in dwell
    assert 'onPointChanged: guard.resetDwell()' in dwell
    assert 'onHoveredChanged: guard.resetDwell()' in dwell
    assert 'dataAvailable: true' not in visuals
    assert 'fixtureMode' not in visuals
    assert "m19MetricValue_" in visuals and "heroReadout: true" in visuals
    assert "m20WorkbenchSpectrum" in visuals
    assert "Flickable {" not in visuals
    assert "ScrollBar.vertical" not in visuals
    assert "analysisProductContentStack" in visuals
    assert "m21FullNineMetricRows" in visuals
    assert "compactFit" not in visuals
    assert "columns: 3" in visuals
    assert 'objectName: "m19MetricsGrid"' in visuals
    assert 'objectName: "m19CenteredViewport"' in visuals
    assert 'y: Math.max(0, (m19Viewport.height - m19.height) / 2)' in visuals
    assert 'QStringLiteral("m19CenteredViewport")' in read(
        "tests/ui_smoke/test_source_metadata_panel.cpp")
    # Signed, display-only graphical comparisons use only finite genuine
    # Product deltas, not fabricated matches or normalized audio scores.
    assert 'function deltaDisplayLimit(key)' in visuals
    for key,limit in (("lufs","6.0"),("rms","6.0"),
                      ("samplePeak","6.0"),("truePeak","6.0"),
                      ("crest","6.0"),("width","1.0")):
        assert f'case "{key}": return {limit}' in visuals
    assert 'case "P10": case "P50": case "P90": return 2.0' in visuals
    assert 'objectName: "m21DeltaBar_"' in visuals
    assert 'visible: m21ValueCell.hasDelta' in visuals
    assert 'root.hasNumber(deltaValue)' in visuals
    assert 'Number.isFinite(displayLimit)' in visuals
    assert 'onDeltaValueChanged: requestPaint()' in visuals
    assert 'Math.abs(deltaValue)/displayLimit' in visuals
    assert 'deltaValue<0' in visuals
    assert 'NOT audio quality' in visuals
    assert 'QStringLiteral("m21DeltaBar_%1_%2")' in read(
        "tests/ui_smoke/test_source_metadata_panel.cpp")
    # PO 2026-11-19: M17 renders the MASTER. PREPARED is a distinct
    # audition identity. Preserve native Prepare API/state compatibility.
    tp_rail = read("ui/qml/TruePeakMeterEditor.qml")
    selector = read("ui/qml/DspChainSelector.qml")
    assert ': root.limiterPreparationVisible ? "RENDERING" : "GR HISTORY"' in tp_rail
    assert '"Offline Master rendering progress"' in tp_rail
    assert '"CONVERTING PLAYBACK"' in tp_rail
    assert 'QStringLiteral("RENDERING")' in read(
        "tests/ui_smoke/test_source_metadata_panel.cpp")
    assert 'function m17RenderCopy(value)' in host
    assert 'root.m17RenderCopy(root.terminalLimiterViewModel.audioStatus)' in host
    assert 'return "Needs render"' in selector
    assert 'return "Rendering Master"' in selector
    assert '"Select the output sample rate before Render Master;' in host
    assert "m17PrepareButton" in host and "requestPrepare()" in host
    assert "PREPARED" in host

    assert 'm19SpareHeight' in visuals
    assert 'm19RelationHeight' in visuals
    assert 'm21RowHeight' in visuals
    assert 'objectName: "m21CenteredViewport"' in visuals
    assert 'y: Math.max(0, (m21Viewport.height - m21.height) / 2)' in visuals
    assert 'height: root.m21RowHeight' in visuals
    assert 'QStringLiteral("Δ = REFERENCE − CURRENT")' in main
    assert 'height: Math.max(140, root.height - 166)' in visuals
    assert 'objectName: "analysisDetailFooter"' not in visuals
    assert 'THREE INDEPENDENT MEASUREMENTS' not in editor
    assert 'anchors.margins: 8' in editor
    assert 'for(const QSize size : {QSize{1184,688},QSize{1350,750}' in read(
        "tests/ui_smoke/test_source_metadata_panel.cpp")
    qt_smoke = read("tests/ui_smoke/test_source_metadata_panel.cpp")
    assert "analysisStack->implicitHeight()" in qt_smoke
    assert "<=analysisBody->height()+1.0" in qt_smoke
    assert "fitDiagnostic" in qt_smoke
    assert 'QStringLiteral("analysisApprovedContent")' in qt_smoke
    assert "Last essential readout clipped at minimum 1184x688" in qt_smoke
    assert 'model: ["PREPARED", "PROCESSED", "REFERENCE"]' in host
    assert 'objectName: "analysisHeaderSourceChooser"' in host
    assert 'Reset Export TP Limiter Ceiling to 0.0 dBTP' in host
    assert 'Restore Export Ceiling to 0.0 dBTP' in host
    assert 'root.isM19AnalysisEditor || root.isM20AnalysisEditor' in host
    assert 'selectPanelSource(' in host
    assert 'analysisSourceOption_' not in editor
    assert 'root.panelIndex === 2' in editor
    assert 'model: ["METRIC","PREPARED","PROCESSED","REFERENCE",' in visuals
    assert '"Δ PREPARED → REFERENCE"' in visuals
    assert '"Δ PROCESSED → REFERENCE"' in visuals
    assert 'readonly property real m21ColumnGap: 8' in visuals
    assert 'function m21ColumnWidth(column, tableWidth)' in visuals
    assert 'width: root.m21ColumnWidth(index,parent.width)' in visuals
    assert 'objectName: "m21Header_" + index' in visuals
    assert 'objectName: "m21ValueCell_"' in visuals
    assert 'readonly property real deltaGroupLeft:' in visuals
    assert 'm21ValueCell.numericWidth' in visuals
    assert 'm21ValueCell.deltaGroupGap' in visuals
    assert '"M21 Δ header must center between numeric value and bar"' in read(
        "tests/ui_smoke/test_source_metadata_panel.cpp")
    assert 'objectName: "m20FrequencyTick_" + index' in visuals
    assert 'parent.width*Math.log10(modelData.hz/20)' in visuals
    assert 'root.statusViewModel.runningAnalysisRole' in editor
    assert "m20MissingSpectrumNotice" in visuals
    assert "m20MissingProfileNotice" in visuals
    # Measured M20 1/12 octave band energy is an already-produced
    # Product value, not a fabricated normalized or "qualified" shape.
    assert 'objectName: "m20MeasuredBandEnergy"' in visuals
    assert 'observedReadouts.m20BandEnergy' in visuals
    assert 'p.linearEnergy<=0' in visuals
    assert 'ctx.lineTo(x,height*(1-normalized))' in visuals
    assert 'NO MEASURED BAND ENERGY' in visuals
    assert 'TONAL PROFILE UNAVAILABLE · SUPPORTED BANDS ONLY' not in visuals
    authentic_port=read("app/src/internal/m21_real_product_metrics_port.hpp")
    assert 's.primary_bands_1_12' in authentic_port
    assert 'out.measured_band_energy_1_12.push_back' in authentic_port
    assert 'out.profile_denominator=0; // linear fractions != shape dB' in authentic_port
    snapshots=read("app/src/internal/m21_metrics_comparison_candidate.hpp")
    assert 'std::vector<MeasuredTonalBandEnergy> measured_band_energy_1_12{};' in snapshots
    assert 'struct ReadinessSnapshot final' in snapshots
    assert 'measured_band_energy_1_12' not in snapshots[snapshots.index('struct ReadinessSnapshot final'):]
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
    assert "metric==3U" not in status_cpp
    assert "cell.quality!=m21_private::Quality::present" in status_cpp
    assert "m21_private::Quality::low_confidence" in status_cpp
    assert "formal Gate G not asserted" in read(
        "app/src/internal/m21_real_product_metrics_port.hpp")
    assert "evidence.prepared_to_gold[i]" in status_cpp
    assert "evidence.processed_to_gold[i]" in status_cpp
    assert "m20StaticPsd" in status_cpp
    assert "m20BandEnergy" in status_cpp
    assert "s.measured_band_energy_1_12" in status_cpp
    assert "s.measured_band_energy_1_12.size()<=256U" in status_cpp
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
    assert '"ANALYSIS"' in status_qml
    assert 'ANALYSIS · DEVELOPMENT' not in status_qml

    # The progress label is driven solely by the genuine currently
    # running single-flight worker; no playback or synthetic percentage.
    assert "Q_PROPERTY(QString runningAnalysisRole" in status_header
    assert "set_running_analysis_role(" in status_cpp
    assert 'status_->set_running_analysis_role(QStringLiteral("REFERENCE"))' in driver
    assert 'QStringLiteral("PREPARED"):QStringLiteral("PROCESSED")' in driver
    assert "status_->set_running_analysis_role({});" in driver
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
    host = read("ui/qml/DspEditorHost.qml")
    # The fixed 120px M18 receipt now uses complete short labels instead
    # of truncating a long BLOCKED diagnostic. The real export permission
    # and native reason must remain unchanged and present in tooltip.
    assert 'return "EXPORT BLOCKED"' in host
    assert 'return "M17 NOT CURRENT"' in host
    assert 'reason.indexOf("M17 VERIFIED EPOCH")' in host
    assert 'm.exportReadinessReason' in host
    assert 'root.masterExportModel.exportReadinessReason' in host
    assert 'enabled: root.canExportVerifiedWav' in host
    assert '&& !!root.masterExportHandler' in host
    assert 'root.masterExportHandler(0)' in host
    assert 'root.masterExportHandler(1)' in host
    # The auto-return is a convenience only. Product canExport is checked
    # immediately AFTER exact device retirement, BEFORE SaveFile can open.
    assert 'root.m18AutoReturnForExportAvailable' in qml
    # #74 removed an old impossible guard where recovery was inferred
    # from monitorExclusive. F05 instead uses the NARROW, current
    # recovery-required predicate, while healthy verified monitoring
    # remains eligible for an exact native Return + Export. Do not turn
    # this back into a blanket monitor exclusion.
    auto_return = qml[qml.index('readonly property bool m18AutoReturnForExportAvailable:'):
                      qml.index('property int m18PendingExportFormatIndex:')]
    assert '&& !root.m17RecoveryExitRequired' in auto_return
    assert 'String(root.appTerminalMonitorViewModel.statusText)' in qml
    assert 'indexOf("RECOVERY REQUIRED") !== -1' in qml
    for token in (
        'root.appTerminalMonitorViewModel.monitorInstalled',
        '!root.appTerminalMonitorViewModel.monitorPreparing',
        '!root.appTerminalMonitorViewModel.monitorCancelling',
        '=== "MONITOR · PRIVATE PCM (NOT LIVE)"',
    ):
        assert token in auto_return
    flow = qml[qml.index('function requestVerifiedWavExport('):
               qml.index('readonly property var appMemoryAdvisor:')]
    assert flow.index('root.returnFromM17Audition()') < flow.index(
        'root.appM18ExportModel.refresh()') < flow.rindex(
        'root.appM18ExportModel.canExport') < flow.index(
        'm18ExportFileDialog.open()')
    assert 'masterExportAutoReturnAvailable' in host
    workspace = read("ui/qml/DspWorkspace.qml")
    assert 'property bool masterExportAutoReturnAvailable: false' in workspace
    assert workspace.count(
        'masterExportAutoReturnAvailable: root.masterExportAutoReturnAvailable') == 2
    assert 'masterExportAutoReturnAvailable: root.m18AutoReturnForExportAvailable' in qml
    assert 'return "RETURN + EXPORT"' in host
    assert 'if (root.masterExportHandler(0))' in host
    assert 'if (root.masterExportHandler(1))' in host


def test_original_source_open_guard_preserved() -> None:
    qml = read("ui/qml/Main.qml")
    # PO physical #68: the old caption-only Ctrl+O did nothing. Retain
    # the M17 monitor admission and SOURCE picker transaction; require an
    # actual application Shortcut, plus dynamic Open/Replace menu text.
    assert 'id: sourceOpenButton' in qml
    assert 'enabled: !root.monitorExclusive' in qml
    assert "onClicked: sourceDialog.open()" in qml
    assert 'objectName: "sourceLoadShortcut"' in qml
    assert 'sequence: "Ctrl+O"' in qml
    assert 'context: Qt.ApplicationShortcut' in qml
    assert 'onActivated: sourceDialog.open()' in qml
    assert 'text: (sourceSelection.hasSource ? "Replace Source" : "Open Source")' in qml
    assert 'title: sourceSelection.hasSource ? "Replace Source WAV" : "Open Source WAV"' in qml


def test_m17_export_ui_presentation_order() -> None:
    # Explicit PO request after FQ #101: only labels and button ordering;
    # no DSP, rate authority, export encoder or Qt object identity changes.
    host = read("ui/qml/DspEditorHost.qml")
    adapter = read("app/src/dsp_module_adapter.hpp")
    assert 'return QStringLiteral("Export TP Limiter");' in adapter
    section = host[host.index('objectName: "m17ExplicitPreparationControls"'):
                   host.index('// The M17 preparation progress belongs')]
    export = section.index('objectName: "m17RateAndTimesButton"')
    prepare = section.index('objectName: "m17PrepareButton"')
    cancel = section.index('objectName: "m17CancelPreparationButton"')
    assert export < prepare < cancel
    assert 'text: "WAV Export"' in section
    assert 'objectName: "m17WavExportPanelTitle"' in host
    assert 'text: "WAV EXPORT · RATE & TIME"' in host
    assert 'objectName: "m17ExportPcm24"' in host
    assert 'objectName: "m17ExportFloat32"' in host
    assert 'root.masterRateState.currentMasterRateAdmissible' in section
    assert 'root.terminalLimiterViewModel.requestPrepare()' in section



def test_physical_po_m16_reference_cache_and_p2_numeric() -> None:
    # The actual True Peak is the ORIGINAL M16 4x FIR on all three sources,
    # including bounded external references >128 MiB. No release authority.
    stream = read("analysis/src/streamed_reference_analyzer.cpp")
    assert 'internal::TruePeakItu4xChannelStream' in stream
    assert "m16[ch].finalize()" in stream
    assert "M16_SUPPORTED_UNQUALIFIED" in stream
    port = read("app/src/internal/m21_real_product_metrics_port.hpp")
    assert 'return {metric.db,Quality::low_confidence,' in port
    view = read("app/src/m21_product_status_view_model.cpp")
    assert "metric==3U" not in view
    assert "m21_private::Quality::low_confidence" in view
    assert "phase6_release_readouts()" in view
    basic = read("analysis/src/true_peak_measurement.cpp")
    assert "qualification_status_ is deliberately left SUPPORTED_UNQUALIFIED" in basic
    tests = read("tests/unit/analysis/test_streamed_reference_analyzer.cpp")
    assert "measure_basic_level_full_resource(*owner.value())" in tests
    assert "BasicLevelValueState::M16_SUPPORTED_UNQUALIFIED" in tests

    # A global M18 epoch revocation cannot force all three independently
    # authenticated PCM scopes to be reanalyzed after a harmless action.
    inbox = read("app/src/internal/m21_product_publication_inbox.hpp")
    coordinator = read("app/src/internal/m21_private_product_analysis_coordinator.hpp")
    assert 'rebind_unchanged_independent_scopes(' in inbox
    assert 'inbox_->rebind_unchanged_independent_scopes(' in coordinator
    assert 'rebind_after_reference_only_change(' not in inbox
    assert 'rebind_unchanged_prepared_reference(' not in inbox
    assert coordinator.index('inbox_->rebind_unchanged_independent_scopes(') < coordinator.index(
        'if(worker_.busy())return;')
    independently = inbox[inbox.index('rebind_unchanged_independent_scopes('):
                          inbox.index('// GOLD is only accepted')]
    assert 'for(const auto column:{Column::prepared,Column::processed})' in independently
    assert 'Product::same_render_observation(' in independently
    assert 'Product::same_gold_observation(before,now)' in independently
    owner = read("app/src/internal/m21_product_owner_analysis.hpp")
    assert 'before.prepared!=now.prepared' in owner
    assert 'before.processed==now.processed' in owner
    assert 'before.processed_realization_id' in owner
    assert 'before.owner.identity.chain_revision' in owner
    # A parameter-only edit preserves ProcessingChain::revision()!
    # The binding payloads must be compared, not just the structural rev.
    assert 'before.manual_bindings==now.manual_bindings' in owner
    assert 'product.upstream.chain.execution_bindings()' in owner
    preview=read("app/src/mastering_preview_controller.cpp")
    assert 'acceptedPreviewBindings_==chainState_->execution_bindings()' in preview
    assert 'acceptedPreviewGeneration_==previewGeneration_' in preview
    assert 'pendingPreviewBindings_=job.bindings' in preview
    assert 'acceptedPreviewBindings_=pendingPreviewBindings_' in preview
    assert 'acceptedProcessedProvider_()==published' in preview
    assert 'acceptedPreviewOwner_=acceptedProcessedProvider_' in preview
    assert 'set_accepted_processed_provider(' in read("app/src/main.cpp")
    unit=read("tests/unit/app/test_mastering_preview_controller.cpp")
    assert 'controller.published_preview_matches_current_bindings()' in unit
    assert '!controller.published_preview_matches_current_bindings()' in unit
    assert 'publishedResult=make_unit_prepared_result();' in unit
    assert 'processedPreviewMatches_()' in coordinator
    assert '||!processedPreviewMatches_' in coordinator
    workflow=read(".github/workflows/m23-failure-matrix-focused.yml")
    assert workflow.count('name: Windows - M23 true file faults') == 1
    assert workflow.count('name: RGS-M23-Bounded-Windows') == 1
    assert workflow.count('name: RGS-M23-Bounded-Linux') == 1
    assert workflow.count('M21 accepted PROCESSED owner and binding test (Linux)') == 1
    assert workflow.count('M21 accepted PROCESSED owner and binding test (Windows)') == 1
    assert workflow.count(" -R '^rgsml_mastering_preview_controller_tests$'") == 2
    assert 'published_preview_matches_current_bindings()' in read(
        "app/src/main.cpp")
    integration=read("tests/integration/app/test_project_session.cpp")
    assert 'rebind_after_reference_only_change(' not in integration
    assert 'rebind_unchanged_prepared_reference(' not in integration
    assert 'referenceCache.rebind_unchanged_independent_scopes(' in integration
    assert 'siblingChanged.processed.reset();' in integration
    assert 'same_render_observation(' in integration
    assert 'paramsChanged.manual_bindings.clear();' in integration
    assert 'same_gold_observation(' in integration
    assert 'newSelection.selection_revision' in integration
    assert 'structuralRevision' in integration
    assert 'session.masteringChainState->set_gain_parameters(' in integration
    assert 'selectGold(QUrl::fromLocalFile(refB))' in integration
    assert 'inflightCache.accept_completed_unchanged_render(' in integration
    assert 'M21ProductStatusViewModel actualNumericUi' in integration
    # Real Windows Product test must support both initial PCM prepare and
    # the genuine same-target PROCESSED handoff. A prepare-only fake
    # produces a false red when new immutable manual PCM is published.
    assert 'session.transport.set_pcm_handoff_handler(' in integration
    assert 'M18_TEST_PROCESSED_HANDOFF_REQUIRES_CURRENT_MANUAL_SINK' in integration
    assert 'currentDevice.value()->audibleRealization.realizationId' in integration
    assert 'samplePeakAfterRef.value("processed").isValid()' in integration
    assert '!samplePeakAfterRef.value("reference").isValid()' in integration
    assert 'samplePeakAfterDsp.value("reference").isValid()' in integration
    assert '!samplePeakAfterDsp.value("processed").isValid()' in integration
    assert 'samplePeakAfterDsp.value("deltaPrepared").isValid()' in integration
    assert 'inflightCache.accept_completed_unchanged_gold(' in integration
    assert 'wrongGoldWorker.resource_identity=' in integration
    assert 'completed.resource_identity!=gold_result_identity(oldCapture)' in inbox
    assert 'gold_result_identity(*now.value())' in inbox
    assert 'reused.resource_identity=gold_result_identity(now)' in inbox
    assert 'result.resource_identity!=gold_result_identity(captured)' in inbox
    assert 'referenceCache.rebind_unchanged_independent_scopes(' in integration
    assert 'before.owner.identity.source_generation' in owner
    assert 'Project save/metadata revision alone is not a PCM revision.' in owner
    assert 'before.owner.identity.project_generation' not in owner[owner.index(
        'same_render_observation('):owner.index('same_gold_observation(')]
    assert 'before.selection_revision==now.selection_revision' in owner
    assert 'gold.selection_revision()!=expected.selection_revision' in owner
    assert 'goldCapture_->selection_revision==gold.selection_revision()' in independently
    assert 'goldSelectionRevision_' not in inbox
    assert 'beforeCapture->owner.epoch>=now.owner.epoch' in independently
    # No duplicate whole-WAV scan when an in-flight completed result can
    # be reauthenticated with the exact current immutable PCM.
    assert 'accept_completed_unchanged_render(' in inbox
    assert 'accept_completed_unchanged_gold(' in inbox
    assert 'accept_completed_unchanged_render(' in coordinator
    assert 'accept_completed_unchanged_gold(' in coordinator
    assert 'renderAttempts_[idx].remember(' in coordinator
    assert 'attempted_[idx]=gold_key(' in coordinator
    main = read("app/src/main.cpp")
    assert 'refreshM21AfterReconcile' in main
    assert main.count('refreshM21AfterReconcile,') == 7
    host = read("ui/qml/DspEditorHost.qml")
    assert 'text: "Render Master"' in host
    assert 'text: "Preview Master"' in host
    assert 'root.terminalLimiterViewModel.requestPrepare()' in host
    assert 'accept_render(product,fanout,selector,now,reused)' in independently
    assert 'accept_gold(product,fanout,selector,gold,now,reused)' in independently
    assert 'M21_PROCESSED_RENDER_CHAIN_REVISION_NOT_YET_CURRENT' in owner
    assert 'evidence.processed->chain_revision()' in coordinator
    assert 'result.column==Column::processed' in inbox
    assert 'render->chain_revision()' in inbox
    # The status-only and numeric readout paths MUST retire each scope
    # without consulting unrelated sibling PCM pointers. This is a real
    # same-epoch bug even when all later epoch-rebind cases are correct.
    retirement=inbox[inbox.index('template<class T>'):
                     inbox.index('m21_private::PrivatePublicationGuard publisher_')]
    assert 'Product::same_render_observation(' in retirement
    assert 'Product::same_gold_observation(' in retirement
    assert 'Product::still_current(' not in retirement
    assert 'Product::gold_still_current(' not in retirement

    editor=read("ui/qml/StreamingTruePeakLimiterEditor.qml")
    icon=read("ui/qml/StudioIcon.qml")
    for marker in ('streamingP2CeilingNumericWell','streamingP2DriveNumericWell',
                   'streamingP2CeilingNumericInput','streamingP2DriveNumericInput',
                   'streamingP2ActionBar','streamingP2ResetButton'):
        assert marker in editor
    assert editor.count('width: 120; height: 32; radius: 4') == 2
    assert editor.count('font.pixelSize: 16') == 2
    assert 'Research margin' not in editor
    assert 'streamingP2Margin2Button' not in editor
    assert 'streamingP2Margin5Button' not in editor
    assert 'EXPERIMENTAL:' not in editor
    assert editor.index('objectName: "streamingP2ResetButton"') < editor.index(
        'objectName: "streamingP2MainPanel"')
    assert 'P2_I2_NUMERIC_WELLS=%s' in read("app/src/main.cpp")
    single=icon[icon.index('if (kind === "module-gain")'):
                icon.index('} else if (kind === "module-eq")')]
    assert single.count('line(') == 2
    assert 'line(2.4, 9.0, 15.6, 9.0)' in single
    assert 'ctx.shadowBlur = icon.glowBlur' in icon

def test_reference_waveform_and_master_parking_physical_po() -> None:
    # Separate waveforms follow the selected audible target. The SOURCE
    # viewport remains independent, with NO REFERENCE click mutating its
    # region or forcing any M19/M21 source reanalysis.
    main=read("app/src/main.cpp")
    qml=read("ui/qml/Main.qml")
    wave=read("app/src/source_waveform_view_model.cpp")
    native=read("tests/integration/app/test_source_waveform_view_model.cpp")
    selector=read("app/src/audition_source_selector.cpp")
    ui=read("app/src/terminal_product_activation_coordinator.cpp")
    session=read("app/src/terminal_private_product_session.cpp")
    fanout=read("platform/windows/src/internal/m18_owner_mutation_fanout.hpp")
    assert 'referenceWaveformPresentation' in main
    assert 'referenceWaveform.source_committed(resource->reference())' in main
    assert 'lastRevision=std::uint64_t{0}' in main
    assert 'referenceWaveform.clear()' in main
    assert 'referenceWaveformPresentation.set_seek_handler(' in main
    assert 'auditionSelector.gold_active()' in main
    assert 'playbackTransport.seek_target_frame(position)' in main
    assert 'QStringLiteral("referenceWaveform")' in main
    assert 'root.referenceWaveformActive ? referenceWaveform : sourceWaveform' in qml
    assert 'text: root.referenceWaveformActive ? "REFERENCE WAVEFORM"' in qml
    assert 'waveformColor: root.referenceWaveformActive ? root.warning : root.accent' in qml
    assert 'presentation: root.displayedWaveform' in qml
    assert 'root.displayedWaveform.sourceFrameCount' in qml
    assert 'onClicked: root.displayedWaveform.fitSource()' in qml
    assert 'SourceWaveformViewModel::clear()' in wave
    assert 'activeStop_.request_stop();' in wave
    assert 'clearRetiresReferenceWorkerWithoutTouchingSource' in native
    assert 'beforeIndependentAudition_(target)' in selector
    assert 'before_independent_audition(' in ui
    # Audit #74: an M18-denied target switch may NOT first retire the
    # live M17 sink. The admission precedes the M17 park, but the
    # selector publication scope still starts AFTER nested MANUAL restore.
    pos_selector=selector.index('core::Status AuditionSourceSelector::switch_to(')
    switch_body=selector[pos_selector:selector.index(
        'core::Status AuditionSourceSelector::switch_to_after_authorized_mutation(',pos_selector)]
    assert switch_body.index('independentAuditionPreflight_(target)') < switch_body.index(
        'admit_m18_source_mutation("switch_target")') < switch_body.index(
        'beforeIndependentAudition_(target)') < switch_body.index(
        'M18ScopedAuditionMutation lifetime')
    assert 'set_independent_audition_preflight(' in main
    assert 'can_begin_independent_audition(target)' in main
    assert 'M17_AUDIT_RENDER_IN_PROGRESS_NO_MUTATION' in read(
        'tests/unit/app/test_terminal_product_activation.cpp')
    # PO 2026-11-19 scope reset: the large additional M17 Qt test
    # remains available in CMake, but it is NOT the Product artifact gate.
    # The focused lane must prove source/Reference/transport tests and
    # explicitly label the exploratory native M17 test NOT RUN (no
    # fraudulent native-qualification PASS while its oracle is stale).
    focused = read(".github/workflows/r1d-p2-i2-product-gui-smoke.yml")
    for target in ('rgsml_audition_source_selector_tests',
                   'rgsml_audition_region_tests',
                   'rgsml_playback_support_tests'):
        assert target in focused
    assert 'M17_LEGACY_COMBINED_NATIVE=NOT_RUN_SCOPE_RESET_PENDING_PO_PHYSICAL' in focused
    assert 'M17_M18_AUDITION_OWNER_AND_CROSS_RATE_NATIVE=PASS' not in focused
    assert 'P2_AB_M17_OWNER_TEST_FAIL' not in focused
    # The genuine 16 frozen FIR admission remains part of the OPTIONAL
    # native suite and the other strict M17/M18 checks; never unpin assets.
    activation_cmake=read("cmake/RgsmlTerminalProductActivationTests.cmake")
    assert 'add_dependencies(rgsml_terminal_product_activation_tests' in activation_cmake
    assert '    rgsml_external_fir_assets)' in activation_cmake
    assert 'rgsml_external_fir_assets' in activation_cmake
    # F05: MONITOR_PRIVATE_RECOVERY_REQUIRED can still be 'installed'
    # natively but is not an ordinary export-return state. Keep the
    # convenience action disabled while explicit DEVICE recovery is needed.
    export_ui=qml[qml.index('readonly property bool m18AutoReturnForExportAvailable:'):
                  qml.index('// QML never mints an export Ticket.')]
    assert '&& !root.m17RecoveryExitRequired' in export_ui
    assert export_ui.index('root.monitorExclusive') < export_ui.index(
        '&& !root.m17RecoveryExitRequired') < export_ui.index(
        'root.appTerminalMonitorViewModel.monitorInstalled')
    monitor=read('app/src/terminal_monitor_view_model.cpp')
    assert 'MONITOR_PRIVATE_RECOVERY_REQUIRED' in monitor
    assert 'AUDITION RECOVERY REQUIRED · OWNER CHECK' in monitor
    assert 'M18_AUDIT_BUSY_WITH_EXACT_OUTPUT_OWNER' in read(
        'tests/unit/app/test_terminal_product_activation.cpp')
    assert 'M17_INDEPENDENT_AUDITION_REQUIRES_OWNER_THREAD' in ui
    assert 'deactivate(true);' in ui
    assert 'comparison_pcm_still_valid()' in ui
    assert 'resume_comparison()' in ui
    assert 'acceptedPreviewOwner_' not in ui  # independent MANUAL
    assert 'comparison_pcm_still_valid() const' in session
    assert 'op=="switch_target" ||op=="set_gold"' in fanout
    assert 'state->verifiedM17Epoch==state->epoch' in fanout
    product=read("tests/integration/app/test_project_session.cpp")
    assert 'M17_VERIFIED' not in product[product.index(
        'm18VerifiedEpochSurvivesOnlyAlreadyVerifiedManualReturn()'):
        product.index('m18RealProductVerifiedMasterToPcm24AndFloat32WavAck()')]
    assert 'QVERIFY(admit("switch_target"));' in product
    assert 'QVERIFY(admit("set_gold"));' in product
    physical=read("tests/unit/app/test_terminal_product_activation.cpp")
    assert 'selector.set_before_independent_audition(' in physical
    assert 'product.comparison_pcm_still_valid()' in physical
    assert 'renderer.render_invocation_count(),savedRenderCount' in physical
    assert 'QVERIFY(selector.switch_to(app::AuditionTarget::GOLD));' in physical
    assert 'QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));' in physical
    assert 'QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));' in physical
    assert 'QVERIFY(!limiter.value()->bypassed());' in physical
    assert 'QVERIFY(!product.comparison_owners_current());' in physical
    assert 'QTRY_COMPARE_WITH_TIMEOUT(product.status(),' in physical
    # PO #72: returning a verified cross-rate Master (88.2k on a 48k
    # endpoint) from REFERENCE/PREPARED must not attempt direct Preview
    # rearm. Retain real checked Master for Listen to Master with its
    # separate SRC and exact monitor ownership admission.
    assert 'session_->status()==QStringLiteral("COMPARISON_VERIFIED")' in ui
    assert '!session_->current_master_matches_device_for_preview()' in ui
    assert '"MASTER VERIFIED · LISTEN TO MASTER"' in ui
    assert 'view_->clearAudioError();' in ui
    assert 'QVERIFY(handoffUi.cancel());' in physical
    assert '"M17_PO_CROSS_RATE_MONITOR_WRONG_PRE_FLIGHT_SOURCE"' in physical
    assert 'QVERIFY(handoffUi.can_install()); // still usable without Render Master' in physical
    assert 'QCOMPARE(device->state,core::PlaybackState::PLAYING);' in physical
    assert 'session_->verified_master_rate_for_monitoring()' in ui
    assert 'QCOMPARE(renderer.render_invocation_count(),crossRateRenderCount);' in physical
    # F06: ON after BYPASS is not permission to discard a cross-rate
    # verified Master by attempting same-rate direct Preview rearm.
    f07_on_handler=ui[ui.index(
        'void TerminalProductActivationCoordinator::limiter_accepted()'):
        ui.index('void TerminalProductActivationCoordinator::manual_preview_changed(')]
    assert 'maybe_rearm_parked_master();' in f07_on_handler
    # F07: successful direct rearm consumes comparison_cached(), but must
    # return from ON before the invalidation/cancel branch is considered.
    assert 'if(rearmed==QStringLiteral("READY_TO_INSTALL")' in f07_on_handler
    assert '||rearmed==QStringLiteral("MEASURED_NONCONFORMING_READY"))' in f07_on_handler
    assert f07_on_handler.index('if(rearmed==QStringLiteral("READY_TO_INSTALL")') < (
        f07_on_handler.index('static_cast<void>(session_->cancel());',
                             f07_on_handler.index('maybe_rearm_parked_master();')))
    assert 'QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));' in physical
    assert 'QCOMPARE(candidates.staged(),initialPreparedPcm);' in physical
    assert 'const auto bypassCrossRateMaster=candidates.staged();' in physical
    assert 'QCOMPARE(renderer.render_invocation_count(),bypassRenderCount);' in physical
    # CI #82: DEVICE rate change is not a project MASTER rate change.
    # Require a real PCM-preserving cross-rate monitor path and a
    # restored same-rate direct Preview without a new render.
    changed_device=physical[physical.index(
        '// F06: the selected MASTER sample rate is independent of the'):
        physical.index('// The Product default is 0 dBTP (distinct from the frozen generic')]
    assert 'QStringLiteral("MASTER VERIFIED · LISTEN TO MASTER")' in changed_device
    assert 'QStringLiteral("COMPARISON_VERIFIED")' in changed_device
    assert '!product.current_master_matches_device_for_preview()' in changed_device
    assert 'QCOMPARE(candidates.staged(),initialPreparedPcm);' in changed_device
    # CI #83: external_context_changed() is deliberately private.
    # Exercise the REAL public PlaybackTransportViewModel::stop() signal,
    # connected to maybe_rearm_parked_master() in the Product coordinator.
    # A synthetic test must never bypass Product public transport wiring.
    assert 'coordinator.external_context_changed();' not in physical
    assert 'transport.stop();' in changed_device
    assert 'QCOMPARE(device->state,core::PlaybackState::PLAYING);' in changed_device
    assert 'QCOMPARE(device->state,core::PlaybackState::STOPPED);' in changed_device
    assert 'PlaybackTransportViewModel::playbackChanged' in ui
    assert 'QStringLiteral("READY_TO_INSTALL")' in changed_device
    assert 'renderCountBeforeDeviceRateSwitch' in changed_device
    # PREPARED/REFERENCE transport-only audition must park, not retire,
    # the exact verified Master and demand a second full rendering.
    retained_target=physical[physical.index(
        'M17_EDGE_CASE=TARGET_CHANGE_RETAINS_VERIFIED_PCM'):
        physical.index('// A genuine Ceiling edit must invalidate the previous checked PCM.')]
    assert 'product.comparison_pcm_still_valid()' in retained_target
    assert '!product.comparison_owners_current()' in retained_target
    assert 'QStringLiteral("READY_TO_INSTALL")' in retained_target
    assert 'QCOMPARE(candidates.staged(),savedBeforeTargetSwitch);' in retained_target
    assert 'QCOMPARE(renderer.render_invocation_count(),renderCountBeforeTargetSwitch);' in retained_target
    assert 'M17_EDGE_CASE=TARGET_CHANGE_DROPS_CACHED_PCM' not in physical
    # CI #84: Product TerminalLimiterState::create_default starts at 0 dBTP.
    # The no-op test may not hard-code generic DSP -1 dBTP as its value.
    # A-B-A MUST restore the ORIGINAL accepted numeric value, while its
    # monotonically increasing parameter revision still invalidates PCM.
    noop_ceiling=physical[physical.index(
        'M17_EDGE_CASE=NOOP_CEILING_RETAINS_AB'):
        physical.index('M17_EDGE_CASE=LOOP_CHANGED_DURING_CACHED_BYPASS')]
    assert 'const auto acceptedCeilingBeforeEdits=' in noop_ceiling
    assert 'QCOMPARE(acceptedCeilingBeforeEdits,0.0);' in noop_ceiling
    assert 'QStringLiteral("ceilingDbTp"),acceptedCeilingBeforeEdits' in noop_ceiling
    assert 'QCOMPARE(limiter.value()->parameter_revision(),revisionBeforeNoop);' in noop_ceiling
    assert 'QCOMPARE(renderer.render_invocation_count(),renderCountBeforeNoop);' in noop_ceiling
    assert 'QCOMPARE(candidates.staged(),currentAdmitted);' in noop_ceiling
    assert 'QStringLiteral("ceilingDbTp"),-1.0' not in noop_ceiling
    ceiling_roundtrip=physical[physical.index(
        '// A genuine Ceiling edit must invalidate the previous checked PCM.'):
        physical.index('// Cancel only an ACTIVE preparation;')]
    assert 'QStringLiteral("ceilingDbTp"),-2.0' in ceiling_roundtrip
    assert 'QStringLiteral("ceilingDbTp"),acceptedCeilingBeforeEdits' in ceiling_roundtrip
    assert 'revisionAfterChange+1' in ceiling_roundtrip
    assert 'QVERIFY(!candidates.staged());' in ceiling_roundtrip
    assert 'QStringLiteral("ceilingDbTp"),-1.0' not in ceiling_roundtrip
    # CI #85: the earlier physical DEVICE-rate rearm deliberately STOPPED
    # exact MANUAL. A later PAUSED-source test MUST explicitly start the
    # original MANUAL first; pause() on STOPPED cannot create PAUSED.
    paused_start=physical[physical.index(
        '// CI #85: the preceding DEVICE-rate restoration'):
        physical.index('M17_EDGE_CASE=STOPPED_AB_TRANSPORT')]
    assert 'QCOMPARE(device->state,core::PlaybackState::STOPPED);' in paused_start
    assert 'QVERIFY(transport.seek_source_frame(core::FrameIndex{2600}));' in paused_start
    assert paused_start.index('transport.playOrResume();') < paused_start.index('transport.pause();')
    assert 'QCOMPARE(device->state,core::PlaybackState::PLAYING);' in paused_start
    assert 'QCOMPARE(device->state,core::PlaybackState::PAUSED);' in paused_start
    assert 'QCOMPARE(device->position,core::FrameIndex{2600});' in paused_start
    assert 'QCOMPARE(renderer.render_invocation_count(),renderCountBeforePausedPreview);' in paused_start
    # CI #80: the native monitor fixture deliberately STOPs MANUAL before
    # installing. Preserve the genuine stopped-to-playing transport proof,
    # not an unconditional PLAYING assertion that contradicts the fixture.
    monitor_native=physical[physical.index('// MAIN TRANSPORT POSITIVE PATH:'):
        physical.index('// Product cannot silently borrow an already installed')]
    assert 'QCOMPARE(device->state,core::PlaybackState::STOPPED);' in monitor_native
    assert 'QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED")' in monitor_native
    assert 'QCOMPARE(handoffUi.transport_state(),QStringLiteral("STOPPED"));' in monitor_native
    assert monitor_native.index('QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED")') < (
        monitor_native.index('QVERIFY(handoffUi.play());')
    )
    assert 'QStringLiteral("MONITOR_PRIVATE_PAUSED")' in monitor_native
    assert 'QCOMPARE(device->state,core::PlaybackState::PLAYING);' in monitor_native
    assert 'QCOMPARE(candidates.staged(),positiveMasterOwner);' in monitor_native
    assert 'QCOMPARE(renderer.render_invocation_count(),positivePreflightRenderCount);' in monitor_native
    # CI #81 exposed an old fixed 1600-frame oracle AFTER independent
    # monitor Play/Pause/Play. Require exact MASTER->SOURCE rational cue
    # projection, SOURCE Loop clamp and follow-up M17 BYPASS cue retention.
    assert 'const auto monitorBeforeReturn=product.private_monitor_master_snapshot();' in monitor_native
    assert 'const auto projectedSourceFrame=core::FrameIndex{' in monitor_native
    assert 'const auto expectedSourceReturnCue=' in monitor_native
    assert 'QCOMPARE(device->position,expectedSourceReturnCue);' in monitor_native
    assert 'const auto exactManualCueAfterMonitor=device->position;' in monitor_native
    post_monitor_bypass=physical[physical.index('const auto exactManualCueAfterMonitor='):
        physical.index('// More than one A/B switch must keep the same immutable PCM')]
    assert 'QCOMPARE(device->position,exactManualCueAfterMonitor);' in post_monitor_bypass
    assert 'QCOMPARE(device->position,core::FrameIndex{1600});' not in post_monitor_bypass
    # PO accepted the independent REFERENCE loop. The old prohibition
    # was a temporary safety guard, not a frozen UX requirement. Require
    # both separate source/Gold region stores and exact owner-gated audio.
    assert 'root.referenceWaveformActive ? referenceRegion : auditionRegion' in qml
    assert 'root.activeAuditionRegion.controlsEnabled' in qml
    assert 'root.activeAuditionRegion.loopEnabled' in qml
    assert 'root.activeAuditionRegion.requestLoopEnabled(checked)' in qml
    assert 'root.displayedWaveform.fitRegion()' in qml
    assert 'referenceWaveformPresentation.set_region_commit_handler(' in main
    assert 'referenceRegion.source_committed(' in main
    assert 'referenceRegion.clear_independent_reference()' in main
    assert 'auditionSelector.set_reference_loop_provider(' in main
    transport=read("app/src/playback_transport_view_model.cpp")
    region=read("app/src/audition_region_view_model.cpp")
    assert 'REFERENCE_LOOP_REQUIRES_REFERENCE_DEVICE_OWNER' in transport
    assert 'REFERENCE_LOOP_REQUIRES_READY_FILE_AND_VALID_TIMEBASE' in transport
    assert 'referenceAuditionChecker_' in transport
    assert 'set_reference_audition_checker(' in main
    assert 'auditionSelector.reference_device_installing()' in main
    assert 'referenceDeviceInstalling_=true' in selector
    assert 'referenceDeviceInstalling_' in read(
        "app/src/audition_source_selector.hpp")
    assert 'referenceInstallScope{referenceDeviceInstalling_}' in selector
    assert 'sawScopedRearmBeforePublication' in read(
        "tests/unit/app/test_audition_source_selector.cpp")
    assert 'sourceDerivedActive_' in transport
    assert 'set_loop_reference_range(' in transport
    assert 'referenceMode_ ? playback_->set_loop_reference_range(' in region
    assert 'referenceMode_ ||static_cast<bool>(m18RegionMutationAdmission_)' in region
    assert 'const auto previousGold=gold_' in selector
    assert 'gold_=previousGold;' in selector
    assert 'referenceLoopReinstalledAfterIndependentTargetSwitch' in read(
        "tests/unit/app/test_audition_source_selector.cpp")
    assert 'referenceLoopIsolationFromSourceAndClear' in read(
        "tests/integration/app/test_audition_region_view_model.cpp")
    assert 'root.restorePreviewAfterComparison=true' in qml
    assert 'root.restorePreviewAfterComparison=false' in qml
    assert 'if (root.monitorExclusive) {' in qml
    assert 'auditionSelector.activeTarget!==target' in qml
    assert 'root.appTerminalProductActivation.resumeParkedPreview()' in qml
    assert 'resumeParkedPreview()' in read(
        "app/src/terminal_product_activation_coordinator.cpp")
    assert 'MASTER RETAINED · SELECT PROCESSED' in physical


if __name__ == "__main__":
    for test in (
        test_visual_order_and_no_chain_mutation,
        test_product_binds_status_without_demo,
        test_export_permission_unchanged,
        test_m17_export_ui_presentation_order,
        test_original_source_open_guard_preserved,
        test_physical_po_m16_reference_cache_and_p2_numeric,
        test_reference_waveform_and_master_parking_physical_po,
    ):
        test()
    print("M19_M21_PRODUCT_NAVIGATION_SOURCE_GUARD_NONRELEASE_PASS")
