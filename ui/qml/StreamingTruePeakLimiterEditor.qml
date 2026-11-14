import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "streamingTruePeakLimiterEditor"
    property var limiterModule: null
    readonly property color copper: "#C4774A"
    readonly property bool compact: height < 390
    readonly property double ceiling: limiterModule ? limiterModule.ceilingDbTp : -1.0
    readonly property double drive: limiterModule ? limiterModule.driveDb : 0.0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? 8 : 14
        spacing: root.compact ? 5 : 12
        // The DspEditorHost already provides the module title and context.
        // Reserve vertical space for the two Input Gain-sized DSP controls.

        Rectangle {
            objectName: "streamingP2MainPanel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            radius: 8
            color: "#0B1824" // InputGainEditor mainPanel
            border.width: 1
            border.color: "#1E354A"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.compact ? 9 : 20
                spacing: root.compact ? 3 : 12

                // Input Gain's large centered numeric readout ABOVE its
                // full-width horizontal slider, repeated twice, not two
                // small knobs/side-by-side compact rows. Exact 36px Consolas
                // Bold, 640px maximum track, 8px track and 18px thumb.
                ColumnLayout {
                    objectName: "streamingP2CeilingGroup"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 640
                    Layout.alignment: Qt.AlignHCenter
                    spacing: root.compact ? 1 : 6

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.compact ? 41 : 50
                        Text {
                            text: "CEILING"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        Text {
                            objectName: "streamingP2CeilingDisplay"
                            anchors.centerIn: parent
                            text: root.ceiling.toFixed(1) + " dBTP"
                            color: root.ceiling === -1.0 ? "#F5F8FC" : root.copper
                            font.family: "Consolas"
                            font.pixelSize: 36
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 13
                        Text { text: "−12.0 dBTP"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                        Item { Layout.fillWidth: true }
                        Text { text: "−6.0 dBTP"; color: "#A1B5C9"; font.family: "Consolas"; font.pixelSize: 11; font.weight: Font.Bold }
                        Item { Layout.fillWidth: true }
                        Text { text: "0.0 dBTP"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                    }
                    StudioParameterSlider {
                        objectName: "streamingP2CeilingSlider"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        Layout.minimumHeight: 24
                        compact: false // Input Gain 8px track / 18px thumb, even at 1184x688
                        from: -12; to: 0; stepSize: 0.1
                        wheelEnabled: true
                        enabled: !!root.limiterModule
                        // M17 ceiling convention: colored extent begins at
                        // 0 dBTP (right endpoint), moves LEFT to the selected
                        // negative ceiling. Never fill from -12 to value.
                        fillFromOrigin: true
                        positionFillOrigin: 0
                        positionFillColor: Qt.alpha(root.copper, 0.38)
                        accentColor: root.copper
                        value: root.ceiling
                        accessibleName: "TP Limiter Ceiling slider"
                        interactionHint: "Mouse wheel: 0.1 dBTP per detent; drag and release to apply."
                        onCommitRequested: function(v) {
                            if (root.limiterModule) root.limiterModule.setCeilingDbTp(v)
                        }
                    }
                }

                ColumnLayout {
                    objectName: "streamingP2DriveGroup"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 640
                    Layout.alignment: Qt.AlignHCenter
                    spacing: root.compact ? 1 : 6

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.compact ? 41 : 50
                        Text {
                            text: "DRIVE"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        Text {
                            objectName: "streamingP2DriveDisplay"
                            anchors.centerIn: parent
                            text: (root.drive > 0 ? "+" : "") + root.drive.toFixed(1) + " dB"
                            color: root.drive === 0.0 ? "#F5F8FC" : root.copper
                            font.family: "Consolas"
                            font.pixelSize: 36
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 13
                        Text { text: "−24.0 dB"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                        Item { Layout.fillWidth: true }
                        Text { text: "0.0 dB"; color: root.drive === 0.0 ? root.copper : "#A1B5C9"; font.family: "Consolas"; font.pixelSize: 11; font.weight: Font.Bold }
                        Item { Layout.fillWidth: true }
                        Text { text: "+24.0 dB"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                    }
                    StudioParameterSlider {
                        objectName: "streamingP2DriveSlider"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        Layout.minimumHeight: 24
                        compact: false // Input Gain 8px track / 18px thumb
                        from: -24; to: 24; stepSize: 0.1
                        wheelEnabled: true
                        enabled: !!root.limiterModule
                        fillFromOrigin: true
                        positionFillOrigin: 0
                        showOriginTick: true
                        accentColor: root.copper
                        value: root.drive
                        accessibleName: "TP Limiter Drive slider"
                        interactionHint: "Pre-limiter gain. Mouse wheel: 0.1 dB per detent."
                        onCommitRequested: function(v) {
                            if (root.limiterModule) root.limiterModule.setDriveDb(v)
                        }
                    }
                }

                // Research variant only. The shipping margin will be fixed
                // after P2 qualification, not promoted to user-facing DSP UI.
                RowLayout {
                    objectName: "streamingP2ResearchMarginRow"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredHeight: root.compact ? 24 : 32
                    spacing: 8
                    Text {
                        text: "Research margin"
                        color: "#A1B5C9"
                        font.family: "Segoe UI"
                        font.pixelSize: 11
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
                        text: root.limiterModule ? "Selected: " + root.limiterModule.marginPercent + "%" : "—"
                        color: root.copper
                        font.family: "Consolas"
                        font.pixelSize: 12
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
                Text {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.compact ? 15 : 28
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: root.limiterModule
                        ? root.limiterModule.stateText + " · " + root.limiterModule.previewStatus
                        : "P2 has no verified audio owner."
                    color: root.limiterModule && root.limiterModule.hasError ? "#F27683" : "#D1E2E8"
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    ToolTip.text: text
                    ToolTip.visible: statusHover.hovered
                    ToolTip.delay: 900
                    HoverHandler { id: statusHover }
                }
            }
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "EXPERIMENTAL: preview audio only. No project save or export qualification."
            color: "#A1B5C9"
            font.family: "Segoe UI"
            font.pixelSize: 11
        }
    }
}
