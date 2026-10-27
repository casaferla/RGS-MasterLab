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
        return (hz / 1000).toFixed(1).replace(".", ",").replace(",0", "") + " kHz"
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
                text: "Rate e tempi"
                minimumControlWidth: 106
                accentColor: "#C4774A"
                onClicked: rateTimePopup.open()
                Accessible.name: "Apri frequenze master, memoria e tempi"
                ToolTip.text: "MASTER di progetto; tempi non ancora calibrati"
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
                    ? root.masterRateState.masterRateHz : 0) + " · ETA n/d"
                Accessible.name: text + ". Tempi non ancora disponibili."
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

    // P0 layout trial. Genuine Project rate + RAM; ETA and SRC unavailable
    // until their real backends and the single immutable MASTER are qualified.
    Popup {
        id: rateTimePopup
        objectName: "m17RateTimePopup"
        parent: root
        modal: false
        focus: true
        padding: 0
        width: Math.min(440, root.width - 16)
        x: Math.max(8, root.width - width - 8)
        y: root.height < 470 ? 42 : 52
        height: Math.min(290, root.height - y - 8)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: if (root.memoryAdvisor) root.memoryAdvisor.refresh()
        background: Rectangle {
            radius: 7
            color: "#101F30"
            border.color: "#5B849A"
        }
        contentItem: Item {
            objectName: "m17RateTimePanel"
            clip: true
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 3
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    Text {
                        text: "RATE E TEMPI · MASTER"
                        color: "#F0F6FB"
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        objectName: "m17RatePanelClose"
                        text: "✕"
                        flat: true
                        implicitWidth: 27
                        implicitHeight: 23
                        onClicked: rateTimePopup.close()
                        Accessible.name: "Chiudi Rate e tempi"
                    }
                }
                Text {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 18
                    color: "#F2CE82"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    text: "Target di progetto; Final SRC e MASTER PCM ancora P0"
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
                        Layout.fillWidth: true
                        Layout.preferredHeight: 27
                        radius: 3
                        color: root.masterRateState
                            && root.masterRateState.masterRateHz === modelData
                            ? "#20455B" : "#142B3C"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 3
                            anchors.rightMargin: 5
                            spacing: 4
                            RadioButton {
                                objectName: "m17MasterRateOption_" + rateRow.index
                                text: root.rateLabel(rateRow.modelData)
                                checked: root.masterRateState
                                    && root.masterRateState.masterRateHz === rateRow.modelData
                                enabled: root.masterRateState
                                    && root.masterRateState.hasSupportedMasterRate
                                    && !root.m17Preparing
                                implicitWidth: 110
                                font.pixelSize: 10
                                onClicked: if (root.masterRateState)
                                    root.masterRateState.choose_master_rate(rateRow.modelData)
                                Accessible.name: "Target master " + text
                            }
                            Text {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                font.pixelSize: 10
                                color: rateRow.advice
                                    && rateRow.advice.kind === "discouraged"
                                    ? "#F4A5AA" : "#B3CAD4"
                                text: rateRow.advice ? rateRow.advice.label : "RAM non valutata"
                            }
                            Text {
                                text: "ETA n/d"
                                color: "#8AA7BA"
                                font.pixelSize: 10
                                Layout.preferredWidth: 53
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 6
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#99ADBF"
                        font.pixelSize: 9
                        text: "Stima non disponibile · SRC da qualificare"
                    }
                    Button {
                        objectName: "m17RateCalibrationButton"
                        text: "Calibra"
                        enabled: false
                        implicitHeight: 25
                        ToolTip.text: "Backend di calibrazione non ancora implementato"
                        ToolTip.visible: hovered && calibrationDwell.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: calibrationDwell
                            anchors.fill: parent
                        }
                        Accessible.name: "Calibrazione: non ancora disponibile"
                    }
                }
            }
        }
    }
}
