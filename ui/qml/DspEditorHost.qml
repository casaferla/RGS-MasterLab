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
    property var limiterGrProvider: null
    property Item activeChainRow: null

    // Backward-compatibility alias so code referencing host.viewModel accesses eqViewModel
    property alias viewModel: root.eqViewModel

    readonly property var activeModule: adapterModel ? adapterModel.activeModule : null
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Host Header Strip (40 lp)
        RowLayout {
            objectName: "dspHostHeaderRegion"
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            spacing: 12

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

        // Explicit isolated MASTER control popup; the shared meter geometry
    // and ordinary terminal DSP editor remain unchanged.
    MasterMonitorPopup {
        id: monitorPopup
        // Popup positions itself against the application overlay, not
        // the narrow adaptive DSP editor viewport.
        monitorViewModel: root.terminalMonitorViewModel
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
                visible: root.isLimiterEditor
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
                visible: root.isLimiterEditor
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
                visible: root.isLimiterEditor
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
                visible: root.isLimiterEditor
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
                visible: root.isLimiterEditor
                objectName: "m17OpenMasterMonitor"
                text: "MONITOR…"
                minimumControlWidth: 90
                accentColor: "#6CBCCF"
                enabled: root.terminalMonitorViewModel !== null
                    && root.terminalMonitorViewModel !== undefined
                onClicked: monitorPopup.open()
                Accessible.name: "Open experimental MASTER / DEVICE monitor"
            }
            Text {
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

        TruePeakMeterEditor {
            id: truePeakMeterEditor
            objectName: "truePeakMeterEditor"
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "TRUE_PEAK_METER" : false
            Layout.fillWidth: true
            Layout.fillHeight: true
            measurementProvider: root.truePeakCurrentStatic
            liveProvider: root.truePeakCurrentLive
        }

        // The M17 editor is reachable ONLY from a real terminal adapter;
        // M16 retains its independent meter presentation identity.
        TruePeakLimiterEditor {
            showLocalValidation: !root.errorsInGlobalStatusBar
            id: truePeakLimiterEditor
            objectName: "truePeakLimiterEditor"
            visible: root.isLimiterEditor
            Layout.fillWidth: true
            Layout.fillHeight: true
            limiterModule: root.isLimiterEditor ? root.activeModule : null
            monitorLocked: root.monitorExclusive
            // Real typed committed/draft Ceiling; still disabled by authentic
            // BYP, with no sonic Preview and no fabricated applied-GR or trim.
            limiterViewModel: root.isLimiterEditor
                ? root.terminalLimiterViewModel : null
            limiterGrProvider: root.isLimiterEditor && !root.monitorExclusive
                ? root.limiterGrProvider : null
            measurementProvider: root.truePeakCurrentStatic
            liveProvider: root.truePeakCurrentLive
        }

        // MONITOR is a selectable, persistent QML utility editor. Its
        // controls address a separate DEVICE audition lease, never M17 DSP.
        MonitorEditor {
            id: monitorEditor
            objectName: "monitorEditor"
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "MASTER_MONITOR" : false
            Layout.fillWidth: true
            Layout.fillHeight: true
            monitorViewModel: root.terminalMonitorViewModel
        }

        StereoMsEditor {
            showLocalValidation: !root.errorsInGlobalStatusBar
            id: stereoMsEditor
            objectName: "stereoMsEditor"
            visible: root.activeModule ? root.activeModule.editorContentKey === "STEREO_MS" : false
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.activeModule ? root.activeModule.stereoMsViewModel : null
        }
    }
}
