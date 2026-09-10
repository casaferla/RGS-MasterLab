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

    Column {
        width: Math.min(parent.width - 64, 760)
        anchors.centerIn: parent
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
