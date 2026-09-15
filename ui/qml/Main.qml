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
    readonly property color accent: "#43C7F3"
    readonly property color warning: "#E6B94A"
    readonly property color error: "#F27683"

    property string statusText: {
        if (sourceSelection.errorMessage.length > 0)
            return sourceSelection.errorMessage
        if (goldSelection.errorMessage.length > 0)
            return goldSelection.errorMessage
        if (auditionSelector.statusText.length > 0)
            return auditionSelector.statusText
        if (playbackTransport.errorMessage.length > 0)
            return playbackTransport.errorMessage
        if (auditionRegion.errorMessage.length > 0)
            return auditionRegion.errorMessage
        if (sourceWaveform.state === "BUILDING")
            return "Analyzing Source waveform…"
        if (!sourceSelection.hasSource)
            return "Ready · Open a Source WAV"
        return "Ready  |  " + sourceSelection.sampleRateHz + " Hz  |  "
                + sourceSelection.sampleFormatLabel + "  |  "
                + sourceSelection.channelLayoutLabel + "  |  "
                + sourceSelection.displayName
    }

    component StudioMenuItem: MenuItem {
        id: menuItem
        implicitWidth: 230
        implicitHeight: 30
        contentItem: Text {
            leftPadding: 10
            rightPadding: 10
            text: menuItem.text
            color: menuItem.enabled ? root.textPrimary : "#586773"
            font.family: "Segoe UI"
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: menuItem.highlighted && menuItem.enabled ? "#173043" : "transparent"
        }
    }

    FileDialog {
        id: sourceDialog
        objectName: "sourceFileDialog"
        title: "Open Source WAV"
        fileMode: FileDialog.OpenFile
        nameFilters: ["WAV audio (*.wav *.wave)"]
        onAccepted: sourceSelection.selectSource(selectedFile)
        onRejected: sourceSelection.cancelSourceSelection()
    }

    FileDialog {
        id: goldDialog
        objectName: "goldFileDialog"
        title: "Open Gold Reference WAV"
        fileMode: FileDialog.OpenFile
        nameFilters: ["WAV audio (*.wav *.wave)"]
        onAccepted: goldSelection.selectGold(selectedFile)
        onRejected: goldSelection.cancelGoldSelection()
    }

    onClosing: function(close) {
        if (playbackTransport.canStop)
            playbackTransport.stop()
        close.accepted = true
    }

    Column {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: applicationHeader
            objectName: "applicationHeader"
            width: parent.width
            height: 56
            color: root.surface
            border.color: root.border

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPressed: root.startSystemMove()
                onDoubleClicked: {
                    if (root.visibility === Window.Maximized)
                        root.showNormal()
                    else
                        root.showMaximized()
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: 5
                    color: "#102D3B"
                    border.color: root.accent
                    Text {
                        anchors.centerIn: parent
                        text: "RGS"
                        color: root.accent
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                }

                Column {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 1
                    Label {
                        text: "RGS MASTERLAB"
                        color: root.textPrimary
                        font.family: "Segoe UI"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.7
                    }
                    Label {
                        text: "Reference Guided Sound · Gold Reference Mastering Environment"
                        color: root.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                    }
                }

                Item { Layout.fillWidth: true }

                AuditionSourceSelector {
                    id: auditionTargetSelector
                    objectName: "headerAuditionTargetSelector"
                    Layout.preferredWidth: implicitWidth
                    Layout.alignment: Qt.AlignVCenter
                    previousTabItem: fitSourceButton
                    nextTabItem: stopButton
                }

                Row {
                    Layout.preferredHeight: 56
                    spacing: 0
                    StudioButton {
                        objectName: "windowMinimizeButton"
                        width: 46; height: 56
                        implicitWidth: 46; implicitHeight: 56
                        contentPadding: 0
                        text: "—"
                        activeFocusOnTab: false
                        onClicked: root.showMinimized()
                        Accessible.name: "Minimize window"
                    }
                    StudioButton {
                        objectName: "windowMaximizeButton"
                        width: 46; height: 56
                        implicitWidth: 46; implicitHeight: 56
                        contentPadding: 0
                        text: root.visibility === Window.Maximized ? "❐" : "□"
                        activeFocusOnTab: false
                        onClicked: root.visibility === Window.Maximized
                            ? root.showNormal() : root.showMaximized()
                        Accessible.name: root.visibility === Window.Maximized
                            ? "Restore window" : "Maximize window"
                    }
                    StudioButton {
                        objectName: "windowCloseButton"
                        width: 48; height: 56
                        implicitWidth: 48; implicitHeight: 56
                        contentPadding: 0
                        tone: "danger"
                        text: "×"
                        activeFocusOnTab: false
                        onClicked: root.close()
                        Accessible.name: "Close window"
                    }
                }
            }
        }

        MenuBar {
            id: desktopMenu
            objectName: "desktopMenuBar"
            width: parent.width
            height: 30
            background: Rectangle { color: "#0B151C"; border.color: root.border }
            delegate: MenuBarItem {
                id: menuBarItem
                implicitWidth: contentItem.implicitWidth + 24
                implicitHeight: 30
                contentItem: Text {
                    objectName: "desktopMenuBarLabel_" + menuBarItem.text.replace("&", "")
                    text: menuBarItem.text.replace("&", "")
                    color: menuBarItem.highlighted ? root.textPrimary : root.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: menuBarItem.highlighted ? root.surfaceRaised : "transparent"
                }
            }

            Menu {
                title: "&File"
                background: Rectangle { color: root.surface; border.color: root.border }
                StudioMenuItem { text: "New Project"; enabled: false }
                StudioMenuItem { text: "Open Project…"; enabled: false }
                MenuSeparator { }
                StudioMenuItem { objectName: "menuOpenSource"; text: "Open Source…\tCtrl+O"; onTriggered: sourceDialog.open() }
                StudioMenuItem { objectName: "menuOpenGold"; text: "Open Gold Reference…"; onTriggered: goldDialog.open() }
                StudioMenuItem { text: "Clear Gold Reference"; enabled: goldSelection.hasGold; onTriggered: goldSelection.clearGold() }
                MenuSeparator { }
                StudioMenuItem { text: "Save Project"; enabled: false }
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
                title: "&View"
                background: Rectangle { color: root.surface; border.color: root.border }
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

        ColumnLayout {
            width: parent.width
            height: parent.height - applicationHeader.height - desktopMenu.height - statusBar.height
            spacing: 10

            Item { Layout.preferredHeight: 2 }

            Rectangle {
                id: sourcePanel
                objectName: "sourceMetadataPanel"
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.preferredHeight: 104
                color: root.panel
                border.color: root.border
                radius: 5

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        radius: 5
                        color: "#0E2632"
                        border.color: sourceSelection.hasSource ? root.accent : root.border
                        Text {
                            anchors.centerIn: parent
                            text: "WAV"
                            color: sourceSelection.hasSource ? root.accent : root.textSecondary
                            font.family: "Cascadia Mono"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 5
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "SOURCE"; color: root.textSecondary; font.pixelSize: 10; font.weight: Font.DemiBold }
                            Label {
                                id: sourceEmptyState
                                objectName: "sourceEmptyState"
                                Layout.fillWidth: true
                                visible: !sourceSelection.hasSource
                                text: "No Source selected"
                                color: root.textSecondary
                                font.pixelSize: 15
                            }
                            Label {
                                id: sourceDisplayName
                                objectName: "sourceDisplayName"
                                Layout.fillWidth: true
                                visible: sourceSelection.hasSource
                                text: sourceSelection.displayName
                                color: root.textPrimary
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                elide: Text.ElideMiddle
                            }
                            Label {
                                id: sourceReadOnlyBadge
                                objectName: "sourceReadOnlyBadge"
                                visible: sourceSelection.hasSource
                                text: "Read-only"
                                color: "#7FD8A6"
                                font.pixelSize: 10
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14
                            Label { objectName: "sourceContainerMetadata"; visible: sourceSelection.hasSource; text: "Container  " + sourceSelection.containerLabel; color: root.textSecondary; font.pixelSize: 11 }
                            Label { objectName: "sourceFormatMetadata"; visible: sourceSelection.hasSource; text: "Format  " + sourceSelection.sampleFormatLabel; color: root.textSecondary; font.pixelSize: 11 }
                            Label { objectName: "sourceRateMetadata"; visible: sourceSelection.hasSource; text: "Rate  " + sourceSelection.sampleRateHz + " Hz"; color: root.textSecondary; font.pixelSize: 11 }
                            Label { objectName: "sourceChannelsMetadata"; visible: sourceSelection.hasSource; text: "Channels  " + sourceSelection.channelLayoutLabel + " (" + sourceSelection.channelCount + ")"; color: root.textSecondary; font.pixelSize: 11 }
                            Label { objectName: "sourceFramesMetadata"; visible: sourceSelection.hasSource; text: "Frames  " + sourceSelection.frameCount; color: root.textSecondary; font.pixelSize: 11 }
                            Label { objectName: "sourceDurationMetadata"; visible: sourceSelection.hasSource; text: "Duration  " + sourceSelection.durationLabel; color: root.textSecondary; font.pixelSize: 11 }
                            Label { visible: !sourceSelection.hasSource; text: "WAV_ONLY · Source identity and analysis remain read-only"; color: "#667783"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                        }
                    }

                    StudioButton {
                        id: sourceOpenButton
                        objectName: "sourceOpenButton"
                        text: sourceSelection.hasSource ? "Replace Source…" : "Open Source…"
                        tone: "primary"
                        Layout.preferredWidth: 132
                        KeyNavigation.tab: goldOpenButton
                        onClicked: sourceDialog.open()
                        Accessible.name: text
                    }
                    Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; Layout.topMargin: 4; Layout.bottomMargin: 4; color: root.border }
                    ColumnLayout {
                        Layout.preferredWidth: 310
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "GOLD REFERENCE"; color: root.warning; font.pixelSize: 10; font.weight: Font.DemiBold }
                            Label {
                                id: goldStateLabel
                                objectName: "goldStateLabel"
                                Layout.fillWidth: true
                                text: goldSelection.hasGold ? "Gold: " + goldSelection.displayName : "Gold: not loaded"
                                color: goldSelection.hasGold ? root.textPrimary : root.textSecondary
                                elide: Text.ElideMiddle
                                horizontalAlignment: Text.AlignRight
                                font.pixelSize: 11
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            StudioButton {
                                id: goldOpenButton
                                objectName: "goldOpenButton"
                                Layout.fillWidth: true
                                text: goldSelection.hasGold ? "Replace Gold…" : "Open Gold…"
                                contentPadding: 10
                                KeyNavigation.backtab: sourceOpenButton
                                KeyNavigation.tab: goldClearButton
                                onClicked: goldDialog.open()
                                Accessible.name: "Pick independent Gold Reference WAV"
                            }
                            StudioButton {
                                id: goldClearButton
                                objectName: "goldClearButton"
                                text: "Clear"
                                contentPadding: 10
                                enabled: goldSelection.hasGold
                                KeyNavigation.backtab: goldOpenButton
                                KeyNavigation.tab: waveformOverview
                                onClicked: goldSelection.clearGold()
                                Accessible.name: "Clear Gold Reference"
                            }
                        }
                        Label { objectName: "goldMetadataLabel"; Layout.fillWidth: true; visible: goldSelection.hasGold; text: goldSelection.metadata; color: root.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
                    }
                }

                Label {
                    id: sourceErrorMessage
                    objectName: "sourceErrorMessage"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 5
                    visible: sourceSelection.errorMessage.length > 0
                    text: sourceSelection.errorMessage
                    color: root.error
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Accessible.role: Accessible.AlertMessage
                }
            }

            Rectangle {
                id: waveformPanel
                objectName: "sourceWaveformPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 236
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                color: "#050D12"
                border.color: root.border
                radius: 5

                Rectangle {
                    id: waveformHeader
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 38
                    radius: 5
                    color: root.surface
                    border.color: root.border
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        Label { objectName: "sourceWaveformTitle"; text: "SOURCE WAVEFORM"; color: root.textPrimary; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 0.6 }
                        Label { text: "REAL · SOURCE-DERIVED · READ-ONLY"; color: root.textSecondary; font.pixelSize: 9 }
                        Item { Layout.fillWidth: true }
                        Label {
                            objectName: "waveformViewportLabel"
                            visible: sourceWaveform.canNavigate
                            text: sourceWaveform.viewportStartText + "  —  " + sourceWaveform.viewportEndText + "  ·  " + sourceWaveform.viewportDurationText
                            color: root.textSecondary
                            font.family: "Cascadia Mono"
                            font.pixelSize: 10
                        }
                        Label {
                            objectName: "sourceWaveformBounds"
                            visible: sourceWaveform.ready && sourceWaveform.sourceFrameCount > 0
                            text: sourceWaveform.baseBucketCount + " ranges · " + sourceWaveform.levelCount + " levels"
                            color: "#647682"
                            font.pixelSize: 9
                        }
                    }
                }

                WaveformItem {
                    id: waveformOverview
                    objectName: "sourceWaveformOverview"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: waveformHeader.bottom
                    anchors.bottom: parent.bottom
                    anchors.margins: 12
                    presentation: sourceWaveform
                    positionFrames: auditionSelector.sourcePlayheadVisible ? playbackTransport.positionFrames : 0
                    durationFrames: playbackTransport.durationFrames
                    waveformColor: "#43C7F3"
                    zeroLineColor: "#29414F"
                    playheadColor: auditionSelector.sourcePlayheadVisible ? "#E6B94A" : "transparent"
                    overrangeColor: root.error
                    regionColor: "#2A87A95C"
                    regionHandleColor: "#E6EEF0"
                    visible: sourceWaveform.ready && sourceWaveform.sourceFrameCount > 0
                    activeFocusOnTab: visible
                    KeyNavigation.backtab: goldClearButton
                    KeyNavigation.tab: zoomOutButton
                    Accessible.role: Accessible.Pane
                    Accessible.name: sourceWaveform.channelCount === 2 ? "Source waveform, separate left and right lanes" : "Source waveform, mono lane"
                    Accessible.description: "Click to seek, drag to pan, Shift drag to create an Audition Region"
                }

                Label { objectName: "sourceWaveformLeftLane"; anchors.left: waveformOverview.left; anchors.top: waveformOverview.top; visible: waveformOverview.visible && sourceWaveform.channelCount === 2; text: "L"; color: root.textSecondary; font.pixelSize: 10 }
                Label { objectName: "sourceWaveformRightLane"; anchors.left: waveformOverview.left; anchors.bottom: waveformOverview.bottom; visible: waveformOverview.visible && sourceWaveform.channelCount === 2; text: "R"; color: root.textSecondary; font.pixelSize: 10 }
                Label { objectName: "sourceWaveformMonoLane"; anchors.left: waveformOverview.left; anchors.verticalCenter: waveformOverview.verticalCenter; visible: waveformOverview.visible && sourceWaveform.channelCount === 1; text: "C"; color: root.textSecondary; font.pixelSize: 10 }
                Label {
                    id: sourceWaveformStatus
                    objectName: "sourceWaveformStatus"
                    anchors.centerIn: waveformOverview
                    width: waveformOverview.width - 40
                    visible: !sourceWaveform.ready || sourceWaveform.sourceFrameCount === 0
                    text: sourceWaveform.statusText
                    color: sourceWaveform.state === "FAILED" ? root.error : root.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                    Accessible.role: Accessible.StaticText
                    Accessible.name: "Source waveform " + sourceWaveform.state.toLowerCase()
                }
                BusyIndicator { objectName: "sourceWaveformBuildingIndicator"; anchors.horizontalCenter: sourceWaveformStatus.horizontalCenter; anchors.bottom: sourceWaveformStatus.top; running: sourceWaveform.state === "BUILDING"; visible: running }
                StudioButton { objectName: "sourceWaveformRetryButton"; anchors.horizontalCenter: sourceWaveformStatus.horizontalCenter; anchors.top: sourceWaveformStatus.bottom; anchors.topMargin: 8; text: "Retry waveform analysis"; visible: sourceWaveform.state === "FAILED"; onClicked: sourceWaveform.requestRetry() }
                Label { objectName: "sourceWaveformOverrangeIndicator"; anchors.right: waveformOverview.right; anchors.bottom: waveformOverview.bottom; visible: waveformOverview.visible && sourceWaveform.hasOverrange; text: "Source peaks exceed ±1.0"; color: root.error; font.pixelSize: 10 }
            }

            Rectangle {
                id: controlStrip
                objectName: "controlStrip"
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.preferredHeight: 70
                color: root.panel
                border.color: root.border
                radius: 5
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8
                    Label { text: "PRECISION NAVIGATOR"; color: root.textSecondary; font.pixelSize: 9; font.weight: Font.DemiBold }
                    StudioButton {
                        id: zoomOutButton
                        objectName: "waveformZoomOutButton"
                        text: "−"
                        contentPadding: 0
                        Layout.preferredWidth: 38
                        enabled: sourceWaveform.canNavigate && !sourceWaveform.fullFit
                        KeyNavigation.backtab: waveformOverview
                        KeyNavigation.tab: zoomControl
                        onClicked: sourceWaveform.zoomOut()
                        Accessible.name: "Zoom out Source waveform"
                    }
                    Slider {
                        id: zoomControl
                        objectName: "waveformZoomControl"
                        Layout.preferredWidth: 170
                        from: 0.0; to: 1.0; stepSize: 0.001
                        enabled: sourceWaveform.canNavigate
                        activeFocusOnTab: true
                        KeyNavigation.backtab: zoomOutButton
                        KeyNavigation.tab: zoomInButton
                        onMoved: sourceWaveform.setZoomPosition(value)
                        Accessible.name: "Continuous Source waveform zoom"
                        Binding { target: zoomControl; property: "value"; value: sourceWaveform.zoomPosition; when: !zoomControl.pressed }
                        background: Rectangle {
                            x: zoomControl.leftPadding
                            y: zoomControl.topPadding + zoomControl.availableHeight / 2 - height / 2
                            width: zoomControl.availableWidth; height: 3; radius: 2; color: root.border
                            Rectangle { width: zoomControl.visualPosition * parent.width; height: parent.height; radius: 2; color: root.accent }
                        }
                        handle: Rectangle {
                            x: zoomControl.leftPadding + zoomControl.visualPosition * (zoomControl.availableWidth - width)
                            y: zoomControl.topPadding + zoomControl.availableHeight / 2 - height / 2
                            width: 14; height: 14; radius: 7
                            color: zoomControl.enabled ? root.accent : "#586773"
                            border.color: zoomControl.activeFocus ? "#FFFFFF" : "#16384A"
                            border.width: zoomControl.activeFocus ? 2 : 1
                        }
                    }
                    StudioButton {
                        id: zoomInButton
                        objectName: "waveformZoomInButton"
                        text: "+"
                        contentPadding: 0
                        Layout.preferredWidth: 38
                        enabled: sourceWaveform.canNavigate && sourceWaveform.zoomPosition < 1.0
                        KeyNavigation.backtab: zoomControl
                        KeyNavigation.tab: fitSourceButton
                        onClicked: sourceWaveform.zoomIn()
                        Accessible.name: "Zoom in Source waveform"
                    }
                    StudioButton {
                        id: fitSourceButton
                        objectName: "waveformFitSourceButton"
                        text: "Fit Source"
                        enabled: sourceWaveform.canNavigate && !sourceWaveform.fullFit
                        KeyNavigation.backtab: zoomInButton
                        KeyNavigation.tab: auditionTargetSelector.firstTarget
                        onClicked: sourceWaveform.fitSource()
                        Accessible.name: "Fit complete Source waveform"
                    }
                    Item { objectName: "controlStripElasticCenter"; Layout.fillWidth: true }
                    Label { text: "TRANSPORT"; color: root.textSecondary; font.pixelSize: 9; font.weight: Font.DemiBold }
                    StudioButton {
                        id: stopButton
                        objectName: "stopButton"
                        text: "Stop"
                        contentPadding: 0
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        enabled: playbackTransport.canStop
                        KeyNavigation.backtab: auditionTargetSelector.lastTarget
                        KeyNavigation.tab: playPauseButton
                        onClicked: playbackTransport.stop()
                        Accessible.name: "Stop"
                    }
                    StudioButton {
                        id: playPauseButton
                        objectName: "playPauseButton"
                        text: playbackTransport.isPlaying ? "Pause" : "Play"
                        tone: "primary"
                        contentPadding: 0
                        Layout.preferredWidth: 68
                        Layout.preferredHeight: 44
                        enabled: playbackTransport.canPlay || playbackTransport.canPause
                        KeyNavigation.backtab: stopButton
                        KeyNavigation.tab: auditionStartEditor.firstField
                        onClicked: {
                            if (playbackTransport.isPlaying)
                                playbackTransport.pause()
                            else
                                playbackTransport.playOrResume()
                        }
                        Accessible.name: playbackTransport.isPlaying ? "Pause" : "Play"
                    }
                    Rectangle {
                        objectName: "transportTimeModule"
                        Layout.preferredWidth: 210
                        Layout.preferredHeight: 44
                        color: "#050D12"
                        border.color: root.border
                        radius: 4
                        Column {
                            anchors.centerIn: parent
                            spacing: 1
                            Label { objectName: "playbackTimeLabel"; anchors.horizontalCenter: parent.horizontalCenter; text: playbackTransport.positionLabel + "  /  " + playbackTransport.durationLabel; color: root.textPrimary; font.family: "Cascadia Mono"; font.pixelSize: 13 }
                            Label { objectName: "playbackStateLabel"; anchors.horizontalCenter: parent.horizontalCenter; text: playbackTransport.stateLabel; color: root.textSecondary; font.pixelSize: 9 }
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
                Layout.preferredHeight: 110
                color: root.panel
                border.color: root.border
                radius: 5

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 10
                    spacing: 10
                    Label { text: "AUDITION REGION"; color: root.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold }
                    Label { text: "START"; color: root.textSecondary; font.pixelSize: 9 }
                    SegmentedTimeEditor {
                        id: auditionStartEditor
                        objectName: "auditionRegionStartEditor"
                        objectPrefix: "auditionRegionStart"
                        endpointName: "Audition Region start"
                        enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion
                        hoursText: auditionRegion.startHours
                        minutesText: auditionRegion.startMinutes
                        secondsText: auditionRegion.startSeconds
                        fractionText: auditionRegion.startFraction
                        previousTabItem: playPauseButton
                        nextTabItem: auditionEndEditor.firstField
                        onCommitRequested: function(hours, minutes, seconds, fraction) {
                            auditionRegion.commitStartSegments(hours, minutes, seconds, fraction)
                        }
                        onEscapeRequested: auditionRegion.clearError()
                        onNudgeBackwardRequested: auditionRegion.nudgeStartBackward()
                        onNudgeForwardRequested: auditionRegion.nudgeStartForward()
                    }
                    Label { text: "END"; color: root.textSecondary; font.pixelSize: 9 }
                    SegmentedTimeEditor {
                        id: auditionEndEditor
                        objectName: "auditionRegionEndEditor"
                        objectPrefix: "auditionRegionEnd"
                        endpointName: "Audition Region end exclusive"
                        enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion
                        hoursText: auditionRegion.endHours
                        minutesText: auditionRegion.endMinutes
                        secondsText: auditionRegion.endSeconds
                        fractionText: auditionRegion.endFraction
                        previousTabItem: auditionStartEditor.lastField
                        nextTabItem: fitRegionButton
                        onCommitRequested: function(hours, minutes, seconds, fraction) {
                            auditionRegion.commitEndSegments(hours, minutes, seconds, fraction)
                        }
                        onEscapeRequested: auditionRegion.clearError()
                        onNudgeBackwardRequested: auditionRegion.nudgeEndBackward()
                        onNudgeForwardRequested: auditionRegion.nudgeEndForward()
                    }
                    Item { Layout.fillWidth: true }
                    StudioButton {
                        id: fitRegionButton
                        objectName: "waveformFitRegionButton"
                        text: "Fit Region"
                        enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion
                        KeyNavigation.backtab: auditionEndEditor.lastField
                        KeyNavigation.tab: loopRegionCheckBox
                        onClicked: sourceWaveform.fitRegion()
                        Accessible.name: "Fit Audition Region in waveform"
                    }
                    CheckBox {
                        id: loopRegionCheckBox
                        objectName: "auditionRegionLoopCheckBox"
                        text: "Loop Region"
                        checked: auditionRegion.loopEnabled
                        enabled: auditionRegion.canLoop || auditionRegion.loopEnabled
                        activeFocusOnTab: true
                        KeyNavigation.backtab: fitRegionButton
                        KeyNavigation.tab: clearRegionButton
                        onClicked: auditionRegion.requestLoopEnabled(checked)
                        Accessible.name: "Loop Audition Region"
                        contentItem: Text {
                            leftPadding: loopRegionCheckBox.indicator.width + 8
                            text: loopRegionCheckBox.text
                            color: loopRegionCheckBox.enabled ? root.textPrimary : "#6B7A87"
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 12
                        }
                        indicator: Rectangle {
                            implicitWidth: 34
                            implicitHeight: 18
                            x: 0
                            y: parent.height / 2 - height / 2
                            radius: 9
                            color: loopRegionCheckBox.checked ? "#16485E" : "#101A22"
                            border.color: loopRegionCheckBox.activeFocus ? "#3DA6FF" : root.border
                            Rectangle {
                                width: 12; height: 12; radius: 6; y: 3
                                x: loopRegionCheckBox.checked ? parent.width - width - 3 : 3
                                color: loopRegionCheckBox.checked ? root.accent : "#6B7A87"
                            }
                        }
                    }
                    StudioButton {
                        id: clearRegionButton
                        objectName: "auditionRegionClearButton"
                        text: "Clear Region"
                        enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion
                        KeyNavigation.backtab: loopRegionCheckBox
                        KeyNavigation.tab: sourceOpenButton
                        onClicked: auditionRegion.requestClearRegion()
                        Accessible.name: "Clear Audition Region"
                    }
                }

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.bottomMargin: 10
                    Label {
                        objectName: "auditionRegionDetails"
                        visible: auditionRegion.hasRegion
                        text: "Source frames [" + auditionRegion.startFrameText + ", " + auditionRegion.endFrameText + ")  ·  Duration " + auditionRegion.durationText
                        color: root.textSecondary
                        font.family: "Cascadia Mono"
                        font.pixelSize: 10
                    }
                    Label { visible: !auditionRegion.hasRegion; text: "Shift-drag the Source waveform to define one region"; color: "#667783"; font.pixelSize: 10 }
                    Item { Layout.fillWidth: true }
                    Label {
                        objectName: "auditionRegionError"
                        visible: auditionRegion.errorMessage.length > 0
                        text: auditionRegion.errorMessage
                        color: root.error
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }

            Item { Layout.preferredHeight: 2 }
        }

        Rectangle {
            id: statusBar
            objectName: "statusBar"
            width: parent.width
            height: 24
            color: "#0B151C"
            border.color: root.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8
                Rectangle {
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 6
                    radius: 3
                    color: sourceSelection.errorMessage.length > 0 || goldSelection.errorMessage.length > 0 || playbackTransport.errorMessage.length > 0 ? root.error : root.accent
                }
                Label { objectName: "statusBarMessage"; Layout.fillWidth: true; text: root.statusText; color: root.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
                Label { text: "WAV_ONLY"; color: "#667783"; font.pixelSize: 9; font.weight: Font.DemiBold }
            }
        }
    }

    Label { objectName: "playbackErrorMessage"; visible: false; text: playbackTransport.errorMessage }
    Label { objectName: "goldErrorLabel"; visible: false; text: goldSelection.errorMessage }
    Label { objectName: "auditionRoutingStatus"; visible: false; text: auditionSelector.statusText }

    MouseArea {
        z: 1000
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: 5
        cursorShape: Qt.SizeHorCursor
        enabled: root.visibility !== Window.Maximized
        onPressed: root.startSystemResize(Qt.LeftEdge)
    }
    MouseArea {
        z: 1000
        anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: 5
        cursorShape: Qt.SizeHorCursor
        enabled: root.visibility !== Window.Maximized
        onPressed: root.startSystemResize(Qt.RightEdge)
    }
    MouseArea {
        z: 1000
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 5
        cursorShape: Qt.SizeVerCursor
        enabled: root.visibility !== Window.Maximized
        onPressed: root.startSystemResize(Qt.TopEdge)
    }
    MouseArea {
        z: 1000
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 5
        cursorShape: Qt.SizeVerCursor
        enabled: root.visibility !== Window.Maximized
        onPressed: root.startSystemResize(Qt.BottomEdge)
    }
}
