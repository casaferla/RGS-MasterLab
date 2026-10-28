import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspEditorHost"

    property var adapterModel: null
    // Opt-in via Main only. Standalone DSP editor fixtures preserve their
    // historical local validation feedback for isolated smoke coverage.
    property bool errorsInGlobalStatusBar: false
    property int selectedModuleIndex: adapterModel ? adapterModel.selectedIndex : 0
    property var gainViewModel: null
    property var eqViewModel: null
    property var compressorViewModel: null
    property var spectrumViewModel: null
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
    readonly property var truePeakCurrentStatic: root.monitorExclusive ? null
        : terminalOn || root.terminalNonconformingAudible
            ? null : root.truePeakStaticViewModel
    property var terminalLimiterViewModel: null
    property var terminalMonitorViewModel: null
    // Real Project authority. Selection does not certify Final SRC/M17 PCM.
    property var masterRateState: null
    property var memoryAdvisor: null
    readonly property bool m17Preparing: root.terminalLimiterViewModel
        && root.terminalLimiterViewModel.audioStatus.startsWith("Preparing")
    function rateLabel(hz) {
        if (!hz || hz <= 0) return "—"
        return (hz / 1000).toFixed(1).replace(".0", "") + " kHz"
    }
    property var limiterGrProvider: null
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
            || root.activeModule.editorContentKey === "MASTER_MONITOR")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compactTallerEditor && root.height < 470 ? 8 : 12
        spacing: root.compactTallerEditor && root.height < 470 ? 5 : 8

        // Host Header Strip (40 lp)
        RowLayout {
            objectName: "dspHostHeaderRegion"
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactTallerEditor && root.height < 470 ? 34 : 40
            spacing: 12

            // Preserve the EXACT old 22px slot: the module title's X
            // coordinate must not shift when docked/undocked.
            Item {
                id: dockedDragGrip
                objectName: "dspHostTearOffGrip"
                Layout.preferredWidth: 22
                Layout.minimumWidth: 22
                Layout.maximumWidth: 22
                Layout.preferredHeight: 30
                StudioIcon {
                    objectName: "dspHostDockToggleIcon"
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    kind: root.detachedWindow ? "dock" : "undock"
                    strokeColor: "#77C7EA"
                    strokeWidth: 2.0
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

                // M17 uses the same top-right Reset affordance as other DSP
                // editors. It only changes Ceiling; BYPASS and layout stay put.
                StudioButton {
                    objectName: "m17ResetToDefaultButton"
                    text: "Reset to Default"
                    minimumControlWidth: 140
                    visible: root.isLimiterEditor
                    enabled: root.isLimiterEditor && !root.monitorExclusive
                        && root.activeModule && root.activeModule.resetSupported
                    onClicked: if (root.activeModule) root.activeModule.resetToDefault()
                    Accessible.name: "Reset True Peak Limiter Ceiling to -1.0 dBTP"
                    ToolTip.text: "Restore Ceiling to -1.0 dBTP without changing Active or Bypass"
                    ToolTip.visible: hovered && m17ResetTooltipDwell.ready
                    ToolTip.delay: 0
                    StudioTooltipDwell {
                        id: m17ResetTooltipDwell
                        anchors.fill: parent
                    }
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
                        ? "True Peak Limiter processing is pending verified OUTPUT_RATE routing"
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
            objectName: "m17ExplicitPreparationControls"
            visible: root.isLimiterEditor || root.isMeterEditor
            Layout.fillWidth: true
            Layout.minimumHeight: 34
            Layout.preferredHeight: 34
            Layout.maximumHeight: 34
            spacing: 8
            StudioButton {
                visible: root.isLimiterEditor && !root.m17Preparing
                objectName: "m17PrepareButton"
                text: "Prepare"
                minimumControlWidth: 86
                accentColor: "#C4774A"
                enabled: !root.monitorExclusive && root.terminalLimiterViewModel
                    && !root.terminalLimiterViewModel.bypass
                    && root.terminalLimiterViewModel.audioStatus.startsWith("NEEDS PREPARATION")
                onClicked: root.terminalLimiterViewModel.requestPrepare()
                Accessible.name: "Prepare offline true peak limiter"
                ToolTip.text: "Render and verify the full master; Play never starts this job."
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
                minimumControlWidth: 80
                enabled: !root.monitorExclusive && root.terminalLimiterViewModel
                    && !root.terminalLimiterViewModel.bypass
                    && root.terminalLimiterViewModel.audioStatus.startsWith("Preparing")
                onClicked: root.terminalLimiterViewModel.cancelPreparation()
                Accessible.name: "Cancel offline limiter preparation"
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
                onClicked: root.terminalLimiterViewModel.previewNonconforming()
                Accessible.name: "Explicit preview of measured Ceiling-nonconforming audio"
                ToolTip.text: "Measured result exceeds requested Ceiling. Preview is unverified, only on your explicit choice."
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
                text: "Preview Prepared"
                minimumControlWidth: 130
                accentColor: "#C4774A"
                enabled: !root.monitorExclusive && root.terminalLimiterViewModel
                    && !root.terminalLimiterViewModel.bypass
                    && root.terminalLimiterViewModel.audioStatus.startsWith("READY")
                onClicked: root.terminalLimiterViewModel.previewPrepared()
                Accessible.name: "Preview verified limiter audio"
            }
            StudioButton {
                objectName: "m17RateAndTimesButton"
                visible: root.isLimiterEditor
                text: "Rate & Time"
                minimumControlWidth: 106
                accentColor: root.activeFamilyAccent
                onClicked: rateTimePopup.open()
                Accessible.name: "Open master rate, memory and estimated processing time"
                ToolTip.text: "Select a project MASTER rate and review memory; time estimates require local calibration."
                ToolTip.visible: hovered && rateDwell.ready
                ToolTip.delay: 0
                StudioTooltipDwell { id: rateDwell; anchors.fill: parent }
            }
            Text {
                objectName: "m17MasterRateSummary"
                visible: root.isLimiterEditor
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
                color: "#B6CADA"
                elide: Text.ElideRight
                text: root.terminalLimiterViewModel
                    ? root.terminalLimiterViewModel.audioStatus : "UNAVAILABLE"
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
            limiterGrProvider: root.isLimiterEditor && !root.monitorExclusive
                ? root.limiterGrProvider : null
            measurementProvider: root.truePeakCurrentStatic
            liveProvider: root.truePeakCurrentLive
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
        y: root.height < 470 ? 42 : 52
        height: Math.min(290, root.height - y - 8)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
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
            // Uniform module-family top rule; do NOT make an all-copper frame.
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 1
                height: 2
                color: root.activeFamilyAccent
            }
        }
        contentItem: Item {
            objectName: "m17RateTimePanel"
            clip: true
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 2
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    Text {
                        text: "MASTER RATE & TIME"
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
                        Accessible.name: "Close master rate and time panel"
                        ToolTip.text: "Close the master rate and time panel."
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
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    color: "#F2CE82"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    text: "Project rate only · Final SRC/MASTER integration pending"
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
                            property string interactionHint: "Predicted offline processing duration for this master rate. N/A until real on-device calibration is available."
                            text: "EST. TIME"
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
                        Layout.fillWidth: true
                        Layout.preferredHeight: 27
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
                                    + " as the project MASTER output rate. Changing this preference does not change the audio device rate. Full Final SRC integration is pending."
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
                                    && root.masterRateState.hasSupportedMasterRate
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
                                property string interactionHint: rateRow.advice
                                    ? rateRow.advice.details
                                    : "No Windows memory observation is available for this rate."
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                                color: rateRow.advice && rateRow.advice.kind === "discouraged"
                                    ? "#EE9398"
                                    : rateRow.advice && rateRow.advice.kind === "caution"
                                      ? "#F0C87D" : "#B3CAD4"
                                text: rateRow.advice
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
                                property string interactionHint: "No estimated processing time yet: per-rate M17/M16 and Final SRC calibration has not been integrated."
                                Layout.preferredWidth: 62
                                text: "N/A"
                                HoverHandler { id: etaHover }
                                ToolTip.text: interactionHint
                                ToolTip.visible: etaHover.hovered && etaDwell.ready
                                ToolTip.delay: 0
                                StudioTooltipDwell {
                                    id: etaDwell
                                    anchors.fill: parent
                                }
                                color: "#92A9BB"
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                                Accessible.name: "Estimated processing time not yet calibrated"
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
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
                        wrapMode: Text.WordWrap
                        color: "#A4B9C8"
                        font.family: "Segoe UI"
                        font.pixelSize: 9
                        text: rateTimePopup.selectedAdvice
                            && rateTimePopup.selectedAdvice.summary
                            ? rateTimePopup.selectedAdvice.summary
                            : "Memory N/A · Estimated time N/A · Final SRC pending"
                    }
                    // The help target remains enabled for hover even though
                    // CALIBRATE itself is genuinely disabled: no action occurs.
                    Item {
                        id: calibrationHelpTarget
                        objectName: "m17RateCalibrationTooltipTarget"
                        property string interactionHint: "CALIBRATE will measure real offline M17/M16 processing on this computer for more accurate time estimates. Not available until the calibrator is integrated."
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
                            text: "CALIBRATE"
                            minimumControlWidth: 91
                            contentPadding: 10
                            accentColor: root.activeFamilyAccent
                            selectedFamilyAccent: true
                            selected: true
                            showDisabledAccent: true
                            enabled: false
                            Accessible.name: "Calibrate processing time: not yet available"
                        }
                    }
                }
            }
        }
    }
}
