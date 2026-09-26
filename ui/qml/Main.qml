import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

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
    flags: Qt.Window | Qt.FramelessWindowHint

    readonly property color surface: "#0F1820"
    readonly property color surfaceRaised: "#13222F"
    readonly property color panel: "#081218"
    readonly property color border: "#2A3947"
    readonly property color textPrimary: "#E6EEF0"
    readonly property color textSecondary: "#8A97A3"
    readonly property color accent: "#00C8FF"
    readonly property color warning: "#F2B632"
    readonly property color error: "#F27683"

    property rect normalGeometry: Qt.rect(100, 100, 1440, 900)
    property bool isChangingWindowState: false

    Timer {
        id: normalGeometryCaptureTimer
        interval: 150
        repeat: false
        onTriggered: {
            if (!root.isChangingWindowState && root.visibility === Window.Windowed && root.windowState === Qt.WindowNoState) {
                root.normalGeometry = Qt.rect(root.x, root.y, root.width, root.height)
            }
        }
    }

    function captureNormalGeometry() {
        if (!isChangingWindowState && root.visibility === Window.Windowed && root.windowState === Qt.WindowNoState) {
            normalGeometryCaptureTimer.restart()
        }
    }

    function toggleMaximizeRestore() {
        if (root.visibility === Window.Maximized || root.windowState === Qt.WindowMaximized) {
            restoreNormalWindow()
        } else {
            if (root.visibility === Window.Windowed && root.windowState === Qt.WindowNoState) {
                normalGeometry = Qt.rect(root.x, root.y, root.width, root.height)
            }
            isChangingWindowState = true
            root.showMaximized()
            stateResetTimer.restart()
        }
    }

    function restoreNormalWindow() {
        isChangingWindowState = true
        root.showNormal()
        restoreTimer.restart()
        stateResetTimer.restart()
    }

    Timer {
        id: stateResetTimer
        interval: 300
        repeat: false
        onTriggered: isChangingWindowState = false
    }

    Timer {
        id: restoreTimer
        interval: 10
        repeat: false
        onTriggered: {
            if (root.visibility !== Window.Maximized && root.windowState !== Qt.WindowMaximized) {
                if (root.normalGeometry.width > 0 && root.normalGeometry.height > 0) {
                    root.width = Math.max(root.minimumWidth, root.normalGeometry.width)
                    root.height = Math.max(root.minimumHeight, root.normalGeometry.height)
                    if (root.normalGeometry.x >= 0 && root.normalGeometry.y >= 0) {
                        root.x = root.normalGeometry.x
                        root.y = root.normalGeometry.y
                    }
                }
            }
        }
    }

    onXChanged: captureNormalGeometry()
    onYChanged: captureNormalGeometry()
    onWidthChanged: captureNormalGeometry()
    onHeightChanged: captureNormalGeometry()

    readonly property bool hasError: {
        return projectSession.errorMessage.length > 0
            || sourceSelection.errorMessage.length > 0
            || goldSelection.errorMessage.length > 0
            || auditionSelector.statusText.length > 0
            || playbackTransport.errorMessage.length > 0
            || auditionRegion.errorMessage.length > 0
    }

    property string statusText: {
        if (projectSession.errorMessage.length > 0) return projectSession.errorMessage
        if (sourceSelection.errorMessage.length > 0) return sourceSelection.errorMessage
        if (goldSelection.errorMessage.length > 0) return goldSelection.errorMessage
        if (auditionSelector.statusText.length > 0) return auditionSelector.statusText
        if (playbackTransport.errorMessage.length > 0) return playbackTransport.errorMessage
        if (auditionRegion.errorMessage.length > 0) return auditionRegion.errorMessage
        if (projectSession.degraded) return projectSession.statusText
        if (sourceWaveform.state === "BUILDING") return "Analyzing Source waveform"
        if (!sourceSelection.hasSource) return "Ready | Open a Source WAV"
        return "Ready | " + sourceSelection.sampleRateHz + " Hz | " + sourceSelection.sampleFormatLabel + " | " + sourceSelection.channelLayoutLabel
    }

    component StudioMenuItem: MenuItem {
        id: menuItem
        implicitWidth: 230
        implicitHeight: 30
        contentItem: Text { leftPadding: 10; rightPadding: 10; text: menuItem.text; color: menuItem.enabled ? root.textPrimary : "#586773"; font.family: "Segoe UI"; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: menuItem.highlighted && menuItem.enabled ? "#173043" : "transparent" }
    }

    FileDialog { id: sourceDialog; objectName: "sourceFileDialog"; title: "Open Source WAV"; fileMode: FileDialog.OpenFile; nameFilters: ["WAV audio (*.wav *.wave)"]; onAccepted: sourceSelection.selectSource(selectedFile); onRejected: sourceSelection.cancelSourceSelection() }
    FileDialog { id: goldDialog; objectName: "goldFileDialog"; title: "Open Gold Reference WAV"; fileMode: FileDialog.OpenFile; nameFilters: ["WAV audio (*.wav *.wave)"]; onAccepted: goldSelection.selectGold(selectedFile); onRejected: goldSelection.cancelGoldSelection() }
    FileDialog { id: projectOpenDialog; objectName: "projectOpenFileDialog"; title: "Open RGS MasterLab Project"; fileMode: FileDialog.OpenFile; nameFilters: ["RGS MasterLab Project (*.rgsml)"]; onAccepted: projectSession.openProject(selectedFile); onRejected: projectSession.cancelProjectOpen() }
    FileDialog { id: projectSaveDialog; objectName: "projectSaveFileDialog"; title: "Save RGS MasterLab Project As"; fileMode: FileDialog.SaveFile; nameFilters: ["RGS MasterLab Project (*.rgsml)"]; onAccepted: projectSession.saveProjectAs(selectedFile); onRejected: projectSession.cancelProjectSave() }
    ParametricEqEditorWindow { id: eqWindow; objectName: "parametricEqToolWindow"; viewModel: eqViewModel; transientParent: root }

    Shortcut {
        sequence: "F10"
        context: Qt.ApplicationShortcut
        onActivated: desktopMenu.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    onClosing: function(close) {
        if (playbackTransport.canStop) playbackTransport.stop()
        if (eqWindow) {
            eqWindow.forceClose = true
            eqWindow.close()
        }
        close.accepted = true
        Qt.quit()
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

            MouseArea {
                id: headerMouseArea
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true

                property point pressPoint: Qt.point(0, 0)
                property bool isDraggingFromMaximized: false

                onPressed: function(mouse) {
                    pressPoint = Qt.point(mouse.x, mouse.y)
                    isDraggingFromMaximized = false
                    if (root.visibility !== Window.Maximized) {
                        root.startSystemMove()
                    }
                }

                onPositionChanged: function(mouse) {
                    if (pressed && root.visibility === Window.Maximized && !isDraggingFromMaximized) {
                        const dx = mouse.x - pressPoint.x
                        const dy = mouse.y - pressPoint.y
                        if (Math.abs(dy) > 4 || Math.abs(dx) > 4) {
                            isDraggingFromMaximized = true
                            root.restoreNormalWindow()
                            root.startSystemMove()
                        }
                    }
                }

                onDoubleClicked: root.toggleMaximizeRestore()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                spacing: 12
                Rectangle {
                    Layout.preferredWidth: 40; Layout.preferredHeight: 40; radius: 4; color: "#8B111B"; border.color: "#C8CDD3"
                    Text { anchors.centerIn: parent; text: "RGS"; color: "#F3F5F7"; font.family: "Segoe UI"; font.pixelSize: 12; font.weight: Font.Bold }
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
                        StudioMenuItem { objectName: "menuOpenProject"; text: "Open Project…"; onTriggered: projectOpenDialog.open() }
                        MenuSeparator { }
                        StudioMenuItem { objectName: "menuOpenSource"; text: "Open Source\tCtrl+O"; onTriggered: sourceDialog.open() }
                        StudioMenuItem { objectName: "menuOpenGold"; text: "Open Gold Reference"; onTriggered: goldDialog.open() }
                        StudioMenuItem { objectName: "menuClearGold"; text: "Clear Gold Reference"; enabled: goldSelection.hasGold; onTriggered: goldSelection.clearGold() }
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
                        objectName: "desktopViewMenu"
                        popupType: Popup.Item
                        width: 230
                        title: "&View"
                        background: Rectangle { color: root.surface; border.color: root.border }
                        StudioMenuItem { objectName: "menuViewParametricEq"; text: "Parametric EQ…"; enabled: auditionSelector.preparedAvailable; onTriggered: { eqWindow.show(); eqWindow.raise(); eqWindow.requestActivate() } }
                        MenuSeparator { }
                        StudioMenuItem { text: "Zoom In\tCtrl++"; enabled: sourceWaveform.canNavigate; onTriggered: sourceWaveform.zoomIn() }
                        StudioMenuItem { text: "Zoom Out\tCtrl+-"; enabled: sourceWaveform.canNavigate && !sourceWaveform.fullFit; onTriggered: sourceWaveform.zoomOut() }
                        StudioMenuItem { text: "Fit Source\tHome"; enabled: sourceWaveform.canNavigate && !sourceWaveform.fullFit; onTriggered: sourceWaveform.fitSource() }
                        StudioMenuItem { text: "Fit Region"; enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion; onTriggered: sourceWaveform.fitRegion() }
                    }
                    Menu {
                        title: "&Transport"
                        background: Rectangle { color: root.surface; border.color: root.border }
                        StudioMenuItem { text: playbackTransport.isPlaying ? "Pause\tSpace" : "Play\tSpace"; enabled: playbackTransport.canPlay || playbackTransport.canPause; onTriggered: playPauseButton.clicked() }
                        StudioMenuItem { text: "Stop"; enabled: playbackTransport.canStop; onTriggered: playbackTransport.stop() }
                        MenuSeparator { }
                        StudioMenuItem { text: auditionRegion.loopEnabled ? "Disable Loop Region" : "Enable Loop Region"; enabled: auditionRegion.canLoop || auditionRegion.loopEnabled; onTriggered: auditionRegion.requestLoopEnabled(!auditionRegion.loopEnabled) }
                    }
                    Menu {
                        title: "&Help"
                        background: Rectangle { color: root.surface; border.color: root.border }
                        StudioMenuItem { text: "RGS MasterLab Help"; enabled: false }
                        StudioMenuItem { text: "About RGS MasterLab"; enabled: false }
                    }
                }

                Item { Layout.fillWidth: true }
                AuditionSourceSelector { id: auditionTargetSelector; objectName: "headerAuditionTargetSelector"; Layout.preferredWidth: implicitWidth; Layout.alignment: Qt.AlignVCenter; previousTabItem: fitSourceButton; nextTabItem: stopButton }
                Row {
                    Layout.preferredHeight: 48; spacing: 0
                    StudioIconButton { objectName: "windowMinimizeButton"; width: 46; height: 48; controlSize: 46; iconKind: "minimize"; activeFocusOnTab: false; onClicked: root.showMinimized(); Accessible.name: "Minimize window" }
                    StudioIconButton { objectName: "windowMaximizeButton"; width: 46; height: 48; controlSize: 46; iconKind: root.visibility === Window.Maximized ? "restore" : "maximize"; activeFocusOnTab: false; onClicked: root.toggleMaximizeRestore(); Accessible.name: root.visibility === Window.Maximized ? "Restore window" : "Maximize window" }
                    StudioIconButton { objectName: "windowCloseButton"; width: 48; height: 48; controlSize: 48; tone: "close"; iconKind: "close"; activeFocusOnTab: false; onClicked: root.close(); Accessible.name: "Close window" }
                }
            }
        }

        ColumnLayout {
            width: parent.width
            height: parent.height - applicationHeader.height - statusBar.height
            spacing: 8
            Item { Layout.preferredHeight: 0 }
            Rectangle {
                id: sourcePanel
                objectName: "sourceMetadataPanel"
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.preferredHeight: 72
                color: root.panel
                border.color: root.border
                radius: 5
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 14
                    anchors.bottomMargin: 14
                    spacing: 16
                    Rectangle {
                        Layout.preferredWidth: 40; Layout.preferredHeight: 40; radius: 5; color: "#0E2632"; border.color: sourceSelection.hasSource ? root.accent : root.border
                        StudioIcon { anchors.centerIn: parent; width: 24; height: 24; kind: "source"; strokeColor: sourceSelection.hasSource ? root.accent : root.textSecondary }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.fillHeight: true; spacing: 3
                        Label { id: sourceDisplayName; objectName: "sourceDisplayName"; Layout.fillWidth: true; visible: sourceSelection.hasSource; text: sourceSelection.hasSource ? sourceSelection.displayName : "No Source selected"; color: sourceSelection.hasSource ? root.textPrimary : root.textSecondary; font.family: "Segoe UI"; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 24
                            Label { objectName: "sourceFormatMetadata"; visible: sourceSelection.hasSource; text: "FORMAT  " + sourceSelection.sampleFormatLabel; color: root.textSecondary; font.pixelSize: 10 }
                            Label { objectName: "sourceRateMetadata"; visible: sourceSelection.hasSource; text: "SAMPLE RATE  " + sourceSelection.sampleRateHz + " Hz"; color: root.textSecondary; font.pixelSize: 10 }
                            Label { objectName: "sourceBitDepthMetadata"; visible: sourceSelection.hasSource; text: "BIT DEPTH  " + sourceSelection.sampleFormatLabel; color: root.textSecondary; font.pixelSize: 10 }
                            Label { objectName: "sourceChannelsMetadata"; visible: sourceSelection.hasSource; text: "CHANNELS  " + sourceSelection.channelLayoutLabel; color: root.textSecondary; font.pixelSize: 10 }
                            Label { objectName: "sourceDurationMetadata"; visible: sourceSelection.hasSource; text: "DURATION  " + sourceSelection.durationLabel; color: root.textSecondary; font.pixelSize: 10 }
                            Label { visible: !sourceSelection.hasSource; text: "Choose a WAV Source to begin"; color: "#667783"; font.pixelSize: 10 }
                            Item { Layout.fillWidth: true }
                        }
                    }
                    StudioButton {
                        id: sourceOpenButton; objectName: "sourceOpenButton"; text: sourceSelection.hasSource ? "Replace Source" : "Open Source"; iconKind: "folder-open"; tone: "primary"; Layout.preferredWidth: 110
                        KeyNavigation.backtab: clearRegionButton; KeyNavigation.tab: waveformOverview
                        onClicked: sourceDialog.open(); Accessible.name: text
                    }
                }
            }

            Rectangle {
                id: waveformPanel
                objectName: "sourceWaveformPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.minimumHeight: 265
                color: "#050C11"
                border.color: root.border
                radius: 5
                Rectangle {
                    id: waveformRuler
                    objectName: "waveformRuler"
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 1
                    height: 24; color: "#0A151D"
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
                    anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 12; anchors.bottomMargin: 12
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
                    KeyNavigation.backtab: sourceOpenButton
                    KeyNavigation.tab: zoomOutButton
                    Accessible.role: Accessible.Pane
                    Accessible.name: sourceWaveform.channelCount === 2 ? "Source waveform, separate left and right lanes" : "Source waveform, mono lane"
                    Accessible.description: "Click to seek, drag to pan, Shift drag to create an Audition Region"
                }
                Rectangle {
                    visible: waveformOverview.visible && sourceWaveform.channelCount === 2
                    anchors.left: waveformOverview.left; anchors.right: waveformOverview.right; anchors.verticalCenter: waveformOverview.verticalCenter
                    height: 16; color: "#071117"
                    Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 1; color: "#20313D" }
                }
                Label { objectName: "sourceWaveformLeftLane"; anchors.left: waveformOverview.left; anchors.leftMargin: 4; anchors.top: waveformOverview.top; anchors.topMargin: 3; visible: waveformOverview.visible && sourceWaveform.channelCount === 2; text: "L"; color: root.textSecondary; font.pixelSize: 10 }
                Label { objectName: "sourceWaveformRightLane"; anchors.left: waveformOverview.left; anchors.leftMargin: 4; anchors.bottom: waveformOverview.bottom; anchors.bottomMargin: 3; visible: waveformOverview.visible && sourceWaveform.channelCount === 2; text: "R"; color: root.textSecondary; font.pixelSize: 10 }
                Label { objectName: "sourceWaveformMonoLane"; anchors.left: waveformOverview.left; anchors.verticalCenter: waveformOverview.verticalCenter; visible: waveformOverview.visible && sourceWaveform.channelCount === 1; text: "C"; color: root.textSecondary; font.pixelSize: 10 }
                Label { id: sourceWaveformStatus; objectName: "sourceWaveformStatus"; anchors.centerIn: waveformOverview; width: waveformOverview.width - 40; visible: !sourceWaveform.ready || sourceWaveform.sourceFrameCount === 0; text: sourceWaveform.statusText; color: sourceWaveform.state === "FAILED" ? root.error : root.textSecondary; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; font.pixelSize: 12 }
                BusyIndicator { objectName: "sourceWaveformBuildingIndicator"; anchors.horizontalCenter: sourceWaveformStatus.horizontalCenter; anchors.bottom: sourceWaveformStatus.top; running: sourceWaveform.state === "BUILDING"; visible: running }
                StudioButton { objectName: "sourceWaveformRetryButton"; anchors.horizontalCenter: sourceWaveformStatus.horizontalCenter; anchors.top: sourceWaveformStatus.bottom; anchors.topMargin: 8; text: "Retry waveform analysis"; visible: sourceWaveform.state === "FAILED"; onClicked: sourceWaveform.requestRetry() }
                Label { anchors.right: waveformOverview.right; anchors.bottom: waveformOverview.bottom; visible: waveformOverview.visible && sourceWaveform.hasOverrange; text: "Source peaks exceed 1.0"; color: root.error; font.pixelSize: 10 }
            }
            Rectangle {
                id: controlStrip
                objectName: "controlStrip"
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.preferredHeight: 88
                color: root.panel
                border.color: root.border
                radius: 5
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
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
                    TransportButton { id: stopButton; objectName: "stopButton"; iconKind: "stop"; enabled: playbackTransport.canStop; KeyNavigation.backtab: auditionTargetSelector.lastTarget; KeyNavigation.tab: playPauseButton; onClicked: playbackTransport.stop(); Accessible.name: "Stop" }
                    Item { Layout.preferredWidth: 16 }
                    TransportButton { id: playPauseButton; objectName: "playPauseButton"; primary: true; iconKind: playbackTransport.isPlaying ? "pause" : "play"; enabled: playbackTransport.canPlay || playbackTransport.canPause; KeyNavigation.backtab: stopButton; KeyNavigation.tab: auditionStartEditor.firstField; onClicked: playbackTransport.isPlaying ? playbackTransport.pause() : playbackTransport.playOrResume(); Accessible.name: playbackTransport.isPlaying ? "Pause" : "Play" }
                    Item { Layout.preferredWidth: 16 }
                    Rectangle {
                        objectName: "transportTimeModule"
                        Layout.preferredWidth: 246
                        Layout.preferredHeight: 56
                        color: "#050D12"
                        border.color: root.border
                        radius: 5
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 24; anchors.rightMargin: 24; spacing: 20
                            ColumnLayout { spacing: 1
                                Label { text: "CURRENT POSITION"; color: root.textSecondary; font.pixelSize: 8; font.weight: Font.DemiBold }
                                Label { objectName: "playbackTimeLabel"; text: playbackTransport.positionLabel; color: root.textPrimary; font.family: "Consolas"; font.pixelSize: 20 }
                            }
                            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; Layout.topMargin: 9; Layout.bottomMargin: 9; color: root.border }
                            ColumnLayout { spacing: 1
                                Label { text: "TOTAL DURATION"; color: root.textSecondary; font.pixelSize: 8; font.weight: Font.DemiBold }
                                Label { text: playbackTransport.durationLabel; color: root.textPrimary; font.family: "Consolas"; font.pixelSize: 20 }
                            }
                        }
                    }
                }
            }
            Rectangle {
                id: regionPanel
                objectName: "auditionRegionControls"
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.preferredHeight: 72
                color: root.panel
                border.color: root.border
                radius: 5
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 16
                    ColumnLayout {
                        spacing: 2
                        Label { text: "START"; color: root.textSecondary; font.pixelSize: 9; font.weight: Font.DemiBold }
                        SegmentedTimeEditor {
                            id: auditionStartEditor; objectName: "auditionRegionStartEditor"; objectPrefix: "auditionRegionStart"; endpointName: "Audition Region start"; enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion
                            hoursText: auditionRegion.startHours; minutesText: auditionRegion.startMinutes; secondsText: auditionRegion.startSeconds; fractionText: auditionRegion.startFraction
                            previousTabItem: playPauseButton; nextTabItem: auditionEndEditor.firstField
                            onCommitRequested: function(hours, minutes, seconds, fraction) { auditionRegion.commitStartSegments(hours, minutes, seconds, fraction) }
                            onEscapeRequested: auditionRegion.clearError(); onNudgeBackwardRequested: auditionRegion.nudgeStartBackward(); onNudgeForwardRequested: auditionRegion.nudgeStartForward()
                        }
                    }
                    Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 36; color: root.border }
                    ColumnLayout {
                        spacing: 2
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
            Item { Layout.preferredHeight: 0 }
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
                Rectangle { objectName: "statusReadyIndicator"; Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4; color: root.hasError ? root.error : "#00E6E6" }
                Label { objectName: "statusBarMessage"; Layout.fillWidth: true; text: root.statusText; color: root.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
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

    MouseArea { z: 1000; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 5; cursorShape: Qt.SizeHorCursor; enabled: root.visibility !== Window.Maximized; onPressed: root.startSystemResize(Qt.LeftEdge) }
    MouseArea { z: 1000; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 5; cursorShape: Qt.SizeHorCursor; enabled: root.visibility !== Window.Maximized; onPressed: root.startSystemResize(Qt.RightEdge) }
    MouseArea { z: 1000; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 5; cursorShape: Qt.SizeVerCursor; enabled: root.visibility !== Window.Maximized; onPressed: root.startSystemResize(Qt.TopEdge) }
    MouseArea { z: 1000; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 5; cursorShape: Qt.SizeVerCursor; enabled: root.visibility !== Window.Maximized; onPressed: root.startSystemResize(Qt.BottomEdge) }

    MouseArea { objectName: "resizeTopLeft"; z: 1001; anchors.left: parent.left; anchors.top: parent.top; width: 8; height: 8; cursorShape: Qt.SizeFDiagCursor; enabled: root.visibility !== Window.Maximized; onPressed: root.startSystemResize(Qt.TopEdge | Qt.LeftEdge) }
    MouseArea { objectName: "resizeTopRight"; z: 1001; anchors.right: parent.right; anchors.top: parent.top; width: 8; height: 8; cursorShape: Qt.SizeBDiagCursor; enabled: root.visibility !== Window.Maximized; onPressed: root.startSystemResize(Qt.TopEdge | Qt.RightEdge) }
    MouseArea { objectName: "resizeBottomLeft"; z: 1001; anchors.left: parent.left; anchors.bottom: parent.bottom; width: 8; height: 8; cursorShape: Qt.SizeBDiagCursor; enabled: root.visibility !== Window.Maximized; onPressed: root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge) }
    MouseArea { objectName: "resizeBottomRight"; z: 1001; anchors.right: parent.right; anchors.bottom: parent.bottom; width: 8; height: 8; cursorShape: Qt.SizeFDiagCursor; enabled: root.visibility !== Window.Maximized; onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge) }
}
