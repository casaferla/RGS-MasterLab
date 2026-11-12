import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "streamingTruePeakLimiterEditor"
    property var limiterModule: null
    readonly property color copper: "#C4774A"
    readonly property bool compact: height < 390

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? 8 : 14
        spacing: root.compact ? 7 : 12
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: "STREAMING TP LIMITER"
                color: "#DDE8F4"
                font.family: "Segoe UI"
                font.pixelSize: root.compact ? 11 : 13
                font.weight: Font.Bold
            }
            Rectangle { width: 2; height: 16; color: root.copper }
            Text {
                text: "MANUAL · RESEARCH PREVIEW"
                color: "#A1B5C9"; font.family: "Segoe UI"; font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
            StudioButton {
                objectName: "streamingP2ResetButton"
                text: "Reset to Default"
                minimumControlWidth: 130
                accentColor: root.copper
                enabled: !!root.limiterModule
                onClicked: if (root.limiterModule) root.limiterModule.resetToDefault()
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            radius: 6
            color: "#081824"
            border.width: 1; border.color: "#27465F"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.compact ? 9 : 16
                spacing: root.compact ? 10 : 18
                Text {
                    text: "CEILING  ·  DRIVE  ·  SAFETY MARGIN"
                    color: "#A1B5C9"; font.family: "Segoe UI"
                    font.pixelSize: 11; font.weight: Font.Bold
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    Text {
                        text: "Ceiling"; color: "#D1E2E8"
                        font.family: "Segoe UI"; font.pixelSize: 12
                        Layout.preferredWidth: 90
                    }
                    StudioParameterSlider {
                        objectName: "streamingP2CeilingSlider"
                        Layout.fillWidth: true; compact: root.compact
                        from: -12; to: 0; stepSize: 0.1
                        wheelEnabled: true
                        enabled: !!root.limiterModule
                        accentColor: root.copper
                        value: root.limiterModule ? root.limiterModule.ceilingDbTp : -1
                        interactionHint: "TP ceiling in dBTP, applied to the real P2 preview."
                        onCommitRequested: function(v) {
                            if (root.limiterModule) root.limiterModule.setCeilingDbTp(v)
                        }
                    }
                    Text {
                        Layout.preferredWidth: 94
                        horizontalAlignment: Text.AlignRight
                        text: (root.limiterModule ? root.limiterModule.ceilingDbTp : -1).toFixed(1) + " dBTP"
                        color: "#F5F8FC"; font.family: "Consolas"; font.pixelSize: 14
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    Text {
                        text: "Drive"; color: "#D1E2E8"
                        font.family: "Segoe UI"; font.pixelSize: 12
                        Layout.preferredWidth: 90
                    }
                    StudioParameterSlider {
                        objectName: "streamingP2DriveSlider"
                        Layout.fillWidth: true; compact: root.compact
                        from: -24; to: 24; stepSize: 0.1
                        wheelEnabled: true
                        enabled: !!root.limiterModule
                        // Same zero-centered bipolar gain convention as
                        // Input Gain, Compressor makeup, and Stereo M/S.
                        fillFromOrigin: true
                        positionFillOrigin: 0
                        accentColor: root.copper
                        value: root.limiterModule ? root.limiterModule.driveDb : 0
                        interactionHint: "Input drive in dB for Streaming TP Limiter."
                        onCommitRequested: function(v) {
                            if (root.limiterModule) root.limiterModule.setDriveDb(v)
                        }
                    }
                    Text {
                        Layout.preferredWidth: 94
                        horizontalAlignment: Text.AlignRight
                        text: (root.limiterModule ? root.limiterModule.driveDb : 0).toFixed(1) + " dB"
                        color: "#F5F8FC"; font.family: "Consolas"; font.pixelSize: 14
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    Text {
                        text: "Margin"; color: "#D1E2E8"
                        font.family: "Segoe UI"; font.pixelSize: 12
                        Layout.preferredWidth: 90
                    }
                    StudioButton {
                        objectName: "streamingP2Margin2Button"
                        text: "2%"; minimumControlWidth: 60
                        accentColor: root.copper
                        enabled: !!root.limiterModule
                        onClicked: if (root.limiterModule) root.limiterModule.setMarginPercent(2)
                    }
                    StudioButton {
                        objectName: "streamingP2Margin5Button"
                        text: "5%"; minimumControlWidth: 60
                        accentColor: root.copper
                        enabled: !!root.limiterModule
                        onClicked: if (root.limiterModule) root.limiterModule.setMarginPercent(5)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.limiterModule ? "Selected: " + root.limiterModule.marginPercent + "%" : "Unavailable"
                        color: root.copper; font.family: "Consolas"; font.pixelSize: 12
                    }
                }
                Item { Layout.fillHeight: true }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: root.limiterModule
                        ? root.limiterModule.stateText + " · " + root.limiterModule.previewStatus
                        : "P2 has no verified audio owner."
                    color: root.limiterModule && root.limiterModule.hasError ? "#F27683" : "#D1E2E8"
                    font.family: "Segoe UI"; font.pixelSize: 12
                }
            }
        }
        Text {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: "EXPERIMENTAL: preview audio only. No project save or export qualification."
            color: "#A1B5C9"; font.family: "Segoe UI"; font.pixelSize: 11
        }
    }
}
