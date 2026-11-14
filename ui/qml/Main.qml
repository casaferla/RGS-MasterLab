import QtQuick
import QtQuick.Window
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
    // Explicitly confirmed in-RAM MASTER loss alone permits exiting a
    // recovery state without reinstalling the original Windows DEVICE.
    property bool m17ConfirmedDiscardExit: false
    readonly property bool m17RecoveryExitRequired: root.monitorExclusive
        && !!root.appTerminalMonitorViewModel
        && (String(root.appTerminalMonitorViewModel.returnTargetText)
                .indexOf("Retry") === 0
            || String(root.appTerminalMonitorViewModel.statusText)
                .indexOf("RECOVERY REQUIRED") !== -1)

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
    readonly property var appTerminalProductActivation: typeof terminalProductActivation !== "undefined"
        ? terminalProductActivation : null
    readonly property var appMasterRateState: typeof masterOutputRateState !== "undefined"
        ? masterOutputRateState : null
    // M18 Export becomes available only when the native Product owner
    // has installed all epoch guards and retained an exact verified M17 Master.
    readonly property var appM18ExportModel: typeof m18ProductExportViewModel !== "undefined"
        ? m18ProductExportViewModel : null
    // Native Project owns the current explicit WAV encoding preference.
    // Loading another project restores the combo without a sticky
    // QML-only choice; TPDF uint64 seed never traverses QML Number.
    readonly property var appM18ProjectSession: typeof projectSession !== "undefined"
        ? projectSession : null
    readonly property int m18ExportFormatIndex: root.appM18ProjectSession
        ? root.appM18ProjectSession.m18WavEncodingIndex : 0
    readonly property var appMemoryAdvisor: typeof prepareRateAdvisor !== "undefined"
        ? prepareRateAdvisor : null
    readonly property var appPrivateFirLibrary:
        typeof privateOriginalFirLibrary !== "undefined"
        ? privateOriginalFirLibrary : null
    readonly property bool monitorExclusive: root.appTerminalMonitorViewModel
        ? root.appTerminalMonitorViewModel.monitorExclusive : false
    // A read-only navigation clock shared with the main full-track waveform.
    // MASTER during anonymous Master Audition; SOURCE when its playhead is
    // visible. This is NOT a Windows-consumed GR witness or second timer.
    readonly property real m17GrTimelineFraction:
        waveformOverview.visible
        && (root.monitorExclusive || auditionSelector.sourcePlayheadVisible)
        && waveformOverview.durationFrames > 0
        ? Math.max(0, Math.min(1,
            waveformOverview.positionFrames / waveformOverview.durationFrames))
        : -1
    readonly property string m17GrTimelineScope:
        root.monitorExclusive ? "MASTER"
        : auditionSelector.sourcePlayheadVisible ? "SOURCE" : ""
    // PO 2026-10-27 §12: Master listening belongs to MAIN transport.
    // The historic service editor remains temporarily for PO parity/FQ,
    // not as a second DSP or a second MASTER Prepare authority.
    readonly property bool canListenMaster: !root.monitorExclusive
        && !!root.appTerminalMonitorViewModel
        && root.appTerminalMonitorViewModel.canInstall
    readonly property bool directM17ReturnAvailable: !root.monitorExclusive
        && !!root.appTerminalProductActivation
        && root.appTerminalProductActivation.directReturnAvailable
    readonly property bool m17ReturnAvailable: root.monitorExclusive
        || root.directM17ReturnAvailable
    function returnFromM17Audition() {
        // The monitor owns anonymous MASTER->DEVICE PCM. Direct M17
        // (including measured NON-CONFORMING Preview Anyway) owns a
        // separate terminal RealizationId and must never be retired via
        // the anonymous monitor's generic cancel path.
        if (root.monitorExclusive && root.appTerminalMonitorViewModel)
            return root.appTerminalMonitorViewModel.cancel()
        if (root.directM17ReturnAvailable && root.appTerminalProductActivation)
            return root.appTerminalProductActivation.returnToSource()
        return false
    }
    readonly property bool masterTransportPlaying: root.monitorExclusive
        && !!root.appTerminalMonitorViewModel
        && root.appTerminalMonitorViewModel.transportState === "PLAYING"
    function listenToPreparedMaster() {
        if (!root.canListenMaster || !root.appTerminalMonitorViewModel)
            return false
        const success = root.appTerminalMonitorViewModel.installMonitor(false)
        if (!success && root.appTerminalMonitorViewModel.errorText.indexOf(
                "MONITOR_MEMORY_NOT_APPROVED_OR_UNAVAILABLE") !== -1)
            limitedMemoryConfirmation.open()
        return success
    }
    // Frame conversion is used only for explicit MASTER preview navigation;
    // the main transport always shows time and never exposes raw frames.
    function masterFrameFromTime(text) {
        if (!root.appTerminalMonitorViewModel
                || root.appTerminalMonitorViewModel.masterRateHz <= 0)
            return -1
        const match = /^([0-9]+):([0-5][0-9])\.([0-9]{3})$/.exec(text.trim())
        if (!match) return -1
        const milliseconds = Number(match[1]) * 60000
            + Number(match[2]) * 1000 + Number(match[3])
        const frames = Math.floor(milliseconds
            * root.appTerminalMonitorViewModel.masterRateHz / 1000)
        return Number.isSafeInteger(frames)
            && frames >= 0
            && frames <= root.appTerminalMonitorViewModel.masterDurationFrames
            ? frames : -1
    }
    function formattedMasterTime(frames) {
        if (!root.appTerminalMonitorViewModel
                || root.appTerminalMonitorViewModel.masterRateHz <= 0)
            return "0:00.000"
        const ms = Math.floor(Math.max(0, frames) * 1000
            / root.appTerminalMonitorViewModel.masterRateHz)
        const minutes = Math.floor(ms / 60000)
        const seconds = Math.floor(ms / 1000) % 60
        const fraction = ms % 1000
        return minutes + ":" + String(seconds).padStart(2, "0")
            + "." + String(fraction).padStart(3, "0")
    }
    // One permanently visible SOURCE/MASTER Region strip. The underlying
    // SOURCE coordinates are never edited while a distinct MASTER PCM owns
    // the DEVICE. The displayed region uses MASTER frame coordinates.
    function masterRegionSegments(frames) {
        const hz = root.appTerminalMonitorViewModel
            ? root.appTerminalMonitorViewModel.masterRateHz : 0
        if (hz <= 0 || frames < 0) return ["00", "00", "00", "000000000"]
        const sec = Math.floor(frames / hz)
        const remainder = frames % hz
        const frac = Math.floor(remainder * 1000000000 / hz)
        return [String(Math.floor(sec / 3600)).padStart(2, "0"),
            String(Math.floor(sec / 60) % 60).padStart(2, "0"),
            String(sec % 60).padStart(2, "0"),
            String(frac).padStart(9, "0")]
    }
    function masterRegionFrameFromSegments(h, m, sec, frac) {
        const hz = root.appTerminalMonitorViewModel
            ? root.appTerminalMonitorViewModel.masterRateHz : 0
        const digits = /^[0-9]+$/
        if(hz <= 0 || !digits.test(h) || !digits.test(m)
                || !digits.test(sec) || !digits.test(frac)
                || Number(m) > 59 || Number(sec) > 59
                || String(frac).length > 9)
            return -1
        const whole = (Number(h) * 3600 + Number(m) * 60 + Number(sec)) * hz
        const fractional = Math.round(Number("0." + String(frac)) * hz)
        const frame = whole + fractional
        return Number.isSafeInteger(frame) && frame >= 0
            && frame <= root.appTerminalMonitorViewModel.masterDurationFrames
            ? frame : -1
    }
    function updateSharedMasterRegion(which, h, m, sec, frac) {
        const vm = root.appTerminalMonitorViewModel
        if (!root.monitorExclusive || !vm || !vm.monitorControllable) return
        const frame = root.masterRegionFrameFromSegments(h,m,sec,frac)
        if (frame < 0) return
        const begin = which === "start" ? frame : vm.masterRegionStartFrames
        const end = which === "end" ? frame : vm.masterRegionEndFrames
        vm.setMasterRegion(begin,end)
    }
    function nudgeSharedMasterRegion(which, direction) {
        const vm=root.appTerminalMonitorViewModel
        if (!root.monitorExclusive || !vm || !vm.monitorControllable) return
        const step=Math.max(1,Math.round(vm.masterRateHz/1000))
        const start=vm.masterRegionStartFrames
        const end=vm.masterRegionEndFrames
        vm.setMasterRegion(which==="start" ? start+direction*step : start,
            which==="end" ? end+direction*step : end)
    }
    function stopCurrentTransport() {
        if (root.monitorExclusive) {
            if (root.appTerminalMonitorViewModel
                    && root.appTerminalMonitorViewModel.monitorControllable)
                root.appTerminalMonitorViewModel.stop()
        } else if (playbackTransport.canStop) {
            playbackTransport.stop()
        }
    }
    function toggleCurrentTransport() {
        if (root.monitorExclusive) {
            if (!root.appTerminalMonitorViewModel
                    || !root.appTerminalMonitorViewModel.monitorControllable)
                return
            if (root.masterTransportPlaying)
                root.appTerminalMonitorViewModel.pause()
            else
                root.appTerminalMonitorViewModel.play()
        } else if (playbackTransport.isPlaying) {
            playbackTransport.pause()
        } else {
            playbackTransport.playOrResume()
        }
    }
    readonly property var appTerminalPostLimiterMeter: typeof terminalPostLimiterTruePeak !== "undefined"
        ? terminalPostLimiterTruePeak : null
    readonly property var appSpectrumViewModel: typeof liveSpectrumViewModel !== "undefined" ? liveSpectrumViewModel : null

    // Single, permanently allocated 24lp status bar. The selected module
    // alone supplies its validation/preview errors; inactive drafts remain
    // local to their owning ViewModel and never replace another DSP status.
    readonly property var statusDspModule: typeof dspChainAdapterModel !== "undefined"
        && dspChainAdapterModel ? dspChainAdapterModel.activeModule : null
    // The original host remains a persistent QObject and is transferred,
    // never cloned, to the first native floating window. Later Undock
    // freezes the current shared host and replaces only the MAIN dock
    // host. One native window and one QML editor own each floating instance.
    property bool editorDetached: false
    property bool dockDropCandidate: false
    property string detachedModuleInstanceId: ""
    property bool primaryDetached: false
    property string primaryDetachedInstanceId: ""
    property var primaryDetachedModule: null
    property real primaryDockReferenceWidth: 0
    property real primaryDockReferenceHeight: 0
    property var additionalFloatingSessions: ({})
    property var dockedEditorHost: null

    readonly property string selectedDspInstanceId: root.statusDspModule
        ? root.statusDspModule.instanceId : ""

    function floatingWindowFor(instanceId) {
        if (!instanceId) return null
        if (root.primaryDetached && instanceId===root.primaryDetachedInstanceId)
            return floatingModuleWindow
        const entry=root.additionalFloatingSessions[instanceId]
        return entry ? entry.window : null
    }
    function editorHostIsFloating(host) {
        if (root.primaryDetached && host===realDspWorkspace.editorHost)
            return true
        const sessions=root.additionalFloatingSessions
        for (const key in sessions) {
            if (sessions[key].host===host) return true
        }
        return false
    }
    function updateSelectedDockState() {
        const floating=!!root.floatingWindowFor(root.selectedDspInstanceId)
        root.editorDetached=floating
        root.detachedModuleInstanceId=floating ? root.selectedDspInstanceId : ""
        if (root.dockedEditorHost)
            root.dockedEditorHost.suppressActiveModule=floating
    }
    function activeDockSlot() {
        return root.useAdaptiveDspComposition
            ? compactEditorSlot : standardEditorSlot
    }
    // Legacy drag/drop entrypoints are inert by PO decision. Windows title
    // bars move natively; only explicit Dock/Undock buttons change state.
    function pointInsideEditorDock(globalX,globalY) { return false }
    function beginDockDrag(globalX,globalY) { root.dockDropCandidate=false }
    function finishDockDrag(globalX,globalY) { root.dockDropCandidate=false }
    function detachByGrip(globalX,globalY) { }

    function detachSelectedModule() {
        const module=root.statusDspModule
        if (!module || !module.instanceId || !root.dockedEditorHost) return
        const existing=root.floatingWindowFor(module.instanceId)
        if (existing) { existing.focusExisting(); return }
        // Allocate the successor dock BEFORE transferring this live editor.
        const spare=realDspWorkspace.createExtraEditorHost()
        if (!spare) return
        const host=root.dockedEditorHost
        const id=module.instanceId
        // Capture while still docked. Never derive minima from an already
        // reparented/zero-sized floating proxy.
        const dockW=Math.max(0, Math.ceil(host.width))
        const dockH=Math.max(0, Math.ceil(host.height))
        root.editorDetached=true // no two simultaneously visible proxies
        host.activeModuleOverride=module
        host.suppressActiveModule=false
        if (host===realDspWorkspace.editorHost && !root.primaryDetached) {
            root.primaryDetachedModule=module
            root.primaryDetachedInstanceId=id
            root.primaryDockReferenceWidth=dockW
            root.primaryDockReferenceHeight=dockH
            root.primaryDetached=true
        } else {
            host.detachedWindow=true
            const floatingWin=additionalFloatingWindowFactory.createObject(root,{
                editorTarget:host,
                moduleInstanceId:id,
                moduleTitle:module.displayName,
                familyAccent:module.familyAccent,
                editorKind:module.editorContentKey,
                dockReferenceWidth:dockW,
                dockReferenceHeight:dockH,
                detached:true
            })
            if (!floatingWin) {
                host.detachedWindow=false
                host.activeModuleOverride=null
                root.editorDetached=false
                spare.destroy()
                return
            }
            const next={}
            for (const key in root.additionalFloatingSessions)
                next[key]=root.additionalFloatingSessions[key]
            next[id]={window:floatingWin,host:host,module:module}
            root.additionalFloatingSessions=next
        }
        root.dockedEditorHost=spare
        root.updateSelectedDockState()
        const floating=root.floatingWindowFor(id)
        Qt.callLater(function() { if(floating)floating.focusExisting() })
    }
    function dockFloatingById(instanceId) {
        const floatingWin=root.floatingWindowFor(instanceId)
        if (!floatingWin) return
        const wasPrimary=root.primaryDetached
            && instanceId===root.primaryDetachedInstanceId
        let returningHost=null
        if (wasPrimary) {
            returningHost=realDspWorkspace.editorHost
            root.primaryDetached=false // first hide native proxy
            root.primaryDetachedInstanceId=""
            root.primaryDetachedModule=null
        } else {
            const entry=root.additionalFloatingSessions[instanceId]
            if (!entry) return
            returningHost=entry.host
            const next={}
            for (const key in root.additionalFloatingSessions)
                if (key!==instanceId) next[key]=root.additionalFloatingSessions[key]
            root.additionalFloatingSessions=next
            floatingWin.detached=false
            floatingWin.destroy()
        }
        const previousDock=root.dockedEditorHost
        root.editorDetached=true
        // The original Main host has an authored binding to
        // workspace.hostDetached (= primaryDetached). Writing false here
        // SEVERS that binding: the next Undock still draws an outward
        // arrow and its header button cannot redock. Let the binding
        // handle the primary host; only dynamic hosts need an assignment.
        if (!wasPrimary)
            returningHost.detachedWindow=false
        returningHost.activeModuleOverride=null
        returningHost.suppressActiveModule=false
        root.dockedEditorHost=returningHost
        if (dspChainAdapterModel)
            dspChainAdapterModel.selectModuleByInstanceId(instanceId)
        root.updateSelectedDockState()
        // The original shared host may be idle after an additional
        // floating instance is redocked. Suspend its module binding so
        // there is never a second active QML editor for the same instance.
        if (previousDock===realDspWorkspace.editorHost
                && previousDock!==returningHost
                && !root.primaryDetached)
            previousDock.suppressActiveModule=true
        if (previousDock && previousDock!==returningHost
                && previousDock!==realDspWorkspace.editorHost
                && !root.editorHostIsFloating(previousDock))
            previousDock.destroy()
        Qt.callLater(function() { returningHost.forceActiveFocus() })
    }
    function returnCurrentEditorToDock() {
        root.dockFloatingById(root.selectedDspInstanceId)
    }
    function activateOrFocusModule(module) {
        if (!module || !dspChainAdapterModel) return
        dspChainAdapterModel.selectModuleByInstanceId(module.instanceId)
        root.updateSelectedDockState()
        const floating=root.floatingWindowFor(module.instanceId)
        if (floating) floating.focusExisting()
        else if (root.dockedEditorHost) root.dockedEditorHost.forceActiveFocus()
    }
    onSelectedDspInstanceIdChanged: {
        // View-menu and programmatic selection must also preserve windows.
        root.updateSelectedDockState()
        const floating=root.floatingWindowFor(root.selectedDspInstanceId)
        if (floating) Qt.callLater(function() { floating.focusExisting() })
    }
    Component.onCompleted: {
        root.dockedEditorHost=realDspWorkspace.editorHost
        root.updateSelectedDockState()
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
        title: "Open Reference WAV"
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

    // M18 Product-authorized output choice. The File menu remains
    // disabled until the application owns the complete M18 gate bundle.
    // No QML surface can mint a Ticket or invoke a detached worker.
    FileDialog {
        id: m18ExportFileDialog
        objectName: "m18ExportFileDialog"
        title: "Export verified Master WAV"
        fileMode: FileDialog.SaveFile
        nameFilters: ["WAV audio (*.wav *.wave)"]
        onAccepted: {
            const model = root.appM18ExportModel
            if (!model) return
            model.beginExport(selectedFile, root.m18ExportFormatIndex)
            m18ExportProgressDialog.open()
        }
    }
    Dialog {
        id: m18ExportFormatDialog
        objectName: "m18ExportFormatDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: "Export Master · WAV format"
        modal: true
        width: Math.min(420, root.width - 48)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ColumnLayout {
            spacing: 10
            Label {
                Layout.fillWidth: true
                text: "Choose the final WAV encoding. The verified M17 Ceiling is unchanged. "
                    + "Existing files are never overwritten; use a new filename."
                color: root.textPrimary
                wrapMode: Text.Wrap
            }
            ComboBox {
                id: m18FormatChoice
                objectName: "m18ExportEncoding"
                Layout.fillWidth: true
                model: ["24-bit PCM · TPDF dither", "32-bit Float · no dither"]
                currentIndex: root.m18ExportFormatIndex
                onActivated: {
                    if (root.appM18ProjectSession)
                        root.appM18ProjectSession.chooseM18WavEncoding(currentIndex)
                }
            }
        }
        onAccepted: {
            if (root.appM18ExportModel && root.appM18ExportModel.canExport)
                m18ExportFileDialog.open()
        }
    }

    // Modeless by design: encoding and readback run on the detached M18
    // worker; Qt retains normal responsiveness and the Product Single
    // Writer gate prevents stale Source/Project edits during final commit.
    Dialog {
        id: m18ExportProgressDialog
        objectName: "m18ExportProgressDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: "Master WAV delivery"
        modal: false
        closePolicy: Popup.NoAutoClose
        width: Math.min(480, root.width - 48)
        standardButtons: Dialog.NoButton
        contentItem: ColumnLayout {
            spacing: 10
            Label {
                Layout.fillWidth: true
                color: root.textPrimary
                font.bold: true
                text: root.appM18ExportModel
                    ? root.appM18ExportModel.statusText : "UNAVAILABLE"
            }
            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 1
                value: root.appM18ExportModel
                    ? root.appM18ExportModel.encodedProgress : 0
            }
            Label {
                Layout.fillWidth: true
                color: root.textSecondary
                wrapMode: Text.Wrap
                text: "Encoded progress is not delivery confirmation. "
                    + "The WAV is delivered only after filesystem readback."
            }
            Label {
                Layout.fillWidth: true
                color: root.error
                visible: root.appM18ExportModel
                    && root.appM18ExportModel.errorText.length > 0
                text: root.appM18ExportModel
                    ? root.appM18ExportModel.errorText : ""
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                color: root.textSecondary
                visible: root.appM18ExportModel
                    && root.appM18ExportModel.verifiedMicroTrimApplied
                text: "User-approved WAV-only micro trim: -0.01 dB. "
                    + "Original verified M17 Master was not changed."
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                color: root.textSecondary
                visible: root.appM18ExportModel
                    && root.appM18ExportModel.verifiedSha256.length === 64
                text: root.appM18ExportModel
                    ? "Verified delivered file: "
                        + root.appM18ExportModel.verifiedBytes
                        + " bytes (post-delivery readback)" : ""
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                color: root.textSecondary
                visible: root.appM18ExportModel
                    && root.appM18ExportModel.verifiedSha256.length === 64
                text: root.appM18ExportModel
                    ? "Verified WAV SHA-256: "
                        + root.appM18ExportModel.verifiedSha256 : ""
                wrapMode: Text.WrapAnywhere
            }
            Button {
                objectName: "m18ApplyMicroTrimExport"
                readonly property var model: root.appM18ExportModel
                Layout.fillWidth: true
                text: "Apply Micro Trim & Export"
                visible: !!model && model.canApplyMicroTrim
                enabled: visible
                onClicked: {
                    // Separate user consent; no automatic audio modification.
                    if (model) model.applyMicroTrimAndExport()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Button {
                    objectName: "m18CancelExport"
                    text: "Cancel Export"
                    enabled: !!root.appM18ExportModel
                        && root.appM18ExportModel.canCancel
                    onClicked: root.appM18ExportModel.cancelExport()
                }
                Item { Layout.fillWidth: true }
                Button {
                    objectName: "m18AcknowledgeExport"
                    readonly property var model: root.appM18ExportModel
                    text: model && model.canAcknowledge
                        ? "Acknowledge result" : "Close"
                    enabled: !!model
                        && (model.canAcknowledge
                            || (model.statusText !== "EXPORTING / VERIFYING"
                                && model.statusText !== "CANCELLING"))
                    onClicked: {
                        if (model && model.canAcknowledge
                            && !model.acknowledgeExport()) return
                        m18ExportProgressDialog.close()
                    }
                }
            }
        }
    }

    // X/Alt+F4 is NOT permission to retry an absent Windows DEVICE.
    // Keep the in-RAM M17 MASTER or explicitly discard on shutdown.
    Dialog {
        id: m17RecoveryExitDialog
        objectName: "m17RecoveryExitConfirmation"
        modal: true
        focus: true
        title: "Audio device unavailable"
        anchors.centerIn: parent
        width: Math.min(500, root.width - 36)
        closePolicy: Popup.CloseOnEscape
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Windows could not verify a safe return from Master "
                    + "listening to the Source device. Your prepared Master "
                    + "remains in memory. Keep working and retry Return to "
                    + "Source, or explicitly exit and lose the unsaved Master."
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    objectName: "m17KeepRecoverySessionButton"
                    text: "Keep working"
                    onClicked: m17RecoveryExitDialog.close()
                }
                Item { Layout.fillWidth: true }
                Button {
                    objectName: "m17DiscardRecoverySessionButton"
                    text: "Discard unsaved Master and exit"
                    onClicked: {
                        root.m17ConfirmedDiscardExit = true
                        m17RecoveryExitDialog.close()
                        root.close()
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "F10"
        context: Qt.ApplicationShortcut
        onActivated: desktopMenu.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    onClosing: function(close) {
        // An async WAV candidate must never outlive a normal Product exit.
        // Cancel is nonblocking; keep the UI alive until the worker is
        // terminal and its transactional writer has finished rollback.
        // This takes precedence over the M17 discard-recovery exit.
        if (root.appM18ExportModel && root.appM18ExportModel.exportInProgress) {
            close.accepted = false
            root.appM18ExportModel.cancelExport()
            if (!m18ExportProgressDialog.opened)
                m18ExportProgressDialog.open()
            return
        }
        if (root.m17ConfirmedDiscardExit) {
            close.accepted = true
            Qt.quit()
            return
        }
        if (root.m17RecoveryExitRequired) {
            close.accepted = false
            if (!m17RecoveryExitDialog.opened)
                m17RecoveryExitDialog.open()
            return
        }
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
                clip: true // timestamps stay inside the Source waveform panel
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 1
                height: 20; color: "#0A151D"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                    Label { text: "SOURCE WAVEFORM"; color: root.textSecondary; font.pixelSize: 9; font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }
                    Label {
                        objectName: "sourceWaveformVisibleClock"
                        visible: sourceWaveform.canNavigate
                        Layout.maximumWidth: Math.max(0, waveformRuler.width - 130)
                        elide: Text.ElideRight
                        text: sourceWaveform.viewportStartText.slice(0,8)
                            + " · " + sourceWaveform.viewportEndText.slice(0,8)
                        color: "#647682"; font.family: "Consolas"; font.pixelSize: 9
                        HoverHandler { id: waveformClockHover }
                        ToolTip.text: sourceWaveform.viewportStartText
                            + " — " + sourceWaveform.viewportEndText
                        ToolTip.visible: (waveformClockHover.hovered && waveformClockDwell.ready)
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: waveformClockDwell
                            anchors.fill: parent
                        }
                    }
                }
            }
            WaveformItem {
                id: waveformOverview
                objectName: "sourceWaveformOverview"
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: waveformRuler.bottom; anchors.bottom: parent.bottom
                anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 6; anchors.bottomMargin: 6
                presentation: sourceWaveform
                positionFrames: root.monitorExclusive
                    && root.appTerminalMonitorViewModel
                    && root.appTerminalMonitorViewModel.masterDurationFrames > 0
                    && sourceWaveform.sourceFrameCount > 0
                    ? Math.min(sourceWaveform.sourceFrameCount,
                        Math.floor(root.appTerminalMonitorViewModel.masterPositionFrames
                            * sourceWaveform.sourceFrameCount
                            / root.appTerminalMonitorViewModel.masterDurationFrames))
                    : auditionSelector.sourcePlayheadVisible
                        ? playbackTransport.positionFrames : 0
                durationFrames: root.monitorExclusive
                    ? sourceWaveform.sourceFrameCount : playbackTransport.durationFrames
                waveformColor: root.accent
                zeroLineColor: "#29414F"
                playheadColor: root.monitorExclusive || auditionSelector.sourcePlayheadVisible
                    ? root.warning : "transparent"
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
            // MASTER sees the same waveform timeline, but click-to-seek
            // addresses only the anonymous DEVICE monitor's MASTER frames.
            // No SOURCE region drag or SOURCE sink seek is permitted.
            MouseArea {
                objectName: "masterWaveformSeek"
                anchors.fill: waveformOverview
                visible: root.monitorExclusive && waveformOverview.visible
                    && !!root.appTerminalMonitorViewModel
                    && root.appTerminalMonitorViewModel.monitorControllable
                enabled: visible
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onClicked: function(mouse) {
                    const vm=root.appTerminalMonitorViewModel
                    const total=sourceWaveform.sourceFrameCount
                    if (!vm || total <= 0 || vm.masterDurationFrames <= 0
                        || width <= 0) return
                    const sourceFrame=sourceWaveform.sourceFrameAtVisibleFraction(
                        mouse.x / width)
                    if (sourceFrame < 0) return
                    const masterFrame=Math.min(vm.masterDurationFrames,
                        Math.floor(sourceFrame * vm.masterDurationFrames / total))
                    vm.seekMaster(masterFrame)
                }
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
                // Master-specific commands are in True Peak Limiter.
                // The ordinary transport retains only shared Play/Pause/Stop.
                Item { objectName: "controlStripElasticCenter"; Layout.fillWidth: true }
                TransportButton {
                    id: stopButton
                    objectName: "stopButton"
                    compact: false
                    iconKind: "stop"
                    enabled: root.monitorExclusive
                        ? !!root.appTerminalMonitorViewModel
                            && root.appTerminalMonitorViewModel.monitorControllable
                        : playbackTransport.canStop
                    KeyNavigation.backtab: auditionTargetSelector.lastTarget
                    KeyNavigation.tab: playPauseButton
                    onClicked: root.stopCurrentTransport()
                    Accessible.name: root.monitorExclusive ? "Stop Master" : "Stop"
                }
                Item { Layout.preferredWidth: 8 }
                TransportButton {
                    id: playPauseButton
                    objectName: "playPauseButton"
                    compact: false
                    primary: true
                    iconKind: (root.monitorExclusive
                        ? root.masterTransportPlaying : playbackTransport.isPlaying)
                        ? "pause" : "play"
                    enabled: root.monitorExclusive
                        ? !!root.appTerminalMonitorViewModel
                            && root.appTerminalMonitorViewModel.monitorControllable
                        : (playbackTransport.canPlay || playbackTransport.canPause)
                    KeyNavigation.backtab: stopButton
                    KeyNavigation.tab: auditionStartEditor.firstField
                    onClicked: root.toggleCurrentTransport()
                    Accessible.name: root.monitorExclusive
                        ? (root.masterTransportPlaying ? "Pause Master" : "Play Master")
                        : (playbackTransport.isPlaying ? "Pause" : "Play")
                }
                Item { Layout.preferredWidth: 8 }
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
                            Label { objectName: "playbackTimeLabel"; text: root.monitorExclusive
                                    && root.appTerminalMonitorViewModel
                                ? root.formattedMasterTime(root.appTerminalMonitorViewModel.masterPositionFrames)
                                : playbackTransport.positionLabel
                                color: root.textPrimary; font.family: "Consolas"; font.pixelSize: 20
                                elide: Text.ElideRight }
                        }
                        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; Layout.topMargin: 4; Layout.bottomMargin: 4; color: root.border }
                        ColumnLayout { spacing: 1
                            Label { text: "TOTAL DURATION"; color: root.textSecondary; font.pixelSize: 8; font.weight: Font.DemiBold }
                            Label { text: root.monitorExclusive
                                    && root.appTerminalMonitorViewModel
                                ? root.formattedMasterTime(root.appTerminalMonitorViewModel.masterDurationFrames)
                                : playbackTransport.durationLabel
                                color: root.textPrimary; font.family: "Consolas"; font.pixelSize: 20
                                elide: Text.ElideRight }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: realRegionPanel
            enabled: true
            objectName: "auditionRegionControls"
            color: "transparent"
            border.color: root.border
            radius: 5
            RowLayout {
                objectName: "auditionRegionContent"
                visible: true
                enabled: true
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
                        id: auditionStartEditor; objectName: "auditionRegionStartEditor"; objectPrefix: "auditionRegionStart"; endpointName: root.monitorExclusive ? "Master Region start" : "Audition Region start"
                        enabled: !root.monitorExclusive
                            && auditionRegion.controlsEnabled && auditionRegion.hasRegion
                        readonly property var masterParts: root.masterRegionSegments(root.appTerminalMonitorViewModel ? root.appTerminalMonitorViewModel.masterRegionStartFrames : 0)
                        hoursText: root.monitorExclusive ? masterParts[0] : auditionRegion.startHours
                        minutesText: root.monitorExclusive ? masterParts[1] : auditionRegion.startMinutes
                        secondsText: root.monitorExclusive ? masterParts[2] : auditionRegion.startSeconds
                        fractionText: root.monitorExclusive ? masterParts[3] : auditionRegion.startFraction
                        previousTabItem: playPauseButton; nextTabItem: auditionEndEditor.firstField
                        onCommitRequested: function(hours, minutes, seconds, fraction) {
                            if(root.monitorExclusive)root.updateSharedMasterRegion("start",hours,minutes,seconds,fraction)
                            else auditionRegion.commitStartSegments(hours,minutes,seconds,fraction)
                        }
                        onEscapeRequested: if(!root.monitorExclusive)auditionRegion.clearError()
                        onNudgeBackwardRequested: if(root.monitorExclusive)root.nudgeSharedMasterRegion("start",-1); else auditionRegion.nudgeStartBackward()
                        onNudgeForwardRequested: if(root.monitorExclusive)root.nudgeSharedMasterRegion("start",1); else auditionRegion.nudgeStartForward()
                    }
                }
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 28; color: root.border }
                ColumnLayout {
                    spacing: 1
                    Label { text: "END"; color: root.textSecondary; font.pixelSize: 9; font.weight: Font.DemiBold }
                    SegmentedTimeEditor {
                        id: auditionEndEditor; objectName: "auditionRegionEndEditor"; objectPrefix: "auditionRegionEnd"; endpointName: root.monitorExclusive ? "Master Region end exclusive" : "Audition Region end exclusive"
                        enabled: !root.monitorExclusive
                            && auditionRegion.controlsEnabled && auditionRegion.hasRegion
                        readonly property var masterParts: root.masterRegionSegments(root.appTerminalMonitorViewModel ? root.appTerminalMonitorViewModel.masterRegionEndFrames : 0)
                        hoursText: root.monitorExclusive ? masterParts[0] : auditionRegion.endHours
                        minutesText: root.monitorExclusive ? masterParts[1] : auditionRegion.endMinutes
                        secondsText: root.monitorExclusive ? masterParts[2] : auditionRegion.endSeconds
                        fractionText: root.monitorExclusive ? masterParts[3] : auditionRegion.endFraction
                        previousTabItem: auditionStartEditor.lastField; nextTabItem: fitRegionButton
                        onCommitRequested: function(hours, minutes, seconds, fraction) {
                            if(root.monitorExclusive)root.updateSharedMasterRegion("end",hours,minutes,seconds,fraction)
                            else auditionRegion.commitEndSegments(hours,minutes,seconds,fraction)
                        }
                        onEscapeRequested: if(!root.monitorExclusive)auditionRegion.clearError()
                        onNudgeBackwardRequested: if(root.monitorExclusive)root.nudgeSharedMasterRegion("end",-1); else auditionRegion.nudgeEndBackward()
                        onNudgeForwardRequested: if(root.monitorExclusive)root.nudgeSharedMasterRegion("end",1); else auditionRegion.nudgeEndForward()
                    }
                }
                Item { Layout.fillWidth: true }
                StudioButton { id: fitRegionButton; objectName: "waveformFitRegionButton"; text: "Fit Region"; iconKind: "fit-region"; minimumControlWidth: 80; enabled: !root.monitorExclusive && auditionRegion.controlsEnabled && auditionRegion.hasRegion; KeyNavigation.backtab: auditionEndEditor.lastField; KeyNavigation.tab: loopRegionCheckBox; onClicked: sourceWaveform.fitRegion(); Accessible.name: "Fit Audition Region in waveform" }
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: root.border }
                StudioToggle {
                    id: loopRegionCheckBox; objectName: "auditionRegionLoopCheckBox"
                    text: "Loop Region"
                    checked: !root.monitorExclusive && auditionRegion.loopEnabled
                    enabled: !root.monitorExclusive
                        && (auditionRegion.canLoop || auditionRegion.loopEnabled)
                    KeyNavigation.backtab: fitRegionButton
                    KeyNavigation.tab: clearRegionButton
                    onClicked: {
                        if (!root.monitorExclusive)
                            auditionRegion.requestLoopEnabled(checked)
                    }
                    Accessible.name: root.monitorExclusive ? "Loop Master Region" : "Loop Audition Region"
                }
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: root.border }
                StudioButton {
                    id: clearRegionButton; objectName: "auditionRegionClearButton"
                    text: "Clear Region"; iconKind: "clear-region"; minimumControlWidth: 80
                    enabled: !root.monitorExclusive
                        && auditionRegion.controlsEnabled && auditionRegion.hasRegion
                    KeyNavigation.backtab: loopRegionCheckBox
                    KeyNavigation.tab: sourceOpenButton
                    onClicked: {
                        if(!root.monitorExclusive)auditionRegion.requestClearRegion()
                    }
                    Accessible.name: root.monitorExclusive ? "Clear Master Region" : "Clear Audition Region"
                }
            }
            // The SAME 72px Region editor always remains visible. Under
            // monitorExclusive it edits only the independent MASTER
            // coordinates; the SOURCE region and waveform are untouched.
        }

        DspWorkspace {
            id: realDspWorkspace
            objectName: "dspWorkspace"
            adapterModel: typeof dspChainAdapterModel !== "undefined" ? dspChainAdapterModel : null
            moduleActivationHandler: function(module) { root.activateOrFocusModule(module) }
            tearOffHandler: function(globalX,globalY) {
                // Legacy gesture is no longer an active UI path.
            }
            hostDetached: root.primaryDetached
            dockToggleHandler: function(host) {
                if (host && host.detachedWindow && host.activeModuleOverride)
                    root.dockFloatingById(host.activeModuleOverride.instanceId)
                else
                    root.detachSelectedModule()
            }
            gainViewModel: root.appGainViewModel
            eqViewModel: root.appEqViewModel
            spectrumViewModel: root.appSpectrumViewModel
            analysisStatusViewModel: typeof m21ProductStatusViewModel !== "undefined"
                ? m21ProductStatusViewModel : null
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
            terminalProductActivation: root.appTerminalProductActivation
            masterListeningHandler: function() { return root.listenToPreparedMaster() }
            masterRateState: root.appMasterRateState
            memoryAdvisor: root.appMemoryAdvisor
            limiterOfflineGrProvider: typeof terminalPrivateGrViewModel !== "undefined"
                ? terminalPrivateGrViewModel : null
            m17GrTimelineFraction: root.m17GrTimelineFraction
            m17GrTimelineScope: root.m17GrTimelineScope
        }
    }

    // The FIRST floating module retains the exact original Main host.
    // Subsequent native windows are created only for other instance IDs.
    DspFloatingModuleWindow {
        id: floatingModuleWindow
        editorTarget: realDspWorkspace.editorHost
        moduleInstanceId: root.primaryDetachedInstanceId
        editorKind: root.primaryDetachedModule
            ? root.primaryDetachedModule.editorContentKey : ""
        dockReferenceWidth: root.primaryDockReferenceWidth
        dockReferenceHeight: root.primaryDockReferenceHeight
        moduleTitle: root.primaryDetachedModule
            ? root.primaryDetachedModule.displayName : "Module"
        familyAccent: root.primaryDetachedModule
            ? root.primaryDetachedModule.familyAccent : "#718092"
        detached: root.primaryDetached
        onRedockRequested: function(id) { root.dockFloatingById(id) }
    }
    Component {
        id: additionalFloatingWindowFactory
        DspFloatingModuleWindow {
            onRedockRequested: function(id) { root.dockFloatingById(id) }
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
                // Compact horizontal brand lockup, derived from the approved
                // large RGS badge (without its decorative cyan background waves).
                Item {
                    objectName: "headerBrandIconFrame"
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    Image {
                        id: headerAppIcon
                        objectName: "headerAppIcon"
                        anchors.fill: parent
                        // Keep the existing high-DPI native icon resources.
                        source: Screen.devicePixelRatio >= 2
                            ? "qrc:/qt/qml/Rgsml/Ui/resources/RGS_MasterLab_96x96.png"
                            : Screen.devicePixelRatio >= 1.25
                                ? "qrc:/qt/qml/Rgsml/Ui/resources/RGS_MasterLab_64x64.png"
                                : "qrc:/qt/qml/Rgsml/Ui/resources/RGS_MasterLab_40x40.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }
                    Rectangle {
                        objectName: "headerBrandIconBorder"
                        anchors.fill: parent
                        radius: 9
                        color: "transparent"
                        border.width: 1
                        border.color: "#30CFEA"
                        opacity: 0.8
                    }
                }
                Column {
                    id: headerBrandLockup
                    objectName: "headerBrandLockup"
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: Math.max(brandTitle.implicitWidth,
                                                    brandSubtitle.implicitWidth)
                    spacing: 1
                    Label {
                        id: brandTitle
                        text: "RGS MasterLab"
                        color: root.textPrimary
                        font.family: "Segoe UI"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                    }
                    Rectangle {
                        objectName: "headerBrandGoldDivider"
                        width: headerBrandLockup.width
                        height: 2
                        radius: 1
                        color: "#F0B73B"
                    }
                    Label {
                        id: brandSubtitle
                        text: "Reference Guided Sound"
                        color: "#A8D7F4"
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                    }
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
                        StudioMenuItem { objectName: "menuOpenGold"; text: "Open Reference"; enabled: !root.monitorExclusive; onTriggered: goldDialog.open() }
                        StudioMenuItem { objectName: "menuClearGold"; text: "Clear Reference"; enabled: !root.monitorExclusive && goldSelection.hasGold; onTriggered: goldSelection.clearGold() }
                        MenuSeparator { }
                        StudioMenuItem { objectName: "menuSaveProjectAs"; text: "Save Project As…"; enabled: projectSession.canSaveProject; onTriggered: projectSaveDialog.open() }
                        StudioMenuItem {
                            objectName: "menuExportMaster"
                            text: "Export verified Master…"
                            enabled: !!root.appM18ExportModel
                                && root.appM18ExportModel.canExport
                            onTriggered: m18ExportFormatDialog.open()
                        }
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
                        MenuSeparator { }
                        StudioMenuItem {
                            objectName: "menuQ0ResourcesIncomplete"
                            // All 16 immutable FIR files must be deployed
                            // beside the EXE. Missing/corrupt assets require
                            // repair of the installation, not user-selected
                            // folders or accidental developer overrides.
                            visible: root.appPrivateFirLibrary
                                && !root.appPrivateFirLibrary.originalQ0Ready
                            text: "Q0 FIR resources missing — reinstall app"
                            enabled: false
                        }
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
                        StudioMenuItem {
                            text: root.monitorExclusive
                                ? (root.masterTransportPlaying ? "Pause Master" : "Play Master")
                                : (playbackTransport.isPlaying ? "Pause\tSpace" : "Play\tSpace")
                            enabled: playPauseButton.enabled
                            onTriggered: root.toggleCurrentTransport()
                        }
                        StudioMenuItem { text: root.monitorExclusive ? "Stop Master" : "Stop"
                            enabled: stopButton.enabled; onTriggered: root.stopCurrentTransport() }
                        StudioMenuItem { text: "Listen to Master"
                            enabled: root.canListenMaster
                            onTriggered: root.listenToPreparedMaster() }
                        StudioMenuItem { text: "Return to Source"
                            enabled: root.m17ReturnAvailable
                            onTriggered: root.returnFromM17Audition() }
                        MenuSeparator { }
                        StudioMenuItem {
                            text: !root.monitorExclusive && auditionRegion.loopEnabled
                                ? "Disable Loop Region" : "Enable Loop Region"
                            enabled: !root.monitorExclusive
                                && (auditionRegion.canLoop || auditionRegion.loopEnabled)
                            onTriggered: {
                                if (!root.monitorExclusive)
                                    auditionRegion.requestLoopEnabled(!auditionRegion.loopEnabled)
                            }
                        }
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
                        target: root.dockedEditorHost
                    }
                    // Placeholder occupies exactly the same geometry with
                    // or without a detached editor: NO unrelated reflow.
                    Rectangle {
                        objectName: "standardEditorDockDropZone"
                        anchors.fill: parent
                        visible: root.editorDetached
                        radius: 6
                        color: "#0B1622"
                        border.width: 1
                        border.color: "#27465F"
                        Column {
                            anchors.centerIn: parent
                            spacing: 10
                            StudioIcon {
                                objectName: "centerDockIcon"
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 64
                                height: 64
                                kind: "dock-large"
                                strokeColor: "#6CBCE2"
                                strokeWidth: 1.5
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Dock"
                                font.family: "Segoe UI"
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                color: "#BFEAF6"
                            }
                        }
                        MouseArea {
                            objectName: "centralDockButton"
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.returnCurrentEditorToDock()
                            Accessible.name: "Dock this floating module"
                            StudioTooltipDwell {
                                id: standardCenterDockTooltipDwell
                                anchors.fill: parent
                            }
                            ToolTip.text: "Dock floating module"
                            ToolTip.visible: standardCenterDockTooltipDwell.ready
                            ToolTip.delay: 0
                        }
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
                                            text: "Reference / AutoMatch"
                                            selected: adaptiveContextWorkspace.activeContextMode === "GOLD_AUTOMATCH"
                                            minimumControlWidth: 166
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
                                                        text: goldSelection.hasGold ? ("REFERENCE  " + goldSelection.displayName) : "NO REFERENCE LOADED"
                                                        color: goldSelection.hasGold ? root.textPrimary : "#586D7C"
                                                        font.family: "Segoe UI"
                                                        font.pixelSize: 10
                                                        font.weight: Font.DemiBold
                                                        elide: Text.ElideNone
                                                        wrapMode: Text.WrapAnywhere
                                                        Layout.fillHeight: true
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
                                                if (visible && root.appMemoryAdvisor)
                                                    root.appMemoryAdvisor.refresh()
                                            }
                                            Connections {
                                                target: sourceSelection
                                                function onSourceChanged() {
                                                    if (restorationControlsGroup.visible
                                                            && prepareRestorationView.visible
                                                            && root.appMemoryAdvisor)
                                                        root.appMemoryAdvisor.refresh()
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
                                                        text: root.appMemoryAdvisor ? root.appMemoryAdvisor.statusText
                                                            : "UNAVAILABLE"
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
                                                        enabled: !!root.appMemoryAdvisor
                                                        onClicked: if (root.appMemoryAdvisor)
                                                            root.appMemoryAdvisor.refresh()
                                                        StudioTooltipDwell {
                                                            id: prepareRefreshDwell
                                                            anchors.fill: parent
                                                        }
                                                        ToolTip.text: "Refresh available RAM and audio device information"
                                                        ToolTip.visible: prepareRefreshDwell.ready
                                                        ToolTip.delay: 0
                                                        Accessible.name: "Refresh Prepare memory guidance"
                                                    }
                                                }
                                                GridLayout {
                                                    Layout.fillWidth: true
                                                    Layout.fillHeight: true
                                                    columns: 2
                                                    columnSpacing: 4
                                                    rowSpacing: 2
                                                    Repeater {
                                                        model: root.appMemoryAdvisor
                                                            ? root.appMemoryAdvisor.recommendations : []
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
                                                                : modelData.kind === "blocked" ? "#B35A64"
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
                                                                        : modelData.kind === "blocked" ? "#FFB5BC"
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
                                                    text: "Minimum PCM estimate · CPU not qualified · refresh before allocation"
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
                        target: root.dockedEditorHost
                    }
                    // Placeholder occupies exactly the same geometry with
                    // or without a detached editor: NO unrelated reflow.
                    Rectangle {
                        objectName: "compactEditorDockDropZone"
                        anchors.fill: parent
                        visible: root.editorDetached
                        radius: 6
                        color: "#0B1622"
                        border.width: 1
                        border.color: "#27465F"
                        Column {
                            anchors.centerIn: parent
                            spacing: 10
                            StudioIcon {
                                objectName: "centerDockIcon"
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 64
                                height: 64
                                kind: "dock-large"
                                strokeColor: "#6CBCE2"
                                strokeWidth: 1.5
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Dock"
                                font.family: "Segoe UI"
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                color: "#BFEAF6"
                            }
                        }
                        MouseArea {
                            objectName: "centralDockButton"
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.returnCurrentEditorToDock()
                            Accessible.name: "Dock this floating module"
                            StudioTooltipDwell {
                                id: compactCenterDockTooltipDwell
                                anchors.fill: parent
                            }
                            ToolTip.text: "Dock floating module"
                            ToolTip.visible: compactCenterDockTooltipDwell.ready
                            ToolTip.delay: 0
                        }
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

    Dialog {
        id: limitedMemoryConfirmation
        objectName: "transportMasterMemoryConfirmation"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: "Master listening · memory headroom"
        width: Math.min(460, root.width - 48)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label {
            padding: 12
            color: root.textPrimary
            wrapMode: Text.Wrap
            text: "Device-rate Master listening requires additional RAM. "
                + "Retry only if the memory policy permits limited headroom? "
                + "Unknown or insufficient memory remains blocked."
        }
        onAccepted: {
            if (root.canListenMaster && root.appTerminalMonitorViewModel)
                root.appTerminalMonitorViewModel.installMonitor(true)
        }
    }
    Label { objectName: "playbackErrorMessage"; visible: false; text: playbackTransport.errorMessage }
    Label { objectName: "goldErrorLabel"; visible: false; text: goldSelection.errorMessage }
    Label { objectName: "auditionRoutingStatus"; visible: false; text: auditionSelector.statusText }
    Label { objectName: "goldStateLabel"; visible: false; text: goldSelection.hasGold ? "Reference: " + goldSelection.displayName : "Reference: not loaded" }
    Label { objectName: "sourceEmptyState"; visible: !sourceSelection.hasSource; opacity: 0; width: 0; height: 0; text: "No Source selected" }
    Label { objectName: "sourceReadOnlyBadge"; visible: sourceSelection.hasSource; opacity: 0; width: 0; height: 0; text: "Read-only" }
    Label { objectName: "sourceErrorMessage"; visible: sourceSelection.errorMessage.length > 0; opacity: 0; width: 0; height: 0; text: sourceSelection.errorMessage }
    Label { objectName: "sourceContainerMetadata"; visible: false; text: "Container  " + sourceSelection.containerLabel }
    Label { objectName: "sourceFramesMetadata"; visible: false; text: "Frames  " + sourceSelection.frameCount }
    Label { objectName: "playbackStateLabel"; visible: false; text: playbackTransport.stateLabel }
}

