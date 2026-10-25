import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQml.Models

ApplicationWindow {
    id: root
    objectName: "mainWindow"
    width: 1440
    height: 900
    minimumWidth: 1184
    minimumHeight: 688
    visible: true
    title: "RGS MasterLab"
    color: "#071117"
    flags: Qt.Window

    readonly property color surface: "#0F1820"
    readonly property color surfaceRaised: "#13222F"
    readonly property color panel: "#081218"
    readonly property color border: "#2A3947"
    readonly property color textPrimary: "#E6EEF0"
    readonly property color textSecondary: "#8A97A3"
    readonly property color accent: "#00C8FF"
    readonly property color warning: "#F2B632"
    readonly property color error: "#F27683"

    // Minimum-first desktop composition: the supported 1184 x 688 geometry is
    // already complete and readable. Larger windows expand elastic content
    // continuously; width/height thresholds must not switch control density.
    readonly property bool useAdaptiveDspComposition: true
    readonly property int sourceTransportPanelHeight: 196
    readonly property int transportStripHeight: 72
    readonly property int auditionRegionStripHeight: 72
    readonly property int upperStripHorizontalPadding: 12
    readonly property int auditionRegionVerticalPadding: 6

    // Capture application context objects under unambiguous names before
    // passing them into components that expose same-named properties.
    readonly property var appGainViewModel: typeof gainViewModel !== "undefined" ? gainViewModel : null
    readonly property var appEqViewModel: typeof eqViewModel !== "undefined" ? eqViewModel : null
    readonly property var appTerminalLimiterViewModel: typeof terminalLimiterViewModel !== "undefined"
        ? terminalLimiterViewModel : null
    readonly property var appTerminalMonitorViewModel: typeof terminalMonitorViewModel !== "undefined"
        ? terminalMonitorViewModel : null
    readonly property bool monitorExclusive: root.appTerminalMonitorViewModel
        ? root.appTerminalMonitorViewModel.monitorExclusive : false
    readonly property var appTerminalPostLimiterMeter: typeof terminalPostLimiterTruePeak !== "undefined"
        ? terminalPostLimiterTruePeak : null
    readonly property var appSpectrumViewModel: typeof liveSpectrumViewModel !== "undefined" ? liveSpectrumViewModel : null

    // Single, permanently allocated 24lp status bar. The selected module
    // alone supplies its validation/preview errors; inactive drafts remain
    // local to their owning ViewModel and never replace another DSP status.
    readonly property var statusDspModule: typeof dspChainAdapterModel !== "undefined"
        && dspChainAdapterModel ? dspChainAdapterModel.activeModule : null
    // Single persistent editor instance. Never duplicate/reload the host
    // when a module is detached. At most one shared host floats at a time.
    property bool editorDetached: false
    property string detachedModuleInstanceId: ""
    readonly property string selectedDspInstanceId: root.statusDspModule
        ? root.statusDspModule.instanceId : ""

    function returnCurrentEditorToDock() {
        if (!root.editorDetached) return
        root.editorDetached = false
        root.detachedModuleInstanceId = ""
    }
    function detachSelectedModule() {
        const m = root.statusDspModule
        if (!m || !m.instanceId || !realDspWorkspace.editorHost) return
        if (root.editorDetached) {
            floatingModuleWindow.focusExisting()
            return
        }
        root.detachedModuleInstanceId = m.instanceId
        root.editorDetached = true
        Qt.callLater(function() { floatingModuleWindow.focusExisting() })
    }
    function activateOrFocusModule(module) {
        if (!module || !dspChainAdapterModel) return
        if (root.editorDetached
            && module.instanceId === root.detachedModuleInstanceId) {
            floatingModuleWindow.focusExisting()
            return
        }
        if (root.editorDetached) root.returnCurrentEditorToDock()
        dspChainAdapterModel.selectModuleByInstanceId(module.instanceId)
        realDspWorkspace.editorHost.forceActiveFocus()
    }
    onSelectedDspInstanceIdChanged: {
        // Chain/menu selection of a different instance returns the old
        // editor to the same workspace before switching the persistent host.
        if (root.editorDetached
            && root.detachedModuleInstanceId !== root.selectedDspInstanceId)
            root.returnCurrentEditorToDock()
    }
    readonly property string statusDspKey: statusDspModule
        ? statusDspModule.editorContentKey : ""
    readonly property var statusDspViewModel: {
        const module = root.statusDspModule
        if (!module) return null
        if (root.statusDspKey === "INPUT_GAIN") return module.gainViewModel
        if (root.statusDspKey === "PARAMETRIC_EQ") return module.eqViewModel
        if (root.statusDspKey === "COMPRESSOR") return module.compressorViewModel
        if (root.statusDspKey === "STEREO_MS") return module.stereoMsViewModel
        // Only the SELECTED M17 typed Ceiling VM supplies validation errors.
        // Existing application > degraded-project > DSP severity stays intact.
        if (root.statusDspKey === "TRUE_PEAK_LIMITER")
            return root.appTerminalLimiterViewModel
        return null
    }
    readonly property string statusDspErrorText: {
        const vm = root.statusDspViewModel
        if (!vm) return ""
        let message = root.statusDspKey === "INPUT_GAIN"
            ? vm.validationError : vm.validationMessage
        if ((!message || message.length === 0)
                && vm.previewStatus === "ERROR") {
            message = vm.previewError
        }
        if (!message || message.length === 0) return ""
        return (root.statusDspModule.displayName || "DSP") + ": " + message
    }
    // Keep the established application-failure priority; do not allow a
    // field typo to mask a transport, project, Source or Gold failure.
    readonly property string applicationErrorText: {
        if (root.appTerminalMonitorViewModel
                && root.appTerminalMonitorViewModel.errorText.length > 0)
            return root.appTerminalMonitorViewModel.errorText
        if (projectSession.errorMessage.length > 0) return projectSession.errorMessage
        if (sourceSelection.errorMessage.length > 0) return sourceSelection.errorMessage
        if (goldSelection.errorMessage.length > 0) return goldSelection.errorMessage
        if (auditionSelector.statusText.length > 0) return auditionSelector.statusText
        if (playbackTransport.errorMessage.length > 0) return playbackTransport.errorMessage
        if (auditionRegion.errorMessage.length > 0) return auditionRegion.errorMessage
        return ""
    }
    readonly property string statusSeverity: {
        if (root.applicationErrorText.length > 0) return "ERROR"
        if (projectSession.degraded) return "WARNING"
        if (root.statusDspErrorText.length > 0) return "ERROR"
        return "INFO"
    }
    readonly property bool hasError: root.statusSeverity === "ERROR"

    property string statusText: {
        if (root.applicationErrorText.length > 0) return root.applicationErrorText
        if (projectSession.degraded) return projectSession.statusText
        if (root.statusDspErrorText.length > 0) return root.statusDspErrorText
        if (sourceWaveform.state === "BUILDING") return "Analyzing Source waveform"
        if (!sourceSelection.hasSource) return "Ready | Open a Source WAV"
        return "Ready | " + sourceSelection.sampleRateHz + " Hz | "
            + sourceSelection.sampleFormatLabel + " | " + sourceSelection.channelLayoutLabel
    }

    component StudioMenuItem: MenuItem {
        id: menuItem
        implicitWidth: 230
        implicitHeight: 30
        contentItem: Text { leftPadding: 10; rightPadding: 10; text: menuItem.text; color: menuItem.enabled ? root.textPrimary : "#586773"; font.family: "Segoe UI"; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: menuItem.highlighted && menuItem.enabled ? "#173043" : "transparent" }
    }

    FileDialog {
        id: sourceDialog
        objectName: "sourceFileDialog"
        title: "Open Source WAV"
        fileMode: FileDialog.OpenFile
        nameFilters: ["WAV audio (*.wav *.wave)"]
        onAccepted: {
            if (!root.monitorExclusive) {
                sourceSelection.selectSource(selectedFile)
            }
        }
        onRejected: sourceSelection.cancelSourceSelection()
    }
    FileDialog {
        id: goldDialog
        objectName: "goldFileDialog"
        title: "Open Gold Reference WAV"
        fileMode: FileDialog.OpenFile
        nameFilters: ["WAV audio (*.wav *.wave)"]
        onAccepted: {
            if (!root.monitorExclusive) {
                goldSelection.selectGold(selectedFile)
            }
        }
        onRejected: goldSelection.cancelGoldSelection()
    }
    FileDialog {
        id: projectOpenDialog
        objectName: "projectOpenFileDialog"
        title: "Open RGS MasterLab Project"
        fileMode: FileDialog.OpenFile
        nameFilters: ["RGS MasterLab Project (*.rgsml)"]
        onAccepted: {
            if (!root.monitorExclusive) {
                projectSession.openProject(selectedFile)
            }
        }
        onRejected: projectSession.cancelProjectOpen()
    }
    FileDialog {
        id: projectSaveDialog
        objectName: "projectSaveFileDialog"
        title: "Save RGS MasterLab Project As"
        fileMode: FileDialog.SaveFile
        nameFilters: ["RGS MasterLab Project (*.rgsml)"]
        onAccepted: {
            if (!root.monitorExclusive) {
                projectSession.saveProjectAs(selectedFile)
            }
        }
        onRejected: projectSession.cancelProjectSave()
    }

    Shortcut {
        sequence: "F10"
        context: Qt.ApplicationShortcut
        onActivated: desktopMenu.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    onClosing: function(close) {
        if (root.monitorExclusive && !root.appTerminalMonitorViewModel.cancel()) {
            close.accepted = false
            return
        }
        if (playbackTransport.canStop) playbackTransport.stop()
        close.accepted = true
        Qt.quit()
    }

    // Neutral Item Pool holding real stateful target components for LayoutItemProxy
    Item {
        id: responsiveItemPool
        visible: false
        width: 0
        height: 0

        Rectangle {
            id: realWaveformPanel
            objectName: "sourceWaveformPanel"
            color: "#050C11"
            border.color: root.border
            radius: 5

            Rectangle {
                id: waveformRuler
                objectName: "waveformRuler"
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 1
                height: 20; color: "#0A151D"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                    Label { text: "SOURCE WAVEFORM"; color: root.textSecondary; font.pixelSize: 9; font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }
                    Label { visible: sourceWaveform.canNavigate; text: sourceWaveform.viewportStartText + "    " + sourceWaveform.viewportEndText; color: "#647682"; font.family: "Consolas"; font.pixelSize: 10 }
                }
            }
            WaveformItem {
                id: waveformOverview
                objectName: "sourceWaveformOverview"
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: waveformRuler.bottom; anchors.bottom: parent.bottom
                anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 6; anchors.bottomMargin: 6
                presentation: sourceWaveform
                positionFrames: auditionSelector.sourcePlayheadVisible ? playbackTransport.positionFrames : 0
                durationFrames: playbackTransport.durationFrames
                waveformColor: root.accent
                zeroLineColor: "#29414F"
                playheadColor: auditionSelector.sourcePlayheadVisible ? root.warning : "transparent"
                overrangeColor: root.error
                regionColor: "#2A87A95C"
                regionHandleColor: "#E6EEF0"
                visible: sourceWaveform.ready && sourceWaveform.sourceFrameCount > 0
                activeFocusOnTab: visible
                enabled: !root.monitorExclusive
                KeyNavigation.backtab: sourceOpenButton
                KeyNavigation.tab: zoomOutButton
                Accessible.role: Accessible.Pane
                Accessible.name: sourceWaveform.channelCount === 2 ? "Source waveform, separate left and right lanes" : "Source waveform, mono lane"
                Accessible.description: "Click to seek, drag to pan, Shift drag to create an Audition Region"
            }
            Rectangle {
                visible: waveformOverview.visible && sourceWaveform.channelCount === 2
                anchors.left: waveformOverview.left; anchors.right: waveformOverview.right; anchors.verticalCenter: waveformOverview.verticalCenter
                height: 12; color: "#071117"
                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 1; color: "#20313D" }
            }
            Label { objectName: "sourceWaveformLeftLane"; anchors.left: waveformOverview.left; anchors.leftMargin: 4; anchors.top: waveformOverview.top; anchors.topMargin: 2; visible: waveformOverview.visible && sourceWaveform.channelCount === 2; text: "L"; color: root.textSecondary; font.pixelSize: 10 }
            Label { objectName: "sourceWaveformRightLane"; anchors.left: waveformOverview.left; anchors.leftMargin: 4; anchors.bottom: waveformOverview.bottom; anchors.bottomMargin: 2; visible: waveformOverview.visible && sourceWaveform.channelCount === 2; text: "R"; color: root.textSecondary; font.pixelSize: 10 }
            Label { objectName: "sourceWaveformMonoLane"; anchors.left: waveformOverview.left; anchors.verticalCenter: waveformOverview.verticalCenter; visible: waveformOverview.visible && sourceWaveform.channelCount === 1; text: "C"; color: root.textSecondary; font.pixelSize: 10 }
            Label { id: sourceWaveformStatus; objectName: "sourceWaveformStatus"; anchors.centerIn: waveformOverview; width: waveformOverview.width - 40; visible: !sourceWaveform.ready || sourceWaveform.sourceFrameCount === 0; text: sourceWaveform.statusText; color: sourceWaveform.state === "FAILED" ? root.error : root.textSecondary; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; font.pixelSize: 12 }
            BusyIndicator { objectName: "sourceWaveformBuildingIndicator"; anchors.horizontalCenter: sourceWaveformStatus.horizontalCenter; anchors.bottom: sourceWaveformStatus.top; running: sourceWaveform.state === "BUILDING"; visible: running }
            StudioButton { objectName: "sourceWaveformRetryButton"; anchors.horizontalCenter: sourceWaveformStatus.horizontalCenter; anchors.top: sourceWaveformStatus.bottom; anchors.topMargin: 8; text: "Retry waveform analysis"; visible: sourceWaveform.state === "FAILED"; onClicked: sourceWaveform.requestRetry() }
            Label { anchors.right: waveformOverview.right; anchors.bottom: waveformOverview.bottom; visible: waveformOverview.visible && sourceWaveform.hasOverrange; text: "Source peaks exceed 1.0"; color: root.error; font.pixelSize: 10 }
        }

        Rectangle {
            id: realControlStrip
            objectName: "controlStrip"
            color: "transparent"
            border.color: root.border
            radius: 5
            RowLayout {
                objectName: "controlStripContent"
                anchors.fill: parent
                anchors.leftMargin: root.upperStripHorizontalPadding
                anchors.rightMargin: root.upperStripHorizontalPadding
                spacing: 8
                StudioIconButton { id: zoomOutButton; objectName: "waveformZoomOutButton"; iconKind: "zoom-out"; enabled: sourceWaveform.canNavigate && !sourceWaveform.fullFit; KeyNavigation.backtab: waveformOverview; KeyNavigation.tab: zoomControl; onClicked: sourceWaveform.zoomOut(); Accessible.name: "Zoom out Source waveform" }
                Slider {
                    id: zoomControl
                    objectName: "waveformZoomControl"
                    Layout.preferredWidth: 160
                    from: 0.0; to: 1.0; stepSize: 0.001
                    enabled: sourceWaveform.canNavigate
                    activeFocusOnTab: true
                    KeyNavigation.backtab: zoomOutButton
                    KeyNavigation.tab: zoomInButton
                    onMoved: sourceWaveform.setZoomPosition(value)
                    Accessible.name: "Continuous Source waveform zoom"
                    Binding { target: zoomControl; property: "value"; value: sourceWaveform.zoomPosition; when: !zoomControl.pressed }
                    background: Rectangle { x: zoomControl.leftPadding; y: zoomControl.topPadding + zoomControl.availableHeight / 2 - height / 2; width: zoomControl.availableWidth; height: 6; radius: 3; color: "#253642" }
                    handle: Rectangle { x: zoomControl.leftPadding + zoomControl.visualPosition * (zoomControl.availableWidth - width); y: zoomControl.topPadding + zoomControl.availableHeight / 2 - height / 2; width: 14; height: 14; radius: 7; color: zoomControl.enabled ? "#DDE6F3" : "#586773"; border.color: zoomControl.activeFocus ? root.accent : "#253642"; border.width: zoomControl.activeFocus ? 2 : 1 }
                }
                StudioIconButton { id: zoomInButton; objectName: "waveformZoomInButton"; iconKind: "zoom-in"; enabled: sourceWaveform.canNavigate && sourceWaveform.zoomPosition < 1.0; KeyNavigation.backtab: zoomControl; KeyNavigation.tab: fitSourceButton; onClicked: sourceWaveform.zoomIn(); Accessible.name: "Zoom in Source waveform" }
                StudioIconButton { id: fitSourceButton; objectName: "waveformFitSourceButton"; iconKind: "fit-source"; enabled: sourceWaveform.canNavigate && !sourceWaveform.fullFit; KeyNavigation.backtab: zoomInButton; KeyNavigation.tab: auditionTargetSelector.firstTarget; onClicked: sourceWaveform.fitSource(); Accessible.name: "Fit complete Source waveform" }
                Item { objectName: "controlStripElasticCenter"; Layout.fillWidth: true }
                TransportButton { id: stopButton; objectName: "stopButton"; compact: false; iconKind: "stop"; enabled: !root.monitorExclusive && playbackTransport.canStop; KeyNavigation.backtab: auditionTargetSelector.lastTarget; KeyNavigation.tab: playPauseButton; onClicked: playbackTransport.stop(); Accessible.name: "Stop" }
                Item { Layout.preferredWidth: 8 }
                TransportButton { id: playPauseButton; objectName: "playPauseButton"; compact: false; primary: true; iconKind: playbackTransport.isPlaying ? "pause" : "play"; enabled: !root.monitorExclusive && (playbackTransport.canPlay || playbackTransport.canPause); KeyNavigation.backtab: stopButton; KeyNavigation.tab: auditionStartEditor.firstField; onClicked: playbackTransport.isPlaying ? playbackTransport.pause() : playbackTransport.playOrResume(); Accessible.name: playbackTransport.isPlaying ? "Pause" : "Play" }
                Item { Layout.preferredWidth: 8 }
                StudioButton {
                    objectName: "globalReturnFromMonitor"
                    text: "Return MANUAL"
                    visible: root.monitorExclusive
                    enabled: root.monitorExclusive
                    minimumControlWidth: 110
                    onClicked: root.appTerminalMonitorViewModel.cancel()
                    Accessible.name: "Return original MANUAL PCM or cancel MONITOR"
                }
                Rectangle {
                    objectName: "transportTimeModule"
                    Layout.preferredWidth: 246
                    Layout.preferredHeight: 56
                    color: "#050D12"
                    border.color: root.border
                    radius: 5
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16; spacing: 16
                        ColumnLayout { spacing: 1
                            Label { text: "CURRENT POSITION"; color: root.textSecondary; font.pixelSize: 8; font.weight: Font.DemiBold }
                            Label { objectName: "playbackTimeLabel"; text: playbackTransport.positionLabel; color: root.textPrimary; font.family: "Consolas"; font.pixelSize: 20 }
                        }
                        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; Layout.topMargin: 4; Layout.bottomMargin: 4; color: root.border }
                        ColumnLayout { spacing: 1
                            Label { text: "TOTAL DURATION"; color: root.textSecondary; font.pixelSize: 8; font.weight: Font.DemiBold }
                            Label { text: playbackTransport.durationLabel; color: root.textPrimary; font.family: "Consolas"; font.pixelSize: 20 }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: realRegionPanel
            enabled: !root.monitorExclusive
            objectName: "auditionRegionControls"
            color: "transparent"
            border.color: root.border
            radius: 5
            RowLayout {
                objectName: "auditionRegionContent"
                anchors.fill: parent
                anchors.leftMargin: root.upperStripHorizontalPadding
                anchors.rightMargin: root.upperStripHorizontalPadding
                anchors.topMargin: root.auditionRegionVerticalPadding
                anchors.bottomMargin: root.auditionRegionVerticalPadding
                spacing: 8
                ColumnLayout {
                    spacing: 1
                    Label { text: "START"; color: root.textSecondary; font.pixelSize: 9; font.weight: Font.DemiBold }
                    SegmentedTimeEditor {
                        id: auditionStartEditor; objectName: "auditionRegionStartEditor"; objectPrefix: "auditionRegionStart"; endpointName: "Audition Region start"; enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion
                        hoursText: auditionRegion.startHours; minutesText: auditionRegion.startMinutes; secondsText: auditionRegion.startSeconds; fractionText: auditionRegion.startFraction
                        previousTabItem: playPauseButton; nextTabItem: auditionEndEditor.firstField
                        onCommitRequested: function(hours, minutes, seconds, fraction) { auditionRegion.commitStartSegments(hours, minutes, seconds, fraction) }
                        onEscapeRequested: auditionRegion.clearError(); onNudgeBackwardRequested: auditionRegion.nudgeStartBackward(); onNudgeForwardRequested: auditionRegion.nudgeStartForward()
                    }
                }
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 28; color: root.border }
                ColumnLayout {
                    spacing: 1
                    Label { text: "END"; color: root.textSecondary; font.pixelSize: 9; font.weight: Font.DemiBold }
                    SegmentedTimeEditor {
                        id: auditionEndEditor; objectName: "auditionRegionEndEditor"; objectPrefix: "auditionRegionEnd"; endpointName: "Audition Region end exclusive"; enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion
                        hoursText: auditionRegion.endHours; minutesText: auditionRegion.endMinutes; secondsText: auditionRegion.endSeconds; fractionText: auditionRegion.endFraction
                        previousTabItem: auditionStartEditor.lastField; nextTabItem: fitRegionButton
                        onCommitRequested: function(hours, minutes, seconds, fraction) { auditionRegion.commitEndSegments(hours, minutes, seconds, fraction) }
                        onEscapeRequested: auditionRegion.clearError(); onNudgeBackwardRequested: auditionRegion.nudgeEndBackward(); onNudgeForwardRequested: auditionRegion.nudgeEndForward()
                    }
                }
                Item { Layout.fillWidth: true }
                StudioButton { id: fitRegionButton; objectName: "waveformFitRegionButton"; text: "Fit Region"; iconKind: "fit-region"; minimumControlWidth: 80; enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion; KeyNavigation.backtab: auditionEndEditor.lastField; KeyNavigation.tab: loopRegionCheckBox; onClicked: sourceWaveform.fitRegion(); Accessible.name: "Fit Audition Region in waveform" }
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: root.border }
                StudioToggle { id: loopRegionCheckBox; objectName: "auditionRegionLoopCheckBox"; text: "Loop Region"; checked: auditionRegion.loopEnabled; enabled: auditionRegion.canLoop || auditionRegion.loopEnabled; KeyNavigation.backtab: fitRegionButton; KeyNavigation.tab: clearRegionButton; onClicked: auditionRegion.requestLoopEnabled(checked); Accessible.name: "Loop Audition Region" }
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: root.border }
                StudioButton { id: clearRegionButton; objectName: "auditionRegionClearButton"; text: "Clear Region"; iconKind: "clear-region"; minimumControlWidth: 80; enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion; KeyNavigation.backtab: loopRegionCheckBox; KeyNavigation.tab: sourceOpenButton; onClicked: auditionRegion.requestClearRegion(); Accessible.name: "Clear Audition Region" }
            }
        }

        DspWorkspace {
            id: realDspWorkspace
            objectName: "dspWorkspace"
            adapterModel: typeof dspChainAdapterModel !== "undefined" ? dspChainAdapterModel : null
            moduleActivationHandler: function(module) { root.activateOrFocusModule(module) }
            gainViewModel: root.appGainViewModel
            eqViewModel: root.appEqViewModel
            spectrumViewModel: root.appSpectrumViewModel
            errorsInGlobalStatusBar: true
            truePeakStaticViewModel: typeof truePeakStatic !== "undefined"
                ? truePeakStatic : null
            truePeakLiveViewModel: typeof truePeakLive !== "undefined"
                ? truePeakLive : null
            terminalPostLimiterTruePeak: root.appTerminalPostLimiterMeter
            // The context value is captured on root. Directly reading the
            // same-named child property here creates a QML binding loop.
            terminalLimiterViewModel: root.appTerminalLimiterViewModel
            terminalMonitorViewModel: root.appTerminalMonitorViewModel
            limiterGrProvider: typeof terminalPrivateGrViewModel !== "undefined"
                ? terminalPrivateGrViewModel : null
        }
    }

    // One Qt Window, one rehosted DspEditorHost. No extra editor QML
    // instance or DSP-chain node is ever created by this window.
    DspFloatingModuleWindow {
        id: floatingModuleWindow
        editorTarget: realDspWorkspace.editorHost
        moduleInstanceId: root.detachedModuleInstanceId
        moduleTitle: root.statusDspModule
            ? root.statusDspModule.displayName : "Module"
        familyAccent: root.statusDspModule
            ? root.statusDspModule.familyAccent : "#718092"
        detached: root.editorDetached
        onRedockRequested: function(id) {
            if (id === root.detachedModuleInstanceId)
                root.returnCurrentEditorToDock()
        }
    }

    Column {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: applicationHeader
            objectName: "applicationHeader"
            width: parent.width
            height: 48
            color: root.surface
            border.color: root.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12
                Image {
                    id: headerAppIcon
                    objectName: "headerAppIcon"
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    source: "qrc:/qt/qml/Rgsml/Ui/resources/RGS_ML_C_header_64.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }
                Column {
                    Layout.alignment: Qt.AlignVCenter; spacing: 0
                    Label { text: "RGS MasterLab"; color: root.textPrimary; font.family: "Segoe UI"; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Label { text: "Reference Guided Sound"; color: root.textSecondary; font.family: "Segoe UI"; font.pixelSize: 12 }
                }

                MenuBar {
                    id: desktopMenu
                    objectName: "desktopMenuBar"
                    Layout.preferredWidth: 370
                    Layout.fillHeight: true
                    background: Item { }
                    delegate: MenuBarItem {
                        id: menuBarItem
                        objectName: "desktopMenuBarItem_" + menuBarItem.text.replace("&", "")
                        implicitWidth: contentItem.implicitWidth + 24
                        implicitHeight: 48
                        contentItem: Text { objectName: "desktopMenuBarLabel_" + menuBarItem.text.replace("&", ""); text: menuBarItem.text.replace("&", ""); color: menuBarItem.highlighted ? root.textPrimary : root.textSecondary; font.family: "Segoe UI"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: menuBarItem.highlighted ? root.surfaceRaised : "transparent" }
                    }
                    Menu {
                        objectName: "desktopFileMenu"
                        popupType: Popup.Item
                        width: 230
                        title: "&File"; background: Rectangle { color: root.surface; border.color: root.border }
                        StudioMenuItem { text: "New Project"; enabled: false }
                        StudioMenuItem { objectName: "menuOpenProject"; text: "Open Project…"; enabled: !root.monitorExclusive; onTriggered: projectOpenDialog.open() }
                        MenuSeparator { }
                        StudioMenuItem { objectName: "menuOpenSource"; text: "Open Source\tCtrl+O"; enabled: !root.monitorExclusive; onTriggered: sourceDialog.open() }
                        StudioMenuItem { objectName: "menuOpenGold"; text: "Open Gold Reference"; enabled: !root.monitorExclusive; onTriggered: goldDialog.open() }
                        StudioMenuItem { objectName: "menuClearGold"; text: "Clear Gold Reference"; enabled: !root.monitorExclusive && goldSelection.hasGold; onTriggered: goldSelection.clearGold() }
                        MenuSeparator { }
                        StudioMenuItem { objectName: "menuSaveProjectAs"; text: "Save Project As…"; enabled: projectSession.canSaveProject; onTriggered: projectSaveDialog.open() }
                        StudioMenuItem { text: "Export"; enabled: false }
                        MenuSeparator { }
                        StudioMenuItem { text: "Exit"; onTriggered: root.close() }
                    }
                    Menu {
                        title: "&Edit"
                        background: Rectangle { color: root.surface; border.color: root.border }
                        StudioMenuItem { text: "Undo"; enabled: false }
                        StudioMenuItem { text: "Redo"; enabled: false }
                        MenuSeparator { }
                        StudioMenuItem { text: "Preferences"; enabled: false }
                    }
                    Menu {
                        id: desktopViewMenu
                        objectName: "desktopViewMenu"
                        popupType: Popup.Item
                        width: 230
                        title: "&View"
                        background: Rectangle { color: root.surface; border.color: root.border }
                        // FIX-UI-002: use the same live module-adapter list as
                        // the DSP chain/editor host. No positional DSP menu list.
                        Instantiator {
                            id: viewEditorInstantiator
                            objectName: "viewEditorInstantiator"
                            model: typeof dspChainAdapterModel !== "undefined"
                                && dspChainAdapterModel
                                ? dspChainAdapterModel.modules : []
                            delegate: StudioMenuItem {
                                readonly property var editorAdapter: modelData
                                readonly property string editorKey: editorAdapter
                                    ? editorAdapter.editorContentKey : ""
                                // Keep the historical smoke object names without
                                // hard-coding a menu entry for each DSP type.
                                objectName: "menuView" + editorKey.toLowerCase()
                                    .split("_").map(function(part) {
                                        return part.charAt(0).toUpperCase()
                                            + part.slice(1)
                                    }).join("")
                                text: editorAdapter ? editorAdapter.displayName : ""
                                enabled: editorAdapter !== null
                                    && editorAdapter !== undefined
                                    && editorKey.length > 0
                                onTriggered: {
                                    if (enabled && editorAdapter && dspChainAdapterModel) {
                                        root.activateOrFocusModule(editorAdapter)
                                    }
                                }
                            }
                            onObjectAdded: (index, object) =>
                                desktopViewMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) =>
                                desktopViewMenu.removeItem(object)
                        }
                        MenuSeparator { }
                        StudioMenuItem {
                            objectName: "menuDockUndockSelectedModule"
                            text: root.editorDetached ? "Dock selected module"
                                                      : "Undock selected module"
                            enabled: !!root.statusDspModule
                            onTriggered: {
                                if (root.editorDetached)
                                    root.returnCurrentEditorToDock()
                                else
                                    root.detachSelectedModule()
                            }
                        }
                        MenuSeparator { }
                        StudioMenuItem { text: "Zoom In\tCtrl++"; enabled: sourceWaveform.canNavigate; onTriggered: sourceWaveform.zoomIn() }
                        StudioMenuItem { text: "Zoom Out\tCtrl+-"; enabled: sourceWaveform.canNavigate && !sourceWaveform.fullFit; onTriggered: sourceWaveform.zoomOut() }
                        StudioMenuItem { text: "Fit Source\tHome"; enabled: sourceWaveform.canNavigate && !sourceWaveform.fullFit; onTriggered: sourceWaveform.fitSource() }
                        StudioMenuItem { text: "Fit Region"; enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion; onTriggered: sourceWaveform.fitRegion() }
                    }
                    Menu {
                        title: "&Transport"
                        background: Rectangle { color: root.surface; border.color: root.border }
                        StudioMenuItem { text: playbackTransport.isPlaying ? "Pause\tSpace" : "Play\tSpace"; enabled: !root.monitorExclusive && (playbackTransport.canPlay || playbackTransport.canPause); onTriggered: playPauseButton.clicked() }
                        StudioMenuItem { text: "Stop"; enabled: !root.monitorExclusive && playbackTransport.canStop; onTriggered: playbackTransport.stop() }
                        MenuSeparator { }
                        StudioMenuItem { text: auditionRegion.loopEnabled ? "Disable Loop Region" : "Enable Loop Region"; enabled: !root.monitorExclusive && (auditionRegion.canLoop || auditionRegion.loopEnabled); onTriggered: auditionRegion.requestLoopEnabled(!auditionRegion.loopEnabled) }
                    }
                    Menu {
                        title: "&Help"
                        background: Rectangle { color: root.surface; border.color: root.border }
                        StudioMenuItem { text: "RGS MasterLab Help"; enabled: false }
                        StudioMenuItem { text: "About RGS MasterLab"; enabled: false }
                    }
                }

                Item { Layout.fillWidth: true }
                AuditionSourceSelector { id: auditionTargetSelector; enabled: !root.monitorExclusive; objectName: "headerAuditionTargetSelector"; Layout.preferredWidth: implicitWidth; Layout.alignment: Qt.AlignVCenter; previousTabItem: fitSourceButton; nextTabItem: stopButton }
            }
        }

        // Legacy standard container (inactive while adaptive composition is authoritative)
        ColumnLayout {
            id: standardMainLayout
            objectName: "mainColumnLayout"
            width: parent.width
            height: parent.height - applicationHeader.height - statusBar.height
            visible: !root.useAdaptiveDspComposition
            spacing: 6

            Rectangle {
                id: standardSourcePanel
                objectName: root.useAdaptiveDspComposition ? "standardSourceMetadataPanel" : "sourceMetadataPanel"
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.preferredHeight: 72
                color: root.panel
                border.color: root.border
                radius: 5

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 40; Layout.preferredHeight: 40; radius: 5; color: "#0E2632"; border.color: sourceSelection.hasSource ? root.accent : root.border
                        StudioIcon { anchors.centerIn: parent; width: 24; height: 24; kind: "source"; strokeColor: sourceSelection.hasSource ? root.accent : root.textSecondary }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.fillHeight: true; spacing: 3
                        Label { id: sourceDisplayNameStandard; Layout.fillWidth: true; visible: sourceSelection.hasSource; text: sourceSelection.hasSource ? sourceSelection.displayName : "No Source selected"; color: sourceSelection.hasSource ? root.textPrimary : root.textSecondary; font.family: "Segoe UI"; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 16
                            Label { visible: sourceSelection.hasSource; text: "FORMAT  " + sourceSelection.sampleFormatLabel; color: root.textSecondary; font.pixelSize: 10 }
                            Label { visible: sourceSelection.hasSource; text: "SAMPLE RATE  " + sourceSelection.sampleRateHz + " Hz"; color: root.textSecondary; font.pixelSize: 10 }
                            Label { visible: sourceSelection.hasSource; text: "BIT DEPTH  " + sourceSelection.sampleFormatLabel; color: root.textSecondary; font.pixelSize: 10 }
                            Label { visible: sourceSelection.hasSource; text: "CHANNELS  " + sourceSelection.channelLayoutLabel; color: root.textSecondary; font.pixelSize: 10 }
                            Label { visible: sourceSelection.hasSource; text: "DURATION  " + sourceSelection.durationLabel; color: root.textSecondary; font.pixelSize: 10 }
                            Label { visible: !sourceSelection.hasSource; text: "Choose a WAV Source to begin"; color: "#667783"; font.pixelSize: 10 }
                            Item { Layout.fillWidth: true }
                        }
                    }
                    StudioButton {
                        text: sourceSelection.hasSource ? "Replace Source" : "Open Source"; iconKind: "folder-open"; tone: "primary"; Layout.preferredWidth: 110
                        onClicked: sourceDialog.open(); Accessible.name: text
                    }
                }
            }

            LayoutItemProxy {
                id: standardWaveformProxy
                target: realWaveformPanel
                Layout.fillWidth: true
                Layout.preferredHeight: 180
            }

            LayoutItemProxy {
                id: standardControlStripProxy
                target: realControlStrip
                Layout.fillWidth: true
                Layout.preferredHeight: 72
            }

            LayoutItemProxy {
                id: standardRegionPanelProxy
                target: realRegionPanel
                Layout.fillWidth: true
                Layout.preferredHeight: 72
            }

            RowLayout {
                id: standardDspWorkspaceLayout
                objectName: "standardDspWorkspaceLayout"
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                LayoutItemProxy {
                    id: standardChainProxy
                    target: realDspWorkspace.chainSelector
                    Layout.minimumWidth: 180
                    Layout.preferredWidth: 180
                    Layout.maximumWidth: 180
                    Layout.fillHeight: true
                }

                Item {
                    id: standardEditorSlot
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 0
                    Layout.minimumHeight: 400
                    Layout.preferredHeight: 400
                    LayoutItemProxy {
                        id: standardDspEditorHostProxy
                        anchors.fill: parent
                        visible: !root.editorDetached
                        target: realDspWorkspace.editorHost
                    }
                }
            }
        }

        // Authoritative minimum-first adaptive desktop composition
        ColumnLayout {
            id: compactMainLayout
            width: parent.width
            height: parent.height - applicationHeader.height - statusBar.height
            visible: root.useAdaptiveDspComposition
            spacing: 6

            Rectangle {
                id: compactSourcePanel
                objectName: root.useAdaptiveDspComposition ? "sourceMetadataPanel" : "compactSourceMetadataPanel"
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.preferredHeight: root.sourceTransportPanelHeight
                color: root.panel
                border.color: root.border
                radius: 5

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 6
                    anchors.bottomMargin: 6
                    spacing: 3

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        spacing: 12
                        Rectangle {
                            Layout.preferredWidth: 24; Layout.preferredHeight: 24; radius: 4; color: "#0E2632"; border.color: sourceSelection.hasSource ? root.accent : root.border
                            StudioIcon { anchors.centerIn: parent; width: 16; height: 16; kind: "source"; strokeColor: sourceSelection.hasSource ? root.accent : root.textSecondary }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 1
                            Label { id: sourceDisplayName; objectName: "sourceDisplayName"; Layout.fillWidth: true; visible: sourceSelection.hasSource; text: sourceSelection.hasSource ? sourceSelection.displayName : "No Source selected"; color: sourceSelection.hasSource ? root.textPrimary : root.textSecondary; font.family: "Segoe UI"; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 12
                                Label { objectName: "sourceFormatMetadata"; visible: sourceSelection.hasSource; text: "FORMAT  " + sourceSelection.sampleFormatLabel; color: root.textSecondary; font.pixelSize: 9 }
                                Label { objectName: "sourceRateMetadata"; visible: sourceSelection.hasSource; text: "SAMPLE RATE  " + sourceSelection.sampleRateHz + " Hz"; color: root.textSecondary; font.pixelSize: 9 }
                                Label { objectName: "sourceBitDepthMetadata"; visible: sourceSelection.hasSource; text: "BIT DEPTH  " + sourceSelection.sampleFormatLabel; color: root.textSecondary; font.pixelSize: 9 }
                                Label { objectName: "sourceChannelsMetadata"; visible: sourceSelection.hasSource; text: "CHANNELS  " + sourceSelection.channelLayoutLabel; color: root.textSecondary; font.pixelSize: 9 }
                                Label { objectName: "sourceDurationMetadata"; visible: sourceSelection.hasSource; text: "DURATION  " + sourceSelection.durationLabel; color: root.textSecondary; font.pixelSize: 9 }
                                Label { visible: !sourceSelection.hasSource; text: "Choose a WAV Source to begin"; color: "#667783"; font.pixelSize: 9 }
                                Item { Layout.fillWidth: true }
                            }
                        }
                        StudioButton {
                            id: sourceOpenButton; objectName: "sourceOpenButton"; enabled: !root.monitorExclusive; text: sourceSelection.hasSource ? "Replace Source" : "Open Source"; iconKind: "folder-open"; tone: "primary"; Layout.preferredWidth: 110; Layout.preferredHeight: 32
                            KeyNavigation.backtab: clearRegionButton; KeyNavigation.tab: waveformOverview
                            onClicked: sourceDialog.open(); Accessible.name: text
                        }
                    }

                    ColumnLayout {
                        id: compactControlsContainer
                        objectName: "compactControlsContainer"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 2

                        LayoutItemProxy {
                            id: compactControlStripProxy
                            target: realControlStrip
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.transportStripHeight
                        }

                        LayoutItemProxy {
                            id: compactRegionPanelProxy
                            target: realRegionPanel
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.auditionRegionStripHeight
                        }
                    }
                }
            }

            // Compact Authored Bottom Split (~36% Left Context Area / ~64% Right DSP Editor Area)
            RowLayout {
                id: compactBottomSplit
                objectName: "compactBottomSplit"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                spacing: 8

                // Left Context Area (~36% width, max 420 px width)
                Rectangle {
                    id: compactBottomLeft
                    objectName: "compactBottomLeft"
                    Layout.preferredWidth: Math.min((root.width - 32) * 0.36, 420)
                    Layout.fillHeight: true
                    color: "transparent"

                    RowLayout {
                        anchors.fill: parent
                        spacing: 6

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 6

                        // Upper Left: Waveform Host
                        LayoutItemProxy {
                            id: compactWaveformProxy
                            objectName: "compactWaveformHost"
                            target: realWaveformPanel
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 135
                        }

                        // Lower Left: Adaptive Context Workspace (Prepare / Restoration <-> Gold / Automatch)
                        Rectangle {
                            id: adaptiveContextWorkspace
                            objectName: "adaptiveContextWorkspace"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 212
                            color: root.panel
                            border.color: root.border
                            radius: 5

                            property string activeContextMode: "GOLD_AUTOMATCH"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 6

                                // Compact header: title and mode tabs use separate rows so neither clips.
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumHeight: 44
                                    Layout.preferredHeight: 44
                                    Layout.maximumHeight: 44
                                    spacing: 3

                                    Text {
                                        text: "ADAPTIVE CONTEXT"
                                        color: "#8A9EA8"
                                        font.family: "Segoe UI"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                    }

                                    RowLayout {
                                        spacing: 4

                                        StudioSegmentButton {
                                            objectName: "adaptiveModeToggle_Gold"
                                            text: "Gold / Automatch"
                                            selected: adaptiveContextWorkspace.activeContextMode === "GOLD_AUTOMATCH"
                                            minimumControlWidth: 105
                                            contentPadding: 3
                                            onClicked: adaptiveContextWorkspace.activeContextMode = "GOLD_AUTOMATCH"
                                        }

                                        StudioSegmentButton {
                                            objectName: "adaptiveModeToggle_Restoration"
                                            text: "Prepare / Restoration"
                                            selected: adaptiveContextWorkspace.activeContextMode === "PREPARE_RESTORATION"
                                            minimumControlWidth: 115
                                            contentPadding: 3
                                            onClicked: adaptiveContextWorkspace.activeContextMode = "PREPARE_RESTORATION"
                                        }

                                        Item { Layout.fillWidth: true }
                                    }
                                }

                                // View 1: Gold / Automatch (Default View)
                                Rectangle {
                                    id: goldAutomatchView
                                    objectName: "goldAutomatchView"
                                    visible: adaptiveContextWorkspace.activeContextMode === "GOLD_AUTOMATCH"
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: "#0B1824"
                                    border.color: "#1E354A"
                                    radius: 4

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 6

                                        // Global Row: Gold Reference Card + Global Total Match Amount Rotary Knob
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumHeight: 46
                                            Layout.preferredHeight: 46
                                            Layout.maximumHeight: 46
                                            spacing: 8

                                            // Gold Reference Identity Card
                                            Rectangle {
                                                id: goldReferenceCard
                                                objectName: "goldReferenceCard"
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                color: "#122434"
                                                border.color: goldSelection.hasGold ? root.accent : "#233A4E"
                                                radius: 4

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 8
                                                    anchors.rightMargin: 8
                                                    spacing: 6
                                                    StudioIcon {
                                                        width: 16
                                                        height: 18
                                                        kind: "gold"
                                                        strokeColor: goldSelection.hasGold ? root.accent : "#586773"
                                                    }
                                                    Text {
                                                        id: goldReferenceStatus
                                                        objectName: "goldReferenceStatus"
                                                        text: goldSelection.hasGold ? ("GOLD  " + goldSelection.displayName) : "NO GOLD REFERENCE LOADED"
                                                        color: goldSelection.hasGold ? root.textPrimary : "#586D7C"
                                                        font.family: "Segoe UI"
                                                        font.pixelSize: 10
                                                        font.weight: Font.DemiBold
                                                        elide: Text.ElideRight
                                                        Layout.fillWidth: true
                                                    }
                                                }
                                            }

                                            // Global Control Container: Total Match Amount Rotary Knob
                                            Rectangle {
                                                objectName: "totalMatchAmountContainer"
                                                Layout.preferredWidth: 130
                                                Layout.fillHeight: true
                                                color: "#122434"
                                                border.color: "#2C4356"
                                                radius: 4

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 8
                                                    anchors.rightMargin: 8
                                                    spacing: 6

                                                    // Primary Global Knob
                                                    Rectangle {
                                                        id: totalMatchAmountKnob
                                                        objectName: "totalMatchAmountKnob"
                                                        Layout.preferredWidth: 32
                                                        Layout.preferredHeight: 32
                                                        Layout.alignment: Qt.AlignVCenter
                                                        radius: 16
                                                        color: "#1A3248"
                                                        border.color: root.accent
                                                        border.width: 1.5

                                                        Rectangle {
                                                            width: 2
                                                            height: 10
                                                            color: root.accent
                                                            anchors.top: parent.top
                                                            anchors.topMargin: 3
                                                            anchors.horizontalCenter: parent.horizontalCenter
                                                        }
                                                    }

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        Layout.alignment: Qt.AlignVCenter
                                                        spacing: 1

                                                        Text {
                                                            text: "TOTAL MATCH"
                                                            color: root.textPrimary
                                                            font.family: "Segoe UI"
                                                            font.pixelSize: 9
                                                            font.weight: Font.Bold
                                                        }
                                                        Text {
                                                            text: "0%"
                                                            color: "#586D7C"
                                                            font.family: "Segoe UI"
                                                            font.pixelSize: 8
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        // Domain Controls Row: Four Equal Domain Knobs
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.minimumHeight: 48
                                            color: "#0F2030"
                                            border.color: "#192D3E"
                                            radius: 4

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                spacing: 4

                                                // Domain 1: Tonal
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    Layout.alignment: Qt.AlignVCenter
                                                    spacing: 3

                                                    Rectangle {
                                                        objectName: "automatchKnob_Tonal"
                                                        Layout.preferredWidth: 26
                                                        Layout.preferredHeight: 26
                                                        Layout.alignment: Qt.AlignHCenter
                                                        radius: 13
                                                        color: "#162838"
                                                        border.color: "#2C4356"
                                                        border.width: 1

                                                        Rectangle {
                                                            width: 2
                                                            height: 8
                                                            color: "#586D7C"
                                                            anchors.top: parent.top
                                                            anchors.topMargin: 3
                                                            anchors.horizontalCenter: parent.horizontalCenter
                                                        }
                                                    }

                                                    Text {
                                                        text: "Tonal"
                                                        color: "#586D7C"
                                                        font.family: "Segoe UI"
                                                        font.pixelSize: 9
                                                        Layout.alignment: Qt.AlignHCenter
                                                    }
                                                }

                                                // Domain 2: Dynamics
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    Layout.alignment: Qt.AlignVCenter
                                                    spacing: 3

                                                    Rectangle {
                                                        objectName: "automatchKnob_Dynamics"
                                                        Layout.preferredWidth: 26
                                                        Layout.preferredHeight: 26
                                                        Layout.alignment: Qt.AlignHCenter
                                                        radius: 13
                                                        color: "#162838"
                                                        border.color: "#2C4356"
                                                        border.width: 1

                                                        Rectangle {
                                                            width: 2
                                                            height: 8
                                                            color: "#586D7C"
                                                            anchors.top: parent.top
                                                            anchors.topMargin: 3
                                                            anchors.horizontalCenter: parent.horizontalCenter
                                                        }
                                                    }

                                                    Text {
                                                        text: "Dynamics"
                                                        color: "#586D7C"
                                                        font.family: "Segoe UI"
                                                        font.pixelSize: 9
                                                        Layout.alignment: Qt.AlignHCenter
                                                    }
                                                }

                                                // Domain 3: Stereo
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    Layout.alignment: Qt.AlignVCenter
                                                    spacing: 3

                                                    Rectangle {
                                                        objectName: "automatchKnob_Stereo"
                                                        Layout.preferredWidth: 26
                                                        Layout.preferredHeight: 26
                                                        Layout.alignment: Qt.AlignHCenter
                                                        radius: 13
                                                        color: "#162838"
                                                        border.color: "#2C4356"
                                                        border.width: 1

                                                        Rectangle {
                                                            width: 2
                                                            height: 8
                                                            color: "#586D7C"
                                                            anchors.top: parent.top
                                                            anchors.topMargin: 3
                                                            anchors.horizontalCenter: parent.horizontalCenter
                                                        }
                                                    }

                                                    Text {
                                                        text: "Stereo"
                                                        color: "#586D7C"
                                                        font.family: "Segoe UI"
                                                        font.pixelSize: 9
                                                        Layout.alignment: Qt.AlignHCenter
                                                    }
                                                }

                                                // Domain 4: Loudness
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    Layout.alignment: Qt.AlignVCenter
                                                    spacing: 3

                                                    Rectangle {
                                                        objectName: "automatchKnob_Loudness"
                                                        Layout.preferredWidth: 26
                                                        Layout.preferredHeight: 26
                                                        Layout.alignment: Qt.AlignHCenter
                                                        radius: 13
                                                        color: "#162838"
                                                        border.color: "#2C4356"
                                                        border.width: 1

                                                        Rectangle {
                                                            width: 2
                                                            height: 8
                                                            color: "#586D7C"
                                                            anchors.top: parent.top
                                                            anchors.topMargin: 3
                                                            anchors.horizontalCenter: parent.horizontalCenter
                                                        }
                                                    }

                                                    Text {
                                                        text: "Loudness"
                                                        color: "#586D7C"
                                                        font.family: "Segoe UI"
                                                        font.pixelSize: 9
                                                        Layout.alignment: Qt.AlignHCenter
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                // View 2: Prepare / Restoration (Alternate View)
                                Rectangle {
                                    id: prepareRestorationView
                                    objectName: "prepareRestorationView"
                                    visible: adaptiveContextWorkspace.activeContextMode === "PREPARE_RESTORATION"
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: "#0F2030"
                                    border.color: "#1E354A"
                                    radius: 4

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 4

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 6

                                            Rectangle {
                                                objectName: "sourceHealthCard"
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 28
                                                color: "#142838"; border.color: "#233A4E"; radius: 4
                                                Text { anchors.centerIn: parent; text: "SOURCE HEALTH: OPTIMAL"; color: "#586D7C"; font.pixelSize: 9; font.weight: Font.Bold }
                                            }

                                            Rectangle {
                                                objectName: "restorationPlanCard"
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 28
                                                color: "#142838"; border.color: "#233A4E"; radius: 4
                                                Text { anchors.centerIn: parent; text: "RESTORATION PLAN"; color: "#586D7C"; font.pixelSize: 9; font.weight: Font.Bold }
                                            }
                                        }

                                        // Read-only, actual Windows headroom guidance. This panel
                                        // never changes MASTER OUTPUT_RATE or starts SRC/Render.
                                        Rectangle {
                                            id: restorationControlsGroup
                                            objectName: "restorationControlsGroup"
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            color: "#142838"
                                            border.color: "#233A4E"
                                            radius: 4

                                            onVisibleChanged: {
                                                if (visible) prepareRateAdvisor.refresh()
                                            }
                                            Connections {
                                                target: sourceSelection
                                                function onSourceChanged() {
                                                    if (restorationControlsGroup.visible
                                                            && prepareRestorationView.visible)
                                                        prepareRateAdvisor.refresh()
                                                }
                                            }
                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 4
                                                spacing: 2
                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 18
                                                    spacing: 4
                                                    Text {
                                                        text: "PREPARE · SAMPLE RATE / RAM"
                                                        color: "#A9C7D8"
                                                        font.family: "Segoe UI"
                                                        font.pixelSize: 9
                                                        font.weight: Font.DemiBold
                                                    }
                                                    Item { Layout.fillWidth: true }
                                                    Text {
                                                        objectName: "prepareMemoryDeviceStatus"
                                                        text: prepareRateAdvisor.statusText
                                                        color: "#8FAAB9"
                                                        font.pixelSize: 9
                                                        elide: Text.ElideRight
                                                        Layout.maximumWidth: 120
                                                    }
                                                    Button {
                                                        objectName: "prepareMemoryRefresh"
                                                        text: "↻"
                                                        flat: true
                                                        font.pixelSize: 12
                                                        implicitWidth: 24
                                                        implicitHeight: 18
                                                        onClicked: prepareRateAdvisor.refresh()
                                                        StudioTooltipDwell {
                                                            id: prepareRefreshDwell
                                                            anchors.fill: parent
                                                        }
                                                        ToolTip.text: "Rivaluta la memoria e il dispositivo adesso"
                                                        ToolTip.visible: prepareRefreshDwell.ready
                                                        ToolTip.delay: 0
                                                        Accessible.name: "Rivaluta memoria per Prepare"
                                                    }
                                                }
                                                GridLayout {
                                                    Layout.fillWidth: true
                                                    Layout.fillHeight: true
                                                    columns: 2
                                                    columnSpacing: 4
                                                    rowSpacing: 2
                                                    Repeater {
                                                        model: prepareRateAdvisor.recommendations
                                                        Rectangle {
                                                            required property var modelData
                                                            Layout.fillWidth: true
                                                            Layout.fillHeight: true
                                                            Layout.minimumHeight: 19
                                                            radius: 3
                                                            color: "#0D2130"
                                                            border.color: modelData.kind === "recommended" ? "#3C786C"
                                                                : modelData.kind === "caution" ? "#806A3B"
                                                                : modelData.kind === "discouraged" ? "#82525A"
                                                                : "#365064"
                                                            RowLayout {
                                                                anchors.fill: parent
                                                                anchors.leftMargin: 4
                                                                anchors.rightMargin: 3
                                                                spacing: 3
                                                                Text {
                                                                    text: modelData.rateLabel
                                                                    color: "#E0EBF2"
                                                                    font.pixelSize: 9
                                                                    font.weight: Font.DemiBold
                                                                    Layout.preferredWidth: 56
                                                                }
                                                                Text {
                                                                    id: adviceText
                                                                    text: modelData.label
                                                                    color: modelData.kind === "recommended" ? "#9DD8C2"
                                                                        : modelData.kind === "caution" ? "#F2CE82"
                                                                        : modelData.kind === "discouraged" ? "#F4A5AA"
                                                                        : "#91AABA"
                                                                    font.pixelSize: 9
                                                                    elide: Text.ElideRight
                                                                    Layout.fillWidth: true
                                                                    StudioTooltipDwell {
                                                                        id: prepareAdviceDwell
                                                                        anchors.fill: parent
                                                                    }
                                                                    ToolTip.text: modelData.details
                                                                    ToolTip.visible: prepareAdviceDwell.ready
                                                                    ToolTip.delay: 0
                                                                }
                                                            }
                                                            Accessible.name: modelData.rateLabel + ": " + modelData.label
                                                            Accessible.description: modelData.details
                                                        }
                                                    }
                                                }
                                                Text {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 12
                                                    text: "Stima PCM minima · CPU non qualificata · rivalutare prima di allocare"
                                                    font.pixelSize: 8
                                                    color: "#8FAAB9"
                                                    elide: Text.ElideRight
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Right Edge of Left Context Area: Vertical Single-Column DSP Chain (frozen 164 px)
                    LayoutItemProxy {
                        id: compactChainProxy
                        objectName: "compactChainContainer"
                        target: realDspWorkspace.chainSelector
                        Layout.minimumWidth: 164
                        Layout.preferredWidth: 164
                        Layout.maximumWidth: 164
                        Layout.fillHeight: true
                    }
                }
                }

                // Right DSP Editor Area (~57% width)
                Item {
                    id: compactEditorSlot
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 0
                    Layout.minimumHeight: 300
                    Layout.preferredHeight: 300
                    LayoutItemProxy {
                        id: compactDspEditorHostProxy
                        objectName: "compactBottomRight"
                        anchors.fill: parent
                        visible: !root.editorDetached
                        target: realDspWorkspace.editorHost
                    }
                }
            }
        }

        Rectangle {
            id: statusBar
            objectName: "statusBar"
            width: parent.width
            height: 24
            color: "#0B151C"
            border.color: root.border
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 8
                Rectangle {
                    objectName: "statusReadyIndicator"
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    radius: 4
                    color: root.statusSeverity === "ERROR" ? root.error
                        : root.statusSeverity === "WARNING" ? root.warning : "#00E6E6"
                }
                Label {
                    id: statusMessage
                    objectName: "statusBarMessage"
                    Layout.fillWidth: true
                    text: root.statusText
                    color: root.statusSeverity === "ERROR" ? root.error
                        : root.statusSeverity === "WARNING" ? root.warning : root.textSecondary
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    HoverHandler { id: globalStatusHover }
                    ToolTip.text: root.statusText
                    ToolTip.visible: (globalStatusHover.hovered && root.statusText.length > 0) && stationaryTooltipDwell1.ready
                    ToolTip.delay: 0
                    StudioTooltipDwell {
                        id: stationaryTooltipDwell1
                        anchors.fill: parent
                    }
                    Accessible.name: root.statusText
                    Accessible.description: root.statusSeverity + ": " + root.statusText
                }
                Text {
                    id: statusBuildProvenanceLabel
                    objectName: "statusBuildProvenanceLabel"
                    text: typeof buildProvenance !== "undefined" ? buildProvenance : "DEV"
                    color: root.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }

    Label { objectName: "playbackErrorMessage"; visible: false; text: playbackTransport.errorMessage }
    Label { objectName: "goldErrorLabel"; visible: false; text: goldSelection.errorMessage }
    Label { objectName: "auditionRoutingStatus"; visible: false; text: auditionSelector.statusText }
    Label { objectName: "goldStateLabel"; visible: false; text: goldSelection.hasGold ? "Gold: " + goldSelection.displayName : "Gold: not loaded" }
    Label { objectName: "sourceEmptyState"; visible: !sourceSelection.hasSource; opacity: 0; width: 0; height: 0; text: "No Source selected" }
    Label { objectName: "sourceReadOnlyBadge"; visible: sourceSelection.hasSource; opacity: 0; width: 0; height: 0; text: "Read-only" }
    Label { objectName: "sourceErrorMessage"; visible: sourceSelection.errorMessage.length > 0; opacity: 0; width: 0; height: 0; text: sourceSelection.errorMessage }
    Label { objectName: "sourceContainerMetadata"; visible: false; text: "Container  " + sourceSelection.containerLabel }
    Label { objectName: "sourceFramesMetadata"; visible: false; text: "Frames  " + sourceSelection.frameCount }
    Label { objectName: "playbackStateLabel"; visible: false; text: playbackTransport.stateLabel }
}

