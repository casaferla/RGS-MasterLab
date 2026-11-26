import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspEditorHost"

    property var adapterModel: null
    // Main installs one Product-backed native preset controller. A
    // standalone QML preview has no file mutation affordance.
    property var presetController: typeof dspPresetController !== "undefined"
        ? dspPresetController : null
    // Opt-in via Main only. Standalone DSP editor fixtures preserve their
    // historical local validation feedback for isolated smoke coverage.
    property bool errorsInGlobalStatusBar: false
    property int selectedModuleIndex: adapterModel ? adapterModel.selectedIndex : 0
    property var gainViewModel: null
    property var eqViewModel: null
    property var compressorViewModel: null
    property var spectrumViewModel: null
    property var analysisStatusViewModel: null
    property var truePeakStaticViewModel: null
    property var truePeakLiveViewModel: null
    property var terminalPostLimiterTruePeak: null
    // The separately owned MONITOR has no RealizationId or qualified M16 LIVE.
    readonly property bool monitorExclusive: root.terminalMonitorViewModel
        ? root.terminalMonitorViewModel.monitorExclusive : false
    // ACTIVE is a user's REQUEST until independently owned OUTPUT_RATE PCM
    // is installed. During full-programme M17 preparation the audible
    // predecessor is still MANUAL: keep showing its truthful M16 Meter.
    // Never display empty post-limiter telemetry merely due to the ON bit.
    readonly property bool terminalOn: root.terminalLimiterViewModel
        && !root.terminalLimiterViewModel.bypass
        && root.terminalLimiterViewModel.audioStatus === "ACTIVE · AUDITION UNQUALIFIED"
    // Preview Anyway installs real M17 PCM, but ONLY the measured full-
    // resource overshoot is available. The verified-only Windows consumed
    // meter cannot certify this separate nonconforming audio identity.
    // Never misrepresent the live PRE-LIMITER M16 stream as this PCM.
    readonly property bool terminalNonconformingAudible: root.terminalLimiterViewModel
        && !root.terminalLimiterViewModel.bypass
        && root.terminalLimiterViewModel.audioStatus === "ACTIVE · NON-CONFORMING AUDITION"
    readonly property var truePeakCurrentLive: root.monitorExclusive ? null
        : terminalOn ? root.terminalPostLimiterTruePeak
        : root.terminalNonconformingAudible ? null : root.truePeakLiveViewModel
    // Full-resource pre-limiter M16 is a completed, source-bound
    // PROCESSED measurement. M17 may keep that historic figure visible
    // while a distinct MASTER owner is being auditioned: the shared rail
    // explicitly labels it PRE-LIMITER, never MASTER POST-LIMITER.
    // The ordinary M16 editor still refuses an unrelated private owner.
    // The coordinator invalidates this value on SOURCE/PROCESSED changes.
    readonly property var truePeakCurrentStatic: root.isLimiterEditor
        ? root.truePeakStaticViewModel
        : root.monitorExclusive ? null
        : root.terminalOn || root.terminalNonconformingAudible
            ? null : root.truePeakStaticViewModel
    property var terminalLimiterViewModel: null
    property var terminalMonitorViewModel: null
    property var terminalProductActivation: null
    readonly property bool directReturnAvailable: !root.monitorExclusive
        && !!root.terminalProductActivation
        && root.terminalProductActivation.directReturnAvailable
    // Main owns any RAM-caution consent dialog. The editor is a consumer,
    // not a second private-MASTER authority.
    property var masterListeningHandler: null
    // M18 export is a Main-owned Product action. The editor may request an
    // encoding, but cannot grant consent, choose a path or publish the WAV.
    property var masterExportModel: null
    property var masterExportHandler: null
    // Informative click affordance; final consent remains in native M18.
    property bool masterExportAutoReturnAvailable: false
    property int masterExportFormatIndex: 0
    // A result is retained by the native M18 transaction until explicit ACK.
    // Track one actual active worker to reveal the inline receipt after it
    // finishes, without opening any separate modal window.
    property bool m18SawActiveExport: false
    readonly property bool m18ReceiptReady: root.masterExportModel
        && root.masterExportModel.canAcknowledge
    readonly property bool m18ExportFailed: root.masterExportModel
        && root.masterExportModel.errorText.length > 0
    readonly property bool canExportVerifiedWav: root.isLimiterEditor
        && !!root.masterExportModel && root.masterExportModel.canExport
    readonly property bool m18ExportActive: root.isLimiterEditor
        && !!root.masterExportModel && root.masterExportModel.exportInProgress
    Connections {
        target: root.masterExportModel
        function onChanged() {
            const model = root.masterExportModel
            if (!model || !root.isLimiterEditor) return
            if (model.exportInProgress) {
                root.m18SawActiveExport = true
            } else if (root.m18SawActiveExport && model.canAcknowledge) {
                root.m18SawActiveExport = false
                rateTimePopup.open()
            }
        }
    }
    readonly property bool canListenPreparedMaster: root.isLimiterEditor
        && !root.monitorExclusive && !!root.terminalMonitorViewModel
        && root.terminalMonitorViewModel.canInstall
        && !root.terminalMonitorViewModel.monitorPreparing
        && !root.terminalMonitorViewModel.monitorCancelling
    readonly property bool monitorReturnVisible: root.isLimiterEditor
        && ((!!root.terminalMonitorViewModel
            && (root.terminalMonitorViewModel.monitorExclusive
                || root.terminalMonitorViewModel.monitorPreparing
                || root.terminalMonitorViewModel.monitorCancelling))
            || root.directReturnAvailable)
    readonly property bool masterActionsVisible:
        root.canListenPreparedMaster || root.monitorReturnVisible
    // Real Project authority. Selection does not certify Final SRC/M17 PCM.
    property var masterRateState: null
    property var memoryAdvisor: null
    readonly property bool m17Preparing: root.terminalLimiterViewModel
        && (root.terminalLimiterViewModel.audioStatus.startsWith("Preparing")
            || root.terminalLimiterViewModel.nonconformingDevicePreparing)
    // Presentation-only vocabulary. Native M17 state-machine strings,
    // preparation* API names, readiness guards and the PREPARED audition
    // identity remain unchanged. Prepare never means the M17 Master action.
    function m17RenderCopy(value) {
        if (value === null || value === undefined) return ""
        return String(value)
            .replace(/\bNEEDS PREPARATION\b/g, "NEEDS RENDER")
            .replace(/\bPREPARATION\b/g, "RENDERING")
            .replace(/\bPreparing\b/g, "Rendering")
            .replace(/\bpreparation\b/g, "rendering")
            .replace(/\bPrepare\b/g, "Render")
    }
    function rateLabel(hz) {
        if (!hz || hz <= 0) return "—"
        return (hz / 1000).toFixed(1).replace(".0", "") + " kHz"
    }
    // OFFLINE applied gain is immutable MASTER evidence, not a Windows
    // consumed-output LIVE witness. It remains visible during Master Audition.
    property var limiterOfflineGrProvider: null
    property real m17GrTimelineFraction: -1
    property string m17GrTimelineScope: ""
    property Item activeChainRow: null
    // Drag is only the user's tear-off INTENT. Parent/Main owns all window
    // creation and module identity; the live editor never clones itself.
    signal tearOffRequested(real globalX, real globalY)
    // Button replaces brittle native drag with a deterministic action.
    property bool detachedWindow: false
    signal dockToggleRequested()

    // Backward-compatibility alias so code referencing host.viewModel accesses eqViewModel
    property alias viewModel: root.eqViewModel

    // A detached host freezes ONE module adapter by instanceId; the Main
    // dock host continues to follow selection. No DSP-chain node is cloned.
    property var activeModuleOverride: null
    property bool suppressActiveModule: false
    readonly property var activeModule: root.suppressActiveModule ? null
        : root.activeModuleOverride ? root.activeModuleOverride
        : (adapterModel ? adapterModel.activeModule : null)
    readonly property var activeViewModel: activeModule ? (activeModule.gainViewModel ? activeModule.gainViewModel : (activeModule.eqViewModel ? activeModule.eqViewModel : activeModule.compressorViewModel)) : null
    readonly property string moduleTitle: activeModule ? activeModule.displayName : ""
    readonly property string moduleContext: activeModule ? activeModule.workspaceContextLabel : ""
    // M17 PO v1.1: a stable shared Meter/Limiter editor never changes
    // geometry on BYP. Theme follows ANALYSIS Meter when bypassed and
    // Dynamics Copper when truly active; do not recolor the M16 module.
    readonly property bool isLimiterEditor: root.activeModule
        ? root.activeModule.editorContentKey === "TRUE_PEAK_LIMITER" : false
    readonly property bool isMeterEditor: root.activeModule
        ? root.activeModule.editorContentKey === "TRUE_PEAK_METER" : false
    readonly property bool isM19AnalysisEditor: root.activeModule
        ? root.activeModule.editorContentKey === "M19_ANALYSIS" : false
    readonly property bool isM20AnalysisEditor: root.activeModule
        ? root.activeModule.editorContentKey === "M20_ANALYSIS" : false
    readonly property var activeAnalysisFrame: !root.analysisStatusViewModel
        ? ({}) : root.isM19AnalysisEditor
        ? root.analysisStatusViewModel.m19 : root.isM20AnalysisEditor
        ? root.analysisStatusViewModel.m20 : ({})
    readonly property color activeFamilyAccent: root.isLimiterEditor
        ? (root.activeModule.bypass ? "#6CBCCF" : "#C4774A")
        : (root.activeModule ? root.activeModule.familyAccent : "#262A6C9F")

    implicitWidth: 992
    implicitHeight: 612

    // Board02 Editor Panel Material
    radius: 8
    color: "#0F2236"
    // Uniform with all other DSPs: family accent ONLY on the 2px
    // top rule. No special all-around copper/teal pill for M17.
    border.color: "#27465F"
    border.width: 1

    gradient: Gradient {
        GradientStop { position: 0.0; color: "#17324D" }
        GradientStop { position: 1.0; color: "#0C1A2A" }
    }

    // Top highlight using generic familyAccent
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        height: 2
        color: root.activeFamilyAccent
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape && activeChainRow) {
            activeChainRow.forceActiveFocus()
            event.accepted = true
        }
    }

    // Input Gain has no editor-local Escape semantic, so Escape returns focus
    // to its chain row. Parametric EQ numeric fields already use Escape to
    // cancel an in-progress draft; do not let a host Shortcut pre-empt that
    // established editor behavior. Unhandled Escape still bubbles to Keys above.
    Shortcut {
        sequence: "Escape"
        enabled: (activeModule ? activeModule.editorContentKey === "INPUT_GAIN" : false)
            && activeChainRow !== null && !inputGainEditor.editingNumeric
        onActivated: {
            if (activeChainRow) {
                activeChainRow.forceActiveFocus()
            }
        }
    }

    readonly property bool compactTallerEditor: root.activeModule
        && (root.activeModule.editorContentKey === "STEREO_MS"
            || root.activeModule.editorContentKey === "TRUE_PEAK_METER"
            || root.activeModule.editorContentKey === "TRUE_PEAK_LIMITER"
            || root.activeModule.editorContentKey === "STREAMING_TRUE_PEAK_LIMITER"
            || root.activeModule.editorContentKey === "MASTER_MONITOR"
            || root.activeModule.editorContentKey === "M19_ANALYSIS"
            || root.activeModule.editorContentKey === "M20_ANALYSIS"
            || root.activeModule.editorContentKey === "M21_ANALYSIS")

    // PO physical replay: ALL editors share the exact Parametric-EQ
    // 12lp inset and 40lp undock/title/context header at EVERY window
    // height. At a genuinely compact 680x338 host that fixed header
    // leaves 266lp for Stereo/M-S, whose graph+controls need 269lp
    // with 8lp between header and body. Reclaim 6lp ONLY from the
    // below-header gap, without resizing or moving the header itself.
    // This selection depends on editor TYPE, never on window height:
    // no vertical snap or moving title can recur on resize.
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: root.compactTallerEditor ? 2 : 8

        RowLayout {
            objectName: "dspHostHeaderRegion"
            Layout.fillWidth: true
            Layout.minimumHeight: 40
            Layout.preferredHeight: 40
            Layout.maximumHeight: 40
            spacing: 12

            // User-approved FIXED 2x2 utility square, at the SAME x/y
            // for every editor and any docked/floating window. The title
            // shifts right exactly 22px compared with the original grip;
            // never based on the title length. No extra header height.
            // Top: Undock | Reset. Bottom: Open | Save.
            // Read-only analysis preserves empty slots, never fake actions.
            Item {
                id: dockedDragGrip
                objectName: "dspHostTearOffGrip"
                Layout.preferredWidth: 44
                Layout.minimumWidth: 44
                Layout.maximumWidth: 44
                Layout.preferredHeight: 40
                Layout.minimumHeight: 40
                Layout.maximumHeight: 40

                Row {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.leftMargin: 1
                    spacing: 2
                    Item {
                        width: 19
                        height: 19
                        StudioIcon {
                            objectName: "dspHostDockToggleIcon"
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            kind: root.detachedWindow ? "dock" : "undock"
                            strokeColor: "#77C7EA"
                            strokeWidth: 1.85
                        }
                        MouseArea {
                            objectName: "dspHostDockToggleButton"
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: root.dockToggleRequested()
                            Accessible.name: root.detachedWindow
                                ? "Dock module into workspace"
                                : "Undock module to native window"
                        }
                        StudioTooltipDwell {
                            id: hostDockActionDwell
                            anchors.fill: parent
                        }
                        ToolTip.text: root.detachedWindow
                            ? "Dock this module" : "Undock this module"
                        ToolTip.visible: hostDockActionDwell.ready
                        ToolTip.delay: 0
                    }
                    StudioIconButton {
                        objectName: "dspHostResetToDefaultButton"
                        iconKind: "preset-reset"
                        controlSize: 19
                        iconSize: 15
                        visible: !!root.activeModule
                            && root.activeModule.resetSupported
                        enabled: visible && (!root.isLimiterEditor
                            || !root.monitorExclusive)
                        onClicked: if (root.activeModule)
                            root.activeModule.resetToDefault()
                        Accessible.name: root.isLimiterEditor
                            ? "Reset Export TP Limiter Ceiling to 0.0 dBTP"
                            : "Reset " + root.moduleTitle + " to defaults"
                        ToolTip.text: root.isLimiterEditor
                            ? "Restore Export Ceiling to 0.0 dBTP without changing Active or Bypass"
                            : root.activeModule
                                && root.activeModule.editorContentKey === "PARAMETRIC_EQ"
                                ? "Reset all EQ parameters to Flat baseline"
                            : "Restore " + root.moduleTitle
                                + " settings to defaults"
                        ToolTip.visible: hovered && resetHeaderDwell.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: resetHeaderDwell
                            anchors.fill: parent
                        }
                    }
                }

                DspPresetHeaderActions {
                    objectName: "dspModulePresetHeaderActions"
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    width: 42
                    height: 20
                    presetController: root.presetController
                    moduleInstanceId: root.activeModule
                        && !root.activeModule.linkedReadOnly
                        && root.activeModule.editorContentKey !== "TRUE_PEAK_LIMITER"
                        && root.activeModule.editorContentKey !== "TRUE_PEAK_METER"
                        && root.activeModule.editorContentKey !== "MASTER_MONITOR"
                        && root.activeModule.editorContentKey !== "M19_ANALYSIS"
                        && root.activeModule.editorContentKey !== "M20_ANALYSIS"
                        && root.activeModule.editorContentKey !== "M21_ANALYSIS"
                        ? root.activeModule.instanceId : ""
                    moduleDisplayName: root.moduleTitle
                }
            }

            ColumnLayout {
                spacing: 1
                Text {
                    objectName: "dspHostModuleTitle"
                    text: root.moduleTitle
                    color: "#F5F8FC"
                    font.family: "Segoe UI"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
                Text {
                    objectName: "dspHostWorkflowContext"
                    text: root.moduleContext
                    color: "#A1B5C9"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                }
            }

            Item { Layout.fillWidth: true }

            // M19/M20 are read-only modules: use the SAME top-right
            // header slot that editable DSPs use for READY/Active/Bypass.
            // M21 has all three columns simultaneously and intentionally
            // has NO pointless source selector. The identical docked/
            // floating host owns these controls; no duplicated popup.
            RowLayout {
                objectName: "analysisHeaderSourceChooser"
                visible: root.isM19AnalysisEditor || root.isM20AnalysisEditor
                spacing: 5
                Repeater {
                    model: ["PREPARED", "PROCESSED", "REFERENCE"]
                    delegate: StudioButton {
                        objectName: "analysisSourceOption_" + index
                        text: modelData
                        minimumControlWidth: 103
                        selected: !!root.activeAnalysisFrame
                            && root.activeAnalysisFrame.sourceRole === modelData
                        accentColor: "#6CBCCF"
                        enabled: !!root.analysisStatusViewModel
                        onClicked: {
                            if(root.analysisStatusViewModel)
                                root.analysisStatusViewModel.selectPanelSource(
                                    root.isM19AnalysisEditor ? 0 : 1,modelData)
                        }
                        ToolTip.text: modelData === "REFERENCE"
                            ? "Show the independent REFERENCE WAV analysis."
                            : modelData === "PREPARED"
                            ? "Show the PREPARED source before manual DSP processing."
                            : "Show the current rendered DSP output analysis."
                        ToolTip.visible: headerAnalysisDwell.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: headerAnalysisDwell
                            anchors.fill: parent
                        }
                    }
                }
            }

            // Preview Status Badge
            Rectangle {
                objectName: "dspHostPreviewBadge"
                visible: root.activeModule ? root.activeModule.previewSupported : false
                Layout.preferredHeight: 28
                Layout.preferredWidth: statusText.implicitWidth + 24
                radius: 6
                color: {
                    if (!root.activeModule) return "#0F2236"
                    const status = root.activeModule.previewStatus
                    if (status === "RENDERING") return "#2A2814"
                    if (status === "READY") return "#1400D47A"
                    if (status === "ERROR") return "#14F27683"
                    return "#0F2236"
                }
                border.color: {
                    if (!root.activeModule) return "#27465F"
                    const status = root.activeModule.previewStatus
                    if (status === "RENDERING") return "#F2B632"
                    if (status === "READY") return "#00D47A"
                    if (status === "ERROR") return "#F27683"
                    return "#27465F"
                }
                border.width: 1

                Text {
                    id: statusText
                    objectName: "dspHostStatusText"
                    anchors.centerIn: parent
                    text: {
                        if (!root.activeModule) return "NO PREVIEW"
                        const status = root.activeModule.previewStatus
                        if (status === "NO_PREPARED_REALIZATION" || status === "IDLE") return "NO PREVIEW"
                        return status
                    }
                    color: {
                        if (!root.activeModule) return "#A1B5C9"
                        const status = root.activeModule.previewStatus
                        if (status === "RENDERING") return "#F2B632"
                        if (status === "READY") return "#00D47A"
                        if (status === "ERROR") return "#F27683"
                        return "#A1B5C9"
                    }
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
            }

            // Undo / Redo + A/B Bypass Controls
            RowLayout {
                spacing: 2
                // A measuring-only module has no audible Active/Bypass, no
                // parameter changes, and no edit history.
                visible: root.activeModule
                    ? (root.activeModule.bypassSupported || root.activeModule.historySupported)
                    : false

                StudioButton {
                    id: undoButton
                    objectName: "eqUndoButton"
                    text: "Undo"
                    minimumControlWidth: 64
                    visible: true
                    enabled: root.activeModule ? root.activeModule.canUndo : false
                    onClicked: if (root.activeModule) root.activeModule.undo()
                    Accessible.name: "Undo edit"
                    ToolTip.text: "Undo last edit (Ctrl+Z)"
                    ToolTip.visible: (hovered) && stationaryTooltipDwell1.ready
                    ToolTip.delay: 0
                    StudioTooltipDwell {
                        id: stationaryTooltipDwell1
                        anchors.fill: parent
                    }
                    Shortcut {
                        sequences: [ StandardKey.Undo ]
                        enabled: root.activeModule ? (root.activeModule.historySupported && root.activeModule.canUndo) : false
                        onActivated: if (root.activeModule) root.activeModule.undo()
                    }
                }

                StudioButton {
                    id: redoButton
                    objectName: "eqRedoButton"
                    text: "Redo"
                    minimumControlWidth: 64
                    visible: true
                    enabled: root.activeModule ? root.activeModule.canRedo : false
                    onClicked: if (root.activeModule) root.activeModule.redo()
                    Accessible.name: "Redo edit"
                    ToolTip.text: "Redo last edit (Ctrl+Y)"
                    ToolTip.visible: (hovered) && stationaryTooltipDwell2.ready
                    ToolTip.delay: 0
                    StudioTooltipDwell {
                        id: stationaryTooltipDwell2
                        anchors.fill: parent
                    }
                    Shortcut {
                        sequences: [ StandardKey.Redo ]
                        enabled: root.activeModule ? (root.activeModule.historySupported && root.activeModule.canRedo) : false
                        onActivated: if (root.activeModule) root.activeModule.redo()
                    }
                }

                Item {
                    Layout.preferredWidth: 8
                    visible: true
                }

                StudioButton {
                    id: abActiveButton
                    objectName: "abButtonActive"
                    text: "Active"
                    Layout.minimumWidth: 100
                    Layout.preferredWidth: 100
                    Layout.maximumWidth: 100
                    selected: root.activeModule ? !root.activeModule.bypass : true
                    tone: "primary"
                    accentColor: root.isLimiterEditor ? root.activeFamilyAccent : "#00C8FF"
                    selectedFamilyAccent: root.isLimiterEditor
                    // M17 exists as real BYP state but has no processed OUTPUT_RATE
                    // publisher; prevent a clickable false "Active" claim.
                    enabled: !root.isLimiterEditor
                        || (!root.monitorExclusive
                            && root.activeModule && root.activeModule.bypassSupported)
                    onClicked: if (root.activeModule) root.activeModule.setBypass(false)
                    Accessible.name: text
                    ToolTip.text: root.isLimiterEditor && !enabled
                        ? "TP Limiter processing is pending verified OUTPUT_RATE routing"
                        : "Activate module processing"
                    ToolTip.visible: (hovered) && stationaryTooltipDwell3.ready
                    ToolTip.delay: 0
                    StudioTooltipDwell {
                        id: stationaryTooltipDwell3
                        anchors.fill: parent
                    }
                }

                StudioButton {
                    id: abBypassButton
                    objectName: "abButtonBypass"
                    text: "Bypass"
                    selected: root.activeModule ? root.activeModule.bypass : false
                    tone: "gold"
                    accentColor: root.isLimiterEditor ? root.activeFamilyAccent : "#F2B632"
                    selectedFamilyAccent: root.isLimiterEditor
                    enabled: !root.isLimiterEditor
                        || (!root.monitorExclusive
                            && root.activeModule && root.activeModule.bypassSupported)
                    onClicked: if (root.activeModule) root.activeModule.setBypass(true)
                    Accessible.name: "Bypass"
                    ToolTip.text: "Bypass module processing"
                    ToolTip.visible: (hovered) && stationaryTooltipDwell4.ready
                    ToolTip.delay: 0
                    StudioTooltipDwell {
                        id: stationaryTooltipDwell4
                        anchors.fill: parent
                    }
                }
            }
        }

    // Active Editor Content Area
        // M17 explicit offline action: controls do not borrow transport Play.
        RowLayout {
            id: m17ControlsRow
            objectName: "m17ExplicitPreparationControls"
            visible: root.isLimiterEditor || root.isMeterEditor
            Layout.fillWidth: true
            Layout.minimumHeight: 34
            Layout.preferredHeight: 34
            Layout.maximumHeight: 34
            spacing: 8
            StudioButton {
                objectName: "m17RateAndTimesButton"
                visible: root.isLimiterEditor
                text: "WAV Export"
                minimumControlWidth: 106
                accentColor: root.activeFamilyAccent
                selected: rateTimePopup.opened
                selectedFamilyAccent: true
                onClicked: {
                    if (rateTimePopup.opened) rateTimePopup.close()
                    else rateTimePopup.open()
                }
                Accessible.name: rateTimePopup.opened
                    ? "Close WAV export settings" : "Open WAV export settings"
                ToolTip.text: "Select the output sample rate before Render Master; view time/memory estimates. Export verified PCM24 or Float32 afterward."
                ToolTip.visible: hovered && rateDwell.ready
                ToolTip.delay: 0
                StudioTooltipDwell { id: rateDwell; anchors.fill: parent }
            }
            StudioButton {
                visible: root.isLimiterEditor && !root.m17Preparing
                    && !root.m18ExportActive
                objectName: "m17PrepareButton"
                // PREPARED is an audio pipeline role. This action renders
                // and verifies M17's final Master, NOT PREPARED state.
                text: "Render Master"
                minimumControlWidth: 124
                accentColor: "#C4774A"
                enabled: !root.monitorExclusive && root.terminalLimiterViewModel
                    && !root.terminalLimiterViewModel.bypass
                    && root.terminalLimiterViewModel.audioStatus.startsWith("NEEDS PREPARATION")
                    && !!root.masterRateState
                    && root.masterRateState.currentMasterRateAdmissible
                onClicked: root.terminalLimiterViewModel.requestPrepare()
                Accessible.name: "Render and verify full Master with Export TP Limiter"
                ToolTip.text: root.masterRateState
                    && !root.masterRateState.currentMasterRateAdmissible
                    ? root.masterRateState.final_src_path_status(
                        root.masterRateState.masterRateHz)
                    : root.terminalLimiterViewModel
                        && root.terminalLimiterViewModel.audioStatus.startsWith("READY")
                        ? "Master already verified: use Listen to Master or change parameters to render another master."
                        : root.terminalLimiterViewModel
                            && (root.terminalLimiterViewModel.audioStatus.startsWith("NON-CONFORMING")
                                || root.terminalLimiterViewModel.audioStatus.startsWith("ACTIVE"))
                            ? "A measured or active candidate is present. This button cannot override its verification; change a setting and render the Master again."
                            : "Render and verify the full Master; PREPARED audio is unaffected."
                ToolTip.visible: hovered && m17PrepareTooltipDwell.ready
                ToolTip.delay: 0
                StudioTooltipDwell {
                    id: m17PrepareTooltipDwell
                    anchors.fill: parent
                }
            }
            StudioButton {
                visible: root.isLimiterEditor && root.m17Preparing
                objectName: "m17CancelPreparationButton"
                text: "Cancel"
                // Same width as Render Master: WAV Export never jumps
                // during an in-flight full-resource job.
                minimumControlWidth: 124
                enabled: !root.monitorExclusive && root.terminalLimiterViewModel
                    && !root.terminalLimiterViewModel.bypass
                    && (root.terminalLimiterViewModel.audioStatus.startsWith("Preparing")
                        || root.terminalLimiterViewModel.nonconformingDevicePreparing)
                onClicked: root.terminalLimiterViewModel.cancelPreparation()
                Accessible.name: "Cancel offline Master rendering"
            }
            StudioButton {
                visible: root.isLimiterEditor && root.terminalLimiterViewModel && root.terminalLimiterViewModel.audioStatus.startsWith("NON-CONFORMING")
                objectName: "m17PreviewNonconformingButton"
                text: "Preview Anyway"
                minimumControlWidth: 125
                accentColor: "#C85D56"
                enabled: !root.monitorExclusive && root.terminalLimiterViewModel
                    && !root.terminalLimiterViewModel.bypass
                    && root.terminalLimiterViewModel.audioStatus.startsWith("NON-CONFORMING")
                    && (root.terminalLimiterViewModel.nonconformingDirectPreviewAvailable
                        || root.terminalLimiterViewModel.nonconformingDevicePreviewAvailable)
                onClicked: root.terminalLimiterViewModel.previewNonconforming()
                Accessible.name: "Explicit preview of measured Ceiling-nonconforming audio"
                ToolTip.text: !root.terminalLimiterViewModel
                    || (!root.terminalLimiterViewModel.nonconformingDirectPreviewAvailable
                        && !root.terminalLimiterViewModel.nonconformingDevicePreviewAvailable)
                    ? "Device audition is unavailable until the measured PCM has an exact, current Windows playback endpoint."
                    : root.terminalLimiterViewModel.nonconformingDevicePreviewAvailable
                        ? "Explicit NON-CONFORMING preview: convert a separate DEVICE-rate FIR PCM asynchronously. MASTER stays unchanged; LIVE meter is not certified."
                        : "Measured result exceeds requested Ceiling. Direct preview is unverified and requires your explicit choice."
                ToolTip.visible: hovered && m17NonconformingDwell.ready
                ToolTip.delay: 0
                StudioTooltipDwell {
                    id: m17NonconformingDwell
                    anchors.fill: parent
                }
            }
            StudioButton {
                visible: root.isLimiterEditor && root.terminalLimiterViewModel && root.terminalLimiterViewModel.audioStatus.startsWith("READY")
                objectName: "m17PreviewPreparedButton"
                text: "Preview Master"
                minimumControlWidth: 130
                accentColor: "#C4774A"
                enabled: !root.monitorExclusive && root.terminalLimiterViewModel
                    && !root.terminalLimiterViewModel.bypass
                    && root.terminalLimiterViewModel.audioStatus.startsWith("READY")
                onClicked: root.terminalLimiterViewModel.previewPrepared()
                Accessible.name: "Preview rendered Master audio"
            }
            StudioButton {
                objectName: "m17CancelExportButton"
                visible: root.m18ExportActive
                text: root.masterExportModel
                    && root.masterExportModel.statusText === "CANCELLING"
                    ? "Cancelling Export…" : "Cancel Export"
                minimumControlWidth: 128
                accentColor: root.activeFamilyAccent
                enabled: root.m18ExportActive && root.masterExportModel.canCancel
                onClicked: if (root.masterExportModel)
                    root.masterExportModel.cancelExport()
                Accessible.name: text
                ToolTip.text: "Cancel WAV encoding or verification; no file is accepted before readback."
                ToolTip.visible: hovered && m18CancelDwell.ready
                ToolTip.delay: 0
                StudioTooltipDwell { id: m18CancelDwell; anchors.fill: parent }
            }
            StudioButton {
                objectName: "m17ListenMasterButton"
                text: "Listen to Master"
                minimumControlWidth: 124
                accentColor: root.activeFamilyAccent
                // Keep the affordance visible even when the current Master
                // is not verified or has been invalidated. Availability must
                // not be inferred from an absent control.
                visible: root.isLimiterEditor && !root.monitorReturnVisible
                enabled: root.canListenPreparedMaster
                onClicked: {
                    if (root.masterListeningHandler) root.masterListeningHandler()
                    else if (root.terminalMonitorViewModel)
                        root.terminalMonitorViewModel.installMonitor(false)
                }
                Accessible.name: "Listen to the verified Master"
                ToolTip.text: root.canListenPreparedMaster
                    ? "Play the verified Master through the device"
                    : root.terminalLimiterViewModel
                        && (root.terminalLimiterViewModel.audioStatus.startsWith("NON-CONFORMING")
                            || root.terminalLimiterViewModel.audioStatus.startsWith("ACTIVE · NON-CONFORMING"))
                        ? "Master is not Ceiling-verified. Preview Anyway is a separate unqualified audition, not Listen to Master."
                        : "No verified Master is available for this source and rate; Render Master first."
                ToolTip.visible: hovered && m17MasterListenDwell.ready
                ToolTip.delay: 0
                StudioTooltipDwell {
                    id: m17MasterListenDwell
                    anchors.fill: parent
                }
            }
            StudioButton {
                objectName: "m17ReturnSourceButton"
                text: root.terminalMonitorViewModel
                        && root.terminalMonitorViewModel.monitorPreparing
                    ? "Cancel Playback Conversion"
                    : root.terminalMonitorViewModel
                        && root.terminalMonitorViewModel.monitorCancelling
                        ? "Cancelling…"
                        : root.terminalMonitorViewModel
                            && root.terminalMonitorViewModel.returnTargetText.indexOf("Retry") === 0
                            ? "Retry Return to Source"
                            : root.terminalMonitorViewModel
                                && root.terminalMonitorViewModel.returnTargetText.indexOf("Close") === 0
                                ? "Close Master Listening" : "Return to Source"
                minimumControlWidth: 126
                accentColor: root.activeFamilyAccent
                visible: root.monitorReturnVisible
                enabled: visible && root.terminalMonitorViewModel
                    && !root.terminalMonitorViewModel.monitorCancelling
                onClicked: {
                    if (root.terminalMonitorViewModel
                        && (root.terminalMonitorViewModel.monitorExclusive
                            || root.terminalMonitorViewModel.monitorPreparing
                            || root.terminalMonitorViewModel.monitorCancelling))
                        root.terminalMonitorViewModel.cancel()
                    else if (root.directReturnAvailable
                             && root.terminalProductActivation)
                        root.terminalProductActivation.returnToSource()
                }
                Accessible.name: text
            }
            Text {
                objectName: "m17MasterRateSummary"
                visible: root.isLimiterEditor && !root.masterActionsVisible
                Layout.preferredWidth: root.width < 740 ? 132 : 178
                elide: Text.ElideRight
                font.family: "Segoe UI"
                font.pixelSize: 10
                color: "#A9D5E2"
                text: "MASTER " + root.rateLabel(root.masterRateState
                    ? root.masterRateState.masterRateHz : 0) + " · ETA N/A"
                Accessible.name: text + ". Time estimates are not available yet."
            }
            Text {
                objectName: "m17PreparationStatusText"
                visible: root.isLimiterEditor
                Layout.fillWidth: true
                font.family: "Segoe UI"
                font.pixelSize: 11
                color: root.terminalLimiterViewModel
                    && root.terminalLimiterViewModel.audioStatus.startsWith("NON-CONFORMING")
                    ? "#F0C87D" : "#B6CADA"
                elide: Text.ElideRight
                text: root.terminalLimiterViewModel
                    ? root.m17RenderCopy(root.terminalLimiterViewModel.audioStatus)
                    : "UNAVAILABLE"
            }
        }

                // The M17 preparation progress belongs to the shared lower
        // meter band, not to an asymmetric host-only row.
        InputGainEditor {
            showLocalValidation: !root.errorsInGlobalStatusBar
            id: inputGainEditor
            objectName: "inputGainEditor"
            visible: root.activeModule ? (root.activeModule.editorContentKey === "INPUT_GAIN") : false
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.activeModule ? root.activeModule.gainViewModel : null
            onEscapeToChainRequested: {
                if (root.activeChainRow)
                    root.activeChainRow.forceActiveFocus()
            }
        }

        ParametricEqEditor {
            showLocalValidation: !root.errorsInGlobalStatusBar
            id: parametricEqEditor
            objectName: "parametricEqEditor"
            visible: root.activeModule ? (root.activeModule.editorContentKey === "PARAMETRIC_EQ") : false
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.activeModule ? root.activeModule.eqViewModel : null
            spectrumViewModel: root.spectrumViewModel
        }

        Loader {
            id: linkedDynamicEqLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            active: !!root.activeModule && root.activeModule.editorContentKey === "DYNAMIC_EQ"
                && !!root.activeModule.linkedParameters.instanceId
            visible: active
            sourceComponent: Component {
                DynamicEqEditor {
                    LinkedDynamicEqViewModel {
                        id: linkedDynamicModel
                        module: root.activeModule
                    }
                    viewModel: linkedDynamicModel
                    dynamicsControls: linkedDynamicModel
                    dynamicActivity: linkedDynamicActivityModel
                    QtObject {
                        id: linkedDynamicActivityModel
                        readonly property var observation: root.activeModule
                            ? root.activeModule.linkedActivity : ({})
                        readonly property bool available: observation.available === true
                        readonly property var buckets: observation.buckets || []
                        readonly property int bandCount: observation.bandCount || 0
                        readonly property var bandSummaries: linkedDynamicModel.bandSummaries
                        readonly property real audibleFrame: observation.audibleFrame === undefined ? -1 : observation.audibleFrame
                        readonly property int sampleRateHz: observation.sampleRateHz || 0
                        readonly property string stateText: observation.stateText || "TELEMETRY UNAVAILABLE"
                        signal changed()
                        onObservationChanged: changed()
                    }
                    spectrumViewModel: root.spectrumViewModel
                    showLocalValidation: !root.errorsInGlobalStatusBar
                }
            }
        }

        Loader {
            id: linkedTransientLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            active: !!root.activeModule && root.activeModule.editorContentKey === "TRANSIENT_SHAPER"
                && !!root.activeModule.linkedParameters.instanceId
            visible: active
            sourceComponent: Component {
                TransientShaperEditor {
                    QtObject {
                        id: linkedTransientModel
                        readonly property var parameters: root.activeModule
                            ? root.activeModule.linkedParameters : ({})
                        readonly property bool manualEditable: false
                        readonly property real attackAmountDb: parameters.attackAmountDb === undefined ? 0 : parameters.attackAmountDb
                        readonly property real sustainAmountDb: parameters.sustainAmountDb === undefined ? 0 : parameters.sustainAmountDb
                        function setAttackAmountDb(value) {}
                        function setSustainAmountDb(value) {}
                        function resetToDefault() {}
                    }
                    transientModule: linkedTransientModel
                    activity: root.activeModule ? root.activeModule.linkedActivity : ({})
                }
            }
        }

        CompressorEditor {
            showLocalValidation: !root.errorsInGlobalStatusBar
            id: compressorEditor
            objectName: "compressorEditor"
            visible: root.activeModule ? (root.activeModule.editorContentKey === "COMPRESSOR") : false
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.activeModule ? root.activeModule.compressorViewModel : null
        }

        // PO 2026-10-26: every function fits in the real compact editor
        // without vertical scrolling, clipping or changing columns.
        TruePeakMeterEditor {
            id: truePeakMeterEditor
            objectName: "truePeakMeterEditor"
            visible: root.isMeterEditor
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
            measurementProvider: root.truePeakCurrentStatic
            liveProvider: root.truePeakCurrentLive
        }

        TruePeakLimiterEditor {
            showLocalValidation: !root.errorsInGlobalStatusBar
            id: truePeakLimiterEditor
            objectName: "truePeakLimiterEditor"
            visible: root.isLimiterEditor
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
            limiterModule: root.isLimiterEditor ? root.activeModule : null
            monitorLocked: root.monitorExclusive
            limiterViewModel: root.isLimiterEditor ? root.terminalLimiterViewModel : null
            monitorViewModel: root.terminalMonitorViewModel
            exportModel: root.masterExportModel
            limiterOfflineGrProvider: root.isLimiterEditor
                ? root.limiterOfflineGrProvider : null
            grTimelineFraction: root.m17GrTimelineFraction
            grTimelineScope: root.m17GrTimelineScope
            measurementProvider: root.truePeakCurrentStatic
            liveProvider: root.truePeakCurrentLive
        }

        // M19–M21: three persistent read-only pages, like the qualified
        // M16 TruePeakMeterEditor. The existing ColumnLayout is the only
        // geometry owner: hidden pages are excluded from its allocation.
        // Pages belong to the host throughout its dock/floating lifetime,
        // preserving view state across navigation without create/destroy.
        AnalysisStatusEditor {
            id: m19LevelAnalysisEditor
            objectName: "m19LevelProductAnalysisEditor"
            panelIndex: 0
            statusViewModel: root.analysisStatusViewModel
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "M19_ANALYSIS" : false
            enabled: visible
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
        }

        AnalysisStatusEditor {
            id: m20TonalStereoAnalysisEditor
            objectName: "m20TonalStereoProductAnalysisEditor"
            panelIndex: 1
            statusViewModel: root.analysisStatusViewModel
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "M20_ANALYSIS" : false
            enabled: visible
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
        }

        AnalysisStatusEditor {
            id: m21MetricsDeltaAnalysisEditor
            objectName: "m21MetricsDeltaProductAnalysisEditor"
            panelIndex: 2
            statusViewModel: root.analysisStatusViewModel
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "M21_ANALYSIS" : false
            enabled: visible
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
        }

        MonitorEditor {
            id: monitorEditor
            objectName: "monitorEditor"
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "MASTER_MONITOR" : false
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
            monitorViewModel: root.terminalMonitorViewModel
            // The product MASTER rate has ONE authority across both editors.
            // Master Audition does not invent a second independent 48-kHz default.
            masterRateState: root.masterRateState
        }

        StreamingTruePeakLimiterEditor {
            id: streamingTruePeakLimiterEditor
            objectName: "streamingTruePeakLimiterEditor"
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "STREAMING_TRUE_PEAK_LIMITER" : false
            enabled: visible
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            limiterModule: visible ? root.activeModule : null
        }

        StereoMsEditor {
            showLocalValidation: !root.errorsInGlobalStatusBar
            id: stereoMsEditor
            objectName: "stereoMsEditor"
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "STEREO_MS" : false
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
            viewModel: root.activeModule
                ? root.activeModule.stereoMsViewModel : null
        }
    }

    // M17 popover uses the SAME copper/teal family rule and Studio controls
    // as the parent editor, not a separate accent palette. Memory is a measured
    // per-rate PCM/FIR LOWER BOUND, not peak process RSS or DSP reserve.
    Popup {
        id: rateTimePopup
        objectName: "m17RateTimePopup"
        parent: root
        modal: false
        focus: true
        padding: 0
        width: Math.min(458, root.width - 16)
        x: Math.max(8, root.width - width - 8)
        // The popup MUST sit below the action row, not over the Rate &
        // WAV Export button. The popup overlay must never consume its second click.
        y: m17ControlsRow.mapToItem(root, 0, m17ControlsRow.height).y + 6
        // Fixed fit at the 1184x688 Product minimum. No Flickable, scroll
        // view, deferred button or increased editor minimum dimensions.
        height: Math.max(120, Math.min(345, root.height - y - 8))
        // Button toggles open/closed; X and Escape are the other exits.
        // CloseOnPressOutside can consume the button click then re-open.
        closePolicy: Popup.CloseOnEscape
        onOpened: if (root.memoryAdvisor) root.memoryAdvisor.refresh()
        readonly property int selectedRateIndex: root.masterRateState
            ? [44100, 48000, 88200, 96000, 176400, 192000]
                .indexOf(root.masterRateState.masterRateHz) : -1
        readonly property var selectedAdvice: root.memoryAdvisor
            && selectedRateIndex >= 0
            && root.memoryAdvisor.recommendations.length === 6
            ? root.memoryAdvisor.recommendations[selectedRateIndex] : null
        background: Rectangle {
            radius: 7
            color: "#101F30"
            border.color: "#27465F"
            border.width: 1
            // The copper stroke follows the *rounded* outer frame,
            // eliminating the displaced square-ended line at 150% DPI.
            Canvas {
                id: copperTopArc
                anchors.fill: parent
                onPaint: {
                    const c = getContext("2d")
                    c.clearRect(0, 0, width, height)
                    c.beginPath()
                    c.moveTo(1, 7)
                    c.quadraticCurveTo(1, 1, 7, 1)
                    c.lineTo(width - 7, 1)
                    c.quadraticCurveTo(width - 1, 1, width - 1, 7)
                    c.lineWidth = 2
                    c.strokeStyle = root.activeFamilyAccent.toString()
                    c.stroke()
                }
                Connections {
                    target: root
                    function onActiveFamilyAccentChanged() {
                        copperTopArc.requestPaint()
                    }
                }
            }
        }
        contentItem: Item {
            objectName: "m17RateTimePanel"
            clip: true
            ColumnLayout {
                id: rateTimeContent
                anchors.fill: parent
                anchors.margins: 8
                spacing: 2
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    Text {
                        objectName: "m17WavExportPanelTitle"
                        text: "WAV EXPORT · RATE & TIME"
                        color: "#F0F6FB"
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        objectName: "m17RatePanelClose"
                        text: "×"
                        implicitWidth: 25
                        implicitHeight: 22
                        padding: 0
                        onClicked: rateTimePopup.close()
                        Accessible.name: "Close WAV export settings"
                        ToolTip.text: "Close WAV export settings."
                        ToolTip.visible: hovered && panelCloseDwell.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: panelCloseDwell
                            anchors.fill: parent
                        }
                        contentItem: Text {
                            text: "×"
                            font.family: "Segoe UI"
                            font.pixelSize: 14
                            color: "#DDE6F3"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: "#142B3C"
                            radius: 4
                            border.color: "#33526B"
                        }
                    }
                }
                Text {
                    objectName: "m17FinalSrcCandidateStatus"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    color: root.masterRateState
                        && root.masterRateState.currentMasterRateAdmissible
                        ? "#B6D6CA" : "#F2CE82"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    text: root.masterRateState
                        ? root.masterRateState.final_src_path_status(
                            root.masterRateState.masterRateHz)
                        : "SOURCE / target rate unavailable"
                    Accessible.name: text
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    color: "#173249"
                    radius: 3
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 3
                        anchors.rightMargin: 5
                        spacing: 4
                        Text {
                            objectName: "m17RateColumnHeader"
                            Layout.preferredWidth: 112
                            property string interactionHint: "Choose the project MASTER output sample rate. It is independent of the Windows audio device rate."
                            text: "MASTER RATE"
                            HoverHandler { id: rateHeaderHover }
                            ToolTip.text: interactionHint
                            ToolTip.visible: rateHeaderHover.hovered && rateHeaderDwell.ready
                            ToolTip.delay: 0
                            StudioTooltipDwell {
                                id: rateHeaderDwell
                                anchors.fill: parent
                            }
                            color: "#D1E0EB"
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                        Text {
                            objectName: "m17MemoryColumnHeader"
                            Layout.fillWidth: true
                            property string interactionHint: "Conservative minimum for the MASTER and DEVICE PCM plus FIR workspace. This is not the full peak RAM requirement."
                            text: "MEMORY (MIN.)"
                            HoverHandler { id: memoryHeaderHover }
                            ToolTip.text: interactionHint
                            ToolTip.visible: memoryHeaderHover.hovered && memoryHeaderDwell.ready
                            ToolTip.delay: 0
                            StudioTooltipDwell {
                                id: memoryHeaderDwell
                                anchors.fill: parent
                            }
                            color: "#D1E0EB"
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                        Text {
                            objectName: "m17TimeColumnHeader"
                            Layout.preferredWidth: 62
                            property string interactionHint: "HOST ~range is an indicative future total Render Master estimate from three or more accepted full runs. ACT is only a past completed render. DSP estimates are M17/M16 compute projections only, NOT the total Master render. SRC N/A is blocked."
                            text: "TIME"
                            HoverHandler { id: timeHeaderHover }
                            ToolTip.text: interactionHint
                            ToolTip.visible: timeHeaderHover.hovered && timeHeaderDwell.ready
                            ToolTip.delay: 0
                            StudioTooltipDwell {
                                id: timeHeaderDwell
                                anchors.fill: parent
                            }
                            color: "#D1E0EB"
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                    }
                }
                Repeater {
                    model: [44100, 48000, 88200, 96000, 176400, 192000]
                    delegate: Rectangle {
                        id: rateRow
                        required property int index
                        required property int modelData
                        readonly property var advice: root.memoryAdvisor
                            && root.memoryAdvisor.recommendations.length === 6
                            ? root.memoryAdvisor.recommendations[index] : null
                        readonly property bool chosen: root.masterRateState
                            && root.masterRateState.masterRateHz === modelData
                        // Read a NOTIFY-backed C++ property. An opaque Q_INVOKABLE
                        // call with constant modelData cannot by itself track
                        // SOURCE epoch changes, leaving FALSE from first load.
                        readonly property var admissions: root.masterRateState
                            ? root.masterRateState.candidateRateAdmissions : null
                        readonly property bool admissible: admissions
                            && admissions.length === 6
                            && admissions[rateRow.index] === true
                        Layout.fillWidth: true
                        // Six compact 23lp rows reclaim 12lp BELOW
                        // the now-common 40lp EQ header. The radio itself
                        // is 23lp; the popup stays wholly onscreen at min
                        // 1184x688 without changing the header or scrolling.
                        Layout.preferredHeight: 23
                        Layout.minimumHeight: 23
                        Layout.maximumHeight: 23
                        radius: 3
                        color: chosen ? Qt.alpha(root.activeFamilyAccent, 0.15) : "#142B3C"
                        border.width: chosen ? 1 : 0
                        border.color: Qt.alpha(root.activeFamilyAccent, 0.55)
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 3
                            anchors.rightMargin: 5
                            spacing: 4
                            RadioButton {
                                id: compactRateRadio
                                objectName: "m17MasterRateOption_" + rateRow.index
                                property string interactionHint: "Select " + text
                                    + " as Project MASTER output rate, independent of device. "
                                    + (root.masterRateState
                                        ? root.masterRateState.final_src_path_status(
                                            rateRow.modelData)
                                        : "SOURCE / target rate unavailable")
                                text: root.rateLabel(rateRow.modelData)
                                ToolTip.text: interactionHint
                                ToolTip.visible: hovered && rateChoiceDwell.ready
                                ToolTip.delay: 0
                                StudioTooltipDwell {
                                    id: rateChoiceDwell
                                    anchors.fill: parent
                                }
                                checked: rateRow.chosen
                                enabled: root.masterRateState
                                    && root.masterRateState.canEditTarget
                                    && !root.m17Preparing && !root.monitorExclusive
                                implicitWidth: 112
                                implicitHeight: 23
                                spacing: 5
                                indicator: Rectangle {
                                    implicitWidth: 13
                                    implicitHeight: 13
                                    x: 1
                                    y: (compactRateRadio.height - height) / 2
                                    radius: 7
                                    color: "#13293B"
                                    border.width: 1
                                    border.color: compactRateRadio.checked
                                        ? root.activeFamilyAccent : "#7895A9"
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 7
                                        height: 7
                                        radius: 4
                                        color: root.activeFamilyAccent
                                        visible: compactRateRadio.checked
                                    }
                                }
                                contentItem: Text {
                                    text: compactRateRadio.text
                                    font.family: "Segoe UI"
                                    font.pixelSize: 10
                                    color: compactRateRadio.enabled ? "#DDEAF2" : "#93AABD"
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: compactRateRadio.indicator.width + 7
                                    elide: Text.ElideRight
                                }
                                onClicked: if (root.masterRateState)
                                    root.masterRateState.choose_master_rate(rateRow.modelData)
                                Accessible.name: "Target master rate " + text
                            }
                            Text {
                                id: rateMemoryText
                                objectName: "m17RateMemory_" + rateRow.index
                                property string interactionHint: !rateRow.admissible
                                    ? "Final SRC BLOCKED: this SOURCE/MASTER pair cannot render. "
                                        + (rateRow.advice && rateRow.advice.details
                                            ? root.m17RenderCopy(rateRow.advice.details)
                                            : root.masterRateState
                                                ? root.masterRateState.final_src_path_status(
                                                    rateRow.modelData) : "")
                                    : rateRow.advice ? root.m17RenderCopy(rateRow.advice.details)
                                    : "No Windows memory observation is available for this rate."
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                                color: !rateRow.admissible ? "#F0C87D"
                                    : rateRow.advice && rateRow.advice.kind === "discouraged"
                                    ? "#EE9398"
                                    : rateRow.advice && rateRow.advice.kind === "caution"
                                      ? "#F0C87D" : "#B3CAD4"
                                text: !rateRow.admissible
                                    ? (rateRow.advice && rateRow.advice.minimumLabel
                                        ? "SRC blocked · " + rateRow.advice.minimumLabel
                                        : "SRC blocked")
                                    : rateRow.advice
                                    ? (rateRow.advice.minimumLabel
                                        ? rateRow.advice.minimumLabel + " · " : "")
                                      + rateRow.advice.label : "Not assessed"
                                Accessible.name: "Memory: " + text
                                ToolTip.text: interactionHint
                                ToolTip.visible: memoryHover.hovered && memoryDwell.ready
                                ToolTip.delay: 0
                                HoverHandler { id: memoryHover }
                                StudioTooltipDwell {
                                    id: memoryDwell
                                    anchors.fill: parent
                                }
                            }
                            Text {
                                objectName: "m17RateTime_" + rateRow.index
                                readonly property var forecast: root.memoryAdvisor
                                    && root.memoryAdvisor.prepareTimeRows.length === 6
                                    ? root.memoryAdvisor.prepareTimeRows[rateRow.index]
                                    : null
                                property string interactionHint: forecast && forecast.details
                                    ? root.m17RenderCopy(forecast.details)
                                    : "Complete future Master render ETA N/A."
                                Layout.preferredWidth: 62
                                text: forecast && forecast.label
                                    ? root.m17RenderCopy(forecast.kind === "historical_actual"
                                        ? "Last " + forecast.label.replace(/^ACT /, "")
                                        : forecast.label) : "N/A"
                                elide: Text.ElideRight
                                HoverHandler { id: etaHover }
                                ToolTip.text: interactionHint
                                ToolTip.visible: etaHover.hovered && etaDwell.ready
                                ToolTip.delay: 0
                                StudioTooltipDwell {
                                    id: etaDwell
                                    anchors.fill: parent
                                }
                                color: forecast && forecast.kind === "host_future"
                                    ? "#B6D6CA"
                                    : forecast && forecast.kind === "blocked"
                                      ? "#F0C87D" : "#92A9BB"
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                                Accessible.name: text + (forecast
                                    && forecast.isFutureWholePrepare
                                    ? ". Indicative host full-Master-render future range."
                                    : ". Not a whole-Master-render future ETA.")
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    // Reserve a fixed slot rather than letting the dynamic
                    // memory sentence steal the bottom WAV controls at the
                    // real 1184x688 Product minimum.
                    Layout.minimumHeight: 32
                    Layout.preferredHeight: 32
                    Layout.maximumHeight: 32
                    spacing: 6
                    Text {
                        objectName: "m17RateMemorySummary"
                        property string interactionHint: "Available memory uses the smaller of physical RAM and available commit, minus a 25% safety reserve (at least 512 MiB). PCM/FIR is a lower bound; other DSP/Qt and render peaks remain unmeasured."
                        HoverHandler { id: memorySummaryHover }
                        ToolTip.text: interactionHint
                        ToolTip.visible: memorySummaryHover.hovered && memorySummaryDwell.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: memorySummaryDwell
                            anchors.fill: parent
                        }
                        Layout.fillWidth: true
                        Layout.maximumHeight: 32
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: "#A4B9C8"
                        font.family: "Segoe UI"
                        font.pixelSize: 9
                        text: root.memoryAdvisor
                            && root.memoryAdvisor.calibrationRunning
                            ? root.m17RenderCopy(root.memoryAdvisor.calibrationStatus)
                            : rateTimePopup.selectedAdvice
                                && rateTimePopup.selectedAdvice.summary
                                ? root.m17RenderCopy(rateTimePopup.selectedAdvice.summary)
                                : "Memory N/A · Total Master Render ETA N/A"
                    }
                    // Real, explicitly triggered offline M17+M16 probes run
                    // only in a cancellable non-audio worker. The resulting
                    // DSP-only ranges NEVER certify Final SRC or total ETA.
                    Item {
                        id: calibrationHelpTarget
                        objectName: "m17RateCalibrationTooltipTarget"
                        property string interactionHint: root.memoryAdvisor
                            ? root.memoryAdvisor.calibrationStatus
                                + ". Internal PCM only; no DEVICE, user track or WAV."
                            : "Calibration unavailable in this fixture."
                        Layout.preferredWidth: calibrationButton.implicitWidth
                        Layout.preferredHeight: calibrationButton.implicitHeight
                        HoverHandler { id: calibrationHover }
                        ToolTip.text: interactionHint
                        ToolTip.visible: calibrationHover.hovered && calibrationDwell.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: calibrationDwell
                            anchors.fill: parent
                        }
                        StudioButton {
                            id: calibrationButton
                            objectName: "m17RateCalibrationButton"
                            anchors.fill: parent
                            text: root.memoryAdvisor
                                && root.memoryAdvisor.calibrationRunning
                                ? "CANCEL" : "CALIBRATE"
                            minimumControlWidth: 91
                            contentPadding: 10
                            accentColor: root.activeFamilyAccent
                            selectedFamilyAccent: true
                            selected: true
                            showDisabledAccent: true
                            enabled: !!root.memoryAdvisor
                                && !!root.terminalLimiterViewModel
                                && (root.memoryAdvisor.calibrationRunning
                                    || root.masterRateState
                                        && root.masterRateState.hasSupportedMasterRate)
                                && !root.m17Preparing && !root.monitorExclusive
                            onClicked: {
                                if (!root.memoryAdvisor) return
                                if (root.memoryAdvisor.calibrationRunning)
                                    root.memoryAdvisor.cancel_calibration()
                                else if (root.terminalLimiterViewModel)
                                    root.memoryAdvisor.calibrate(
                                        root.terminalLimiterViewModel.ceilingDbTp)
                            }
                            Accessible.name: text + " local M17/M16 only; total Master Render ETA unavailable"
                        }
                    }
                }
                // The two independently labeled format actions are a view
                // over the EXISTING M18 verified-Master export transaction.
                // They neither recalculate M17 nor create a new WAV writer.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#27465F"
                }
                RowLayout {
                    objectName: "m17WavExportActions"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    Layout.minimumHeight: 32
                    Layout.maximumHeight: 32
                    spacing: 6
                    // The SAME fixed 120px slot conveys status, SHA and
                    // ACK. It never adds a row, scroll or moves WAV buttons.
                    RowLayout {
                        objectName: "m18WavInlineReceipt"
                        Layout.minimumWidth: 120
                        Layout.preferredWidth: 120
                        Layout.maximumWidth: 120
                        spacing: 2
                        Text {
                            objectName: "m18WavInlineResult"
                            Layout.fillWidth: true
                            elide: Text.ElideNone
                            color: root.m18ExportFailed ? "#F0A5A5"
                                : !root.m18ExportActive && !root.m18ReceiptReady
                                  && (!root.canExportVerifiedWav || !root.masterExportHandler)
                                  ? "#F0C87D"
                                : root.m18ReceiptReady
                                  && root.masterExportModel.statusText === "VERIFIED DELIVERED"
                                  ? "#B6D6CA" : "#D1E0EB"
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                            text: {
                                const m = root.masterExportModel
                                if (!m) return "WAV OUTPUT"
                                if (root.m18ExportActive) return "EXPORTING"
                                if (m.canAcknowledge) {
                                    if (m.statusText === "VERIFIED DELIVERED")
                                        return root.masterExportFormatIndex === 0
                                            ? "PCM24 ✓" : "FLOAT32 ✓"
                                    return m.statusText === "CANCELLED"
                                        ? "CANCEL" : "FAILED"
                                }
                                if (root.m18ExportFailed) return "ERROR"
                                if (root.masterExportAutoReturnAvailable
                                    && root.masterExportHandler)
                                    return "RETURN + EXPORT"
                                if (!root.canExportVerifiedWav || !root.masterExportHandler) {
                                    // A fixed 120px receipt cannot contain diagnostic
                                    // identifiers. Show a COMPLETE short human label;
                                    // the full native reason remains in the tooltip.
                                    const reason = String(m.exportReadinessReason || "")
                                    if (!root.masterExportHandler) return "EXPORT UNAVAILABLE"
                                    if (reason.indexOf("M17 VERIFIED EPOCH") !== -1)
                                        return "M17 NOT CURRENT"
                                    if (reason.indexOf("STAGE MISSING") !== -1)
                                        return "RENDER REQUIRED"
                                    if (reason.indexOf("BYPASS") !== -1)
                                        return "M17 BYPASSED"
                                    if (reason.indexOf("STALE") !== -1)
                                        return "MASTER STALE"
                                    return "EXPORT BLOCKED"
                                }
                                return "WAV OUTPUT"
                            }
                            HoverHandler { id: m18ResultHover }
                            ToolTip.text: {
                                const m = root.masterExportModel
                                if (!m) return "No verified WAV delivered."
                                if (m.errorText.length > 0) return m.errorText
                                if (m.verifiedSha256.length === 64)
                                    return "Verified WAV SHA-256: " + m.verifiedSha256
                                if (root.masterExportAutoReturnAvailable
                                    && root.masterExportHandler)
                                    return "Export will return to Source and reverify the Master before Save WAV."
                                if (!root.canExportVerifiedWav || !root.masterExportHandler)
                                    return "Export unavailable · " + (!root.masterExportHandler
                                        ? "EXPORT HANDLER UNAVAILABLE"
                                        : m.exportReadinessReason)
                                return "Delivery requires independent WAV readback."
                            }
                            ToolTip.visible: m18ResultHover.hovered
                                && m18ResultDwell.ready
                            ToolTip.delay: 0
                            StudioTooltipDwell {
                                id: m18ResultDwell
                                anchors.fill: parent
                            }
                            Accessible.name: text + " · " + ToolTip.text
                        }
                        // Both receipt actions MUST share the StudioButton
                        // visual grammar and the same 1-second stationary hover
                        // policy as the other RGS controls. No native Basic
                        // Button / immediate tooltip exceptions in M18.
                        StudioButton {
                            objectName: "m18CopyVerifiedSha"
                            visible: !!root.masterExportModel
                                && root.masterExportModel.verifiedSha256.length === 64
                                && root.m18ReceiptReady
                            Layout.minimumWidth: visible ? 36 : 0
                            Layout.preferredWidth: visible ? 36 : 0
                            Layout.maximumWidth: visible ? 36 : 0
                            Layout.minimumHeight: 24
                            Layout.preferredHeight: 24
                            Layout.maximumHeight: 24
                            minimumControlWidth: 36
                            contentPadding: 3
                            accentColor: root.activeFamilyAccent
                            text: "SHA"
                            onClicked: root.masterExportModel.copyVerifiedSha256()
                            Accessible.name: "Copy verified WAV SHA-256 to clipboard"
                            ToolTip.text: "Copy full SHA-256 of the verified delivered WAV"
                            ToolTip.visible: hovered && m18ShaTooltipDwell.ready
                            ToolTip.delay: 0
                            StudioTooltipDwell {
                                id: m18ShaTooltipDwell
                                anchors.fill: parent
                            }
                        }
                        StudioButton {
                            objectName: "m18AcknowledgeExport"
                            visible: root.m18ReceiptReady
                            Layout.minimumWidth: visible ? 32 : 0
                            Layout.preferredWidth: visible ? 32 : 0
                            Layout.maximumWidth: visible ? 32 : 0
                            Layout.minimumHeight: 24
                            Layout.preferredHeight: 24
                            Layout.maximumHeight: 24
                            minimumControlWidth: 32
                            contentPadding: 3
                            accentColor: root.activeFamilyAccent
                            selectedFamilyAccent: true
                            selected: true
                            text: "OK"
                            onClicked: {
                                if (root.masterExportModel
                                    && root.masterExportModel.acknowledgeExport())
                                    root.m18SawActiveExport = false
                            }
                            Accessible.name: "Acknowledge M18 WAV delivery result"
                            ToolTip.text: "Acknowledge the current WAV result to enable another export."
                            ToolTip.visible: hovered && m18AckTooltipDwell.ready
                            ToolTip.delay: 0
                            StudioTooltipDwell {
                                id: m18AckTooltipDwell
                                anchors.fill: parent
                            }
                        }
                    }
                    // Consume all spare width BEFORE both export actions:
                    // FLOAT32's right edge now aligns exactly with CALIBRATE.
                    // This flexible spacer adds no height and no scrolling.
                    Item {
                        objectName: "m18WavOutputRightAlignSpacer"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                    }
                    StudioButton {
                        objectName: "m17ExportPcm24"
                        text: "EXPORT PCM24"
                        minimumControlWidth: 130
                        contentPadding: 9
                        accentColor: root.activeFamilyAccent
                        showDisabledAccent: true
                        enabled: root.canExportVerifiedWav
                            ? !!root.masterExportHandler
                            : root.masterExportAutoReturnAvailable
                              && !!root.masterExportHandler
                        onClicked: {
                            if (!root.masterExportHandler
                                || (!root.canExportVerifiedWav
                                    && !root.masterExportAutoReturnAvailable))
                                return
                            // Main returns true only after synchronous,
                            // exact-owner Return and fresh Product eligibility.
                            if (root.masterExportHandler(0))
                                rateTimePopup.close()
                        }
                        Accessible.name: "Export verified Master as 24-bit PCM WAV with TPDF dither"
                        ToolTip.text: "24-bit PCM with TPDF dither. "
                            + (enabled ? "Choose a NEW WAV filename."
                                       : root.masterExportAutoReturnAvailable
                                         ? "Click to return to Source and verify the Master before choosing a WAV filename."
                                         : "Export unavailable: " + (root.masterExportModel
                                             ? root.masterExportModel.exportReadinessReason
                                             : "M18 model missing"))
                        ToolTip.visible: hovered && exportPcm24Dwell.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: exportPcm24Dwell
                            anchors.fill: parent
                        }
                    }
                    StudioButton {
                        objectName: "m17ExportFloat32"
                        text: "EXPORT FLOAT32"
                        minimumControlWidth: 135
                        contentPadding: 9
                        accentColor: root.activeFamilyAccent
                        showDisabledAccent: true
                        enabled: root.canExportVerifiedWav
                            ? !!root.masterExportHandler
                            : root.masterExportAutoReturnAvailable
                              && !!root.masterExportHandler
                        onClicked: {
                            if (!root.masterExportHandler
                                || (!root.canExportVerifiedWav
                                    && !root.masterExportAutoReturnAvailable))
                                return
                            // Main returns true only after synchronous,
                            // exact-owner Return and fresh Product eligibility.
                            if (root.masterExportHandler(1))
                                rateTimePopup.close()
                        }
                        Accessible.name: "Export verified Master as IEEE 32-bit floating-point WAV without dither"
                        ToolTip.text: "IEEE Float32, no dither. "
                            + (enabled ? "Choose a NEW WAV filename."
                                       : root.masterExportAutoReturnAvailable
                                         ? "Click to return to Source and verify the Master before choosing a WAV filename."
                                         : "Export unavailable: " + (root.masterExportModel
                                             ? root.masterExportModel.exportReadinessReason
                                             : "M18 model missing"))
                        ToolTip.visible: hovered && exportFloat32Dwell.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: exportFloat32Dwell
                            anchors.fill: parent
                        }
                    }
                }
            }
        }
    }
}
