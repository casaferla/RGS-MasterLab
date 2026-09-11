import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    objectName: "mainWindow"
    width: 1280
    height: 720
    minimumWidth: 640
    minimumHeight: 360
    visible: true
    title: "RGS MasterLab"
    color: "#17191d"

    FileDialog {
        id: sourceDialog
        objectName: "sourceFileDialog"
        title: "Open Source WAV"
        fileMode: FileDialog.OpenFile
        nameFilters: ["WAV audio (*.wav *.wave)"]
        onAccepted: sourceSelection.selectSource(selectedFile)
        onRejected: sourceSelection.cancelSourceSelection()
    }

    ScrollView {
        id: workspaceScroll
        anchors.fill: parent
        anchors.margins: 32
        clip: true
        contentWidth: availableWidth
        contentHeight: workspaceContent.implicitHeight
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Column {
            id: workspaceContent
            x: Math.max(0, (workspaceScroll.availableWidth - width) / 2)
            width: Math.min(workspaceScroll.availableWidth, 900)
            spacing: 18

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#f1f3f5"
            font.pixelSize: 32
            font.weight: Font.DemiBold
            text: "RGS MasterLab"
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#8f969e"
            font.pixelSize: 14
            text: "Source workspace"
        }

        Button {
            objectName: "sourceOpenButton"
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Open Source WAV…"
            activeFocusOnTab: true
            onClicked: sourceDialog.open()
        }

        Rectangle {
            objectName: "sourceMetadataPanel"
            width: parent.width
            height: metadataContent.implicitHeight + 40
            radius: 10
            color: "#22262c"
            border.color: "#343a42"

            Column {
                id: metadataContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 20
                spacing: 10

                Label {
                    objectName: "sourceEmptyState"
                    visible: !sourceSelection.hasSource
                    color: "#aeb5bd"
                    text: "No Source selected"
                }

                Label {
                    objectName: "sourceDisplayName"
                    visible: sourceSelection.hasSource
                    color: "#f1f3f5"
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                    width: parent.width
                    text: sourceSelection.displayName
                }

                Label {
                    objectName: "sourceReadOnlyBadge"
                    visible: sourceSelection.hasSource
                    color: "#8ee3b1"
                    text: "Read-only"
                }

                Label {
                    objectName: "sourceContainerMetadata"
                    visible: sourceSelection.hasSource
                    color: "#d7dce2"
                    text: "Container: " + sourceSelection.containerLabel
                }

                Label {
                    objectName: "sourceFormatMetadata"
                    visible: sourceSelection.hasSource
                    color: "#d7dce2"
                    text: "Format: " + sourceSelection.sampleFormatLabel
                }

                Label {
                    objectName: "sourceRateMetadata"
                    visible: sourceSelection.hasSource
                    color: "#d7dce2"
                    text: "Sample rate: " + sourceSelection.sampleRateHz + " Hz"
                }

                Label {
                    objectName: "sourceChannelsMetadata"
                    visible: sourceSelection.hasSource
                    color: "#d7dce2"
                    text: "Channels: " + sourceSelection.channelLayoutLabel
                          + " (" + sourceSelection.channelCount + ")"
                }

                Label {
                    objectName: "sourceFramesMetadata"
                    visible: sourceSelection.hasSource
                    color: "#d7dce2"
                    text: "Frames: " + sourceSelection.frameCount
                }

                Label {
                    objectName: "sourceDurationMetadata"
                    visible: sourceSelection.hasSource
                    color: "#d7dce2"
                    text: "Duration: " + sourceSelection.durationLabel
                }
            }
        }

        Rectangle {
            objectName: "playbackTransportPanel"
            width: parent.width
            height: transportContent.implicitHeight + 32
            radius: 10
            color: "#22262c"
            border.color: "#343a42"

            Column {
                id: transportContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 12

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 12

                    Button {
                        objectName: "playPauseButton"
                        text: playbackTransport.isPlaying ? "Pause" : "Play"
                        enabled: playbackTransport.canPlay || playbackTransport.canPause
                        activeFocusOnTab: true
                        onClicked: {
                            if (playbackTransport.isPlaying)
                                playbackTransport.pause()
                            else
                                playbackTransport.playOrResume()
                        }
                    }

                    Button {
                        objectName: "stopButton"
                        text: "Stop"
                        enabled: playbackTransport.canStop
                        activeFocusOnTab: true
                        onClicked: playbackTransport.stop()
                    }
                }

                Label {
                    objectName: "playbackStateLabel"
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: playbackTransport.playbackAvailable ? "#d7dce2" : "#aeb5bd"
                    text: playbackTransport.stateLabel
                }

                Label {
                    objectName: "playbackTimeLabel"
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#aeb5bd"
                    text: playbackTransport.positionLabel + " / "
                          + playbackTransport.durationLabel
                }

                Label {
                    objectName: "playbackErrorMessage"
                    visible: playbackTransport.errorMessage.length > 0
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: "#ffd9dc"
                    text: playbackTransport.errorMessage
                }
            }
        }

        Rectangle {
            objectName: "sourceWaveformPanel"
            width: parent.width
            height: 475
            radius: 10
            color: "#22262c"
            border.color: "#343a42"

            Label {
                id: waveformTitle
                objectName: "sourceWaveformTitle"
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 16
                color: "#f1f3f5"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                text: "Source / RAW · Waveform navigation"
            }

            Label {
                objectName: "sourceWaveformBounds"
                anchors.right: parent.right
                anchors.verticalCenter: waveformTitle.verticalCenter
                anchors.rightMargin: 16
                color: "#8f969e"
                font.pixelSize: 11
                visible: sourceWaveform.ready && sourceWaveform.sourceFrameCount > 0
                text: sourceWaveform.baseBucketCount + " base ranges · "
                      + sourceWaveform.levelCount + " levels"
            }

            Row {
                id: waveformNavigationControls
                objectName: "waveformNavigationControls"
                anchors.left: parent.left
                anchors.top: waveformTitle.bottom
                anchors.leftMargin: 16
                anchors.topMargin: 10
                spacing: 8

                Button {
                    objectName: "waveformZoomInButton"
                    text: "Zoom In"
                    enabled: sourceWaveform.canNavigate
                    activeFocusOnTab: true
                    onClicked: sourceWaveform.zoomIn()
                    Accessible.name: "Zoom in Source waveform"
                }

                Button {
                    objectName: "waveformZoomOutButton"
                    text: "Zoom Out"
                    enabled: sourceWaveform.canNavigate && !sourceWaveform.fullFit
                    activeFocusOnTab: true
                    onClicked: sourceWaveform.zoomOut()
                    Accessible.name: "Zoom out Source waveform"
                }

                Button {
                    objectName: "waveformFitSourceButton"
                    text: "Fit Source"
                    enabled: sourceWaveform.canNavigate && !sourceWaveform.fullFit
                    activeFocusOnTab: true
                    onClicked: sourceWaveform.fitSource()
                    Accessible.name: "Fit complete Source waveform"
                }
            }

            Label {
                objectName: "waveformViewportLabel"
                anchors.left: waveformNavigationControls.right
                anchors.right: parent.right
                anchors.verticalCenter: waveformNavigationControls.verticalCenter
                anchors.leftMargin: 12
                anchors.rightMargin: 16
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
                color: "#aeb5bd"
                font.pixelSize: 11
                visible: sourceWaveform.canNavigate
                text: "Visible " + sourceWaveform.viewportStartText + " — "
                      + sourceWaveform.viewportEndText + " · "
                      + sourceWaveform.viewportDurationText
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            WaveformItem {
                id: waveformOverview
                objectName: "sourceWaveformOverview"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: waveformNavigationControls.bottom
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 10
                height: 190
                presentation: sourceWaveform
                positionFrames: playbackTransport.positionFrames
                durationFrames: playbackTransport.durationFrames
                waveformColor: "#73d6ff"
                zeroLineColor: "#505862"
                playheadColor: "#f5c96a"
                overrangeColor: "#ff7d8a"
                regionColor: "#487a9660"
                regionHandleColor: "#f1f3f5"
                visible: sourceWaveform.ready && sourceWaveform.sourceFrameCount > 0
                activeFocusOnTab: visible
                Accessible.role: Accessible.Pane
                Accessible.name: sourceWaveform.channelCount === 2
                    ? "Source waveform navigation, separate left and right lanes"
                    : "Source waveform navigation, mono lane"
                Accessible.description: "Click to seek, drag to pan, Shift drag to create an Audition Region"
                KeyNavigation.tab: auditionStartEditor.firstField
            }

            Label {
                objectName: "sourceWaveformLeftLane"
                anchors.left: waveformOverview.left
                anchors.top: waveformOverview.top
                color: "#aeb5bd"
                font.pixelSize: 10
                visible: sourceWaveform.ready && sourceWaveform.sourceFrameCount > 0
                         && sourceWaveform.channelCount === 2
                text: "L"
            }

            Label {
                objectName: "sourceWaveformRightLane"
                anchors.left: waveformOverview.left
                anchors.bottom: waveformOverview.bottom
                color: "#aeb5bd"
                font.pixelSize: 10
                visible: sourceWaveform.ready && sourceWaveform.sourceFrameCount > 0
                         && sourceWaveform.channelCount === 2
                text: "R"
            }

            Label {
                objectName: "sourceWaveformMonoLane"
                anchors.left: waveformOverview.left
                anchors.verticalCenter: waveformOverview.verticalCenter
                color: "#aeb5bd"
                font.pixelSize: 10
                visible: sourceWaveform.ready && sourceWaveform.sourceFrameCount > 0
                         && sourceWaveform.channelCount === 1
                text: "C"
            }

            Label {
                id: sourceWaveformStatus
                objectName: "sourceWaveformStatus"
                anchors.centerIn: waveformOverview
                width: waveformOverview.width - 32
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: sourceWaveform.state === "FAILED" ? "#ffd9dc" : "#aeb5bd"
                visible: !sourceWaveform.ready || sourceWaveform.sourceFrameCount === 0
                text: sourceWaveform.statusText
                Accessible.role: Accessible.StaticText
                Accessible.name: "Source waveform " + sourceWaveform.state.toLowerCase()
                Accessible.description: sourceWaveform.statusText
            }

            BusyIndicator {
                objectName: "sourceWaveformBuildingIndicator"
                anchors.horizontalCenter: waveformOverview.horizontalCenter
                anchors.bottom: sourceWaveformStatus.top
                anchors.bottomMargin: 8
                running: sourceWaveform.state === "BUILDING"
                visible: running
            }

            Button {
                objectName: "sourceWaveformRetryButton"
                anchors.horizontalCenter: waveformOverview.horizontalCenter
                anchors.top: sourceWaveformStatus.bottom
                anchors.topMargin: 10
                text: "Retry waveform analysis"
                visible: sourceWaveform.state === "FAILED"
                activeFocusOnTab: visible
                onClicked: sourceWaveform.requestRetry()
            }

            Label {
                objectName: "sourceWaveformOverrangeIndicator"
                anchors.right: waveformOverview.right
                anchors.bottom: waveformOverview.bottom
                color: "#ff9aa4"
                font.pixelSize: 10
                visible: sourceWaveform.ready && sourceWaveform.hasOverrange
                text: "Source peaks exceed ±1.0"
            }

            Flow {
                id: auditionRegionControls
                objectName: "auditionRegionControls"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: waveformOverview.bottom
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 12
                spacing: 8
                height: childrenRect.height

                Label {
                    color: "#d7dce2"
                    text: "Region"
                }

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
                    previousTabItem: waveformOverview
                    nextTabItem: auditionEndEditor.firstField
                    onCommitRequested: function(hours, minutes, seconds, fraction) {
                        auditionRegion.commitStartSegments(
                            hours, minutes, seconds, fraction)
                    }
                    onEscapeRequested: auditionRegion.clearError()
                    onNudgeBackwardRequested: auditionRegion.nudgeStartBackward()
                    onNudgeForwardRequested: auditionRegion.nudgeStartForward()
                }

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
                        auditionRegion.commitEndSegments(
                            hours, minutes, seconds, fraction)
                    }
                    onEscapeRequested: auditionRegion.clearError()
                    onNudgeBackwardRequested: auditionRegion.nudgeEndBackward()
                    onNudgeForwardRequested: auditionRegion.nudgeEndForward()
                }

                Button {
                    id: fitRegionButton
                    objectName: "waveformFitRegionButton"
                    text: "Fit Region"
                    enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion
                    activeFocusOnTab: true
                    onClicked: sourceWaveform.fitRegion()
                    Accessible.name: "Fit Audition Region in waveform"
                }

                CheckBox {
                    objectName: "auditionRegionLoopCheckBox"
                    text: "Loop Region"
                    checked: auditionRegion.loopEnabled
                    enabled: auditionRegion.canLoop || auditionRegion.loopEnabled
                    activeFocusOnTab: true
                    onClicked: auditionRegion.requestLoopEnabled(checked)
                    Accessible.name: "Loop Audition Region"
                    Accessible.description: auditionRegion.loopEnabled
                        ? "Loop Region armed; arbitrary boundaries may click"
                        : "Loop Region disarmed"
                }

                Button {
                    objectName: "auditionRegionClearButton"
                    text: "Clear"
                    enabled: auditionRegion.controlsEnabled && auditionRegion.hasRegion
                    activeFocusOnTab: true
                    onClicked: auditionRegion.requestClearRegion()
                    Accessible.name: "Clear Audition Region"
                }
            }

            Label {
                id: auditionRegionDetails
                objectName: "auditionRegionDetails"
                anchors.left: parent.left
                anchors.top: auditionRegionControls.bottom
                anchors.leftMargin: 16
                anchors.topMargin: 8
                color: "#aeb5bd"
                visible: auditionRegion.hasRegion
                text: "Source frames [" + auditionRegion.startFrameText + ", "
                      + auditionRegion.endFrameText + ") · Duration "
                      + auditionRegion.durationText
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            Label {
                objectName: "auditionRegionError"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: auditionRegionDetails.bottom
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 4
                color: "#ffd9dc"
                visible: auditionRegion.errorMessage.length > 0
                wrapMode: Text.Wrap
                text: auditionRegion.errorMessage
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }

        Rectangle {
            objectName: "sourceErrorPanel"
            width: parent.width
            height: errorLabel.implicitHeight + 24
            radius: 8
            color: "#47272b"
            border.color: "#8c414a"
            visible: sourceSelection.errorMessage.length > 0

            Label {
                id: errorLabel
                objectName: "sourceErrorMessage"
                anchors.fill: parent
                anchors.margins: 12
                color: "#ffd9dc"
                wrapMode: Text.Wrap
                text: sourceSelection.errorMessage
            }
        }
        }
    }
}
