import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "compressorEditor"

    property var viewModel: null

    readonly property color textPrimary: "#F5F8FC"
    readonly property color textSecondary: "#A1B5C9"
    readonly property color textMuted: "#586773"
    readonly property color accent: "#00C8FF"
    readonly property color copperAccent: "#C4774A"
    readonly property color error: "#F27683"
    readonly property color borderDark: "#1E354A"
    readonly property color panelBg: "#0B1824"
    readonly property color wellBg: "#060D14"

    readonly property string validationField: root.viewModel ? root.viewModel.validationField : ""
    readonly property string validationErrorText: root.viewModel ? root.viewModel.validationMessage : ""
    readonly property bool hasValidationError: validationErrorText.length > 0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // Header Action Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "DYNAMICS COMPRESSOR"
                color: root.textSecondary
                font.family: "Segoe UI"
                font.pixelSize: 11
                font.weight: Font.Bold
            }

            Item { Layout.fillWidth: true }

            // Reset to Default Button
            StudioButton {
                id: resetButton
                objectName: "resetCompressorButton"
                text: "Reset to Default"
                minimumControlWidth: 140
                enabled: root.viewModel !== null
                onClicked: if (root.viewModel) root.viewModel.resetToDefault()
                Accessible.name: "Reset Compressor to default parameters"
                ToolTip.text: "Reset compressor to canonical M13 defaults"
                ToolTip.visible: hovered
            }
        }

        // Main Editor Body Split
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // Left Side: Static Transfer Curve Display Well
            Rectangle {
                id: curveWell
                objectName: "compressorCurveWell"
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                radius: 6
                color: root.wellBg
                border.color: root.borderDark
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "STATIC TRANSFER CURVE"
                            color: root.copperAccent
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "In: -60..+6 dBFS"
                            color: root.textMuted
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                        }
                    }

                    // Canvas Graph
                    Canvas {
                        id: curveCanvas
                        objectName: "compressorCurveCanvas"
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        property var pointsList: root.viewModel ? root.viewModel.transferCurvePoints : []

                        onPointsListChanged: requestPaint()
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)

                            const minDbfs = -60.0
                            const maxDbfs = 6.0
                            const dbRange = maxDbfs - minDbfs

                            function mapX(db) {
                                return (db - minDbfs) / dbRange * width
                            }
                            function mapY(db) {
                                return height - ((db - minDbfs) / dbRange * height)
                            }

                            // Grid Lines
                            ctx.lineWidth = 1
                            ctx.strokeStyle = "#122536"
                            const gridSteps = [-48, -36, -24, -12, 0]
                            for (var i = 0; i < gridSteps.length; ++i) {
                                var gx = mapX(gridSteps[i])
                                var gy = mapY(gridSteps[i])

                                // Vertical line
                                ctx.beginPath()
                                ctx.moveTo(gx, 0)
                                ctx.lineTo(gx, height)
                                ctx.stroke()

                                // Horizontal line
                                ctx.beginPath()
                                ctx.moveTo(0, gy)
                                ctx.lineTo(width, gy)
                                ctx.stroke()
                            }

                            // 1:1 45-degree Reference Line
                            ctx.strokeStyle = "#1E3A52"
                            ctx.setLineDash([3, 3])
                            ctx.beginPath()
                            ctx.moveTo(mapX(-60), mapY(-60))
                            ctx.lineTo(mapX(6), mapY(6))
                            ctx.stroke()
                            ctx.setLineDash([])

                            // Transfer Curve
                            if (pointsList && pointsList.length > 0) {
                                ctx.strokeStyle = "#C4774A"
                                ctx.lineWidth = 2
                                ctx.beginPath()

                                for (var p = 0; p < pointsList.length; ++p) {
                                    var pt = pointsList[p]
                                    var px = mapX(pt.inputDbfs)
                                    var py = mapY(pt.outputDbfs)

                                    if (p === 0) {
                                        ctx.moveTo(px, py)
                                    } else {
                                        ctx.lineTo(px, py)
                                    }
                                }
                                ctx.stroke()
                            }
                        }
                    }
                }
            }

            // Right Side: Control Panels
            Rectangle {
                id: controlsPanel
                objectName: "compressorControlsPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 6
                color: root.panelBg
                border.color: root.hasValidationError ? root.error : root.borderDark
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 12

                        // Detector & Stereo Link Selector Strip
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            // Detector Mode Group
                            ColumnLayout {
                                spacing: 4
                                Text {
                                    text: "DETECTOR MODE"
                                    color: root.textSecondary
                                    font.family: "Segoe UI"
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                }
                                RowLayout {
                                    spacing: 2
                                    StudioButton {
                                        objectName: "detectorRmsButton"
                                        text: "RMS"
                                        selected: root.viewModel ? (root.viewModel.detectorMode === "RMS") : true
                                        minimumControlWidth: 64
                                        onClicked: if (root.viewModel) root.viewModel.setDetectorMode("RMS")
                                    }
                                    StudioButton {
                                        objectName: "detectorPeakButton"
                                        text: "PEAK"
                                        selected: root.viewModel ? (root.viewModel.detectorMode === "PEAK") : false
                                        minimumControlWidth: 64
                                        onClicked: if (root.viewModel) root.viewModel.setDetectorMode("PEAK")
                                    }
                                }
                            }

                            // Stereo Link Group (Applicability bound)
                            ColumnLayout {
                                opacity: root.viewModel ? (root.viewModel.channelLinkEffective ? 1.0 : 0.4) : 1.0
                                enabled: root.viewModel ? root.viewModel.channelLinkEffective : true
                                spacing: 4

                                RowLayout {
                                    spacing: 4
                                    Text {
                                        text: "STEREO LINK"
                                        color: root.textSecondary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                    }
                                    Text {
                                        text: "(MONO - Non-Effective)"
                                        color: root.textMuted
                                        font.family: "Segoe UI"
                                        font.pixelSize: 9
                                        visible: root.viewModel ? !root.viewModel.channelLinkEffective : false
                                    }
                                }

                                RowLayout {
                                    spacing: 2
                                    StudioButton {
                                        objectName: "linkMaxButton"
                                        text: "LINKED MAX"
                                        selected: root.viewModel ? (root.viewModel.channelLink === "LINKED_MAX") : true
                                        minimumControlWidth: 90
                                        onClicked: if (root.viewModel) root.viewModel.setChannelLink("LINKED_MAX")
                                    }
                                    StudioButton {
                                        objectName: "linkMeanButton"
                                        text: "LINKED MEAN"
                                        selected: root.viewModel ? (root.viewModel.channelLink === "LINKED_MEAN") : false
                                        minimumControlWidth: 90
                                        onClicked: if (root.viewModel) root.viewModel.setChannelLink("LINKED_MEAN")
                                    }
                                    StudioButton {
                                        objectName: "linkDualMonoButton"
                                        text: "DUAL MONO"
                                        selected: root.viewModel ? (root.viewModel.channelLink === "DUAL_MONO") : false
                                        minimumControlWidth: 80
                                        onClicked: if (root.viewModel) root.viewModel.setChannelLink("DUAL_MONO")
                                    }
                                }
                            }
                        }

                        // Divider
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#1A2C3D"
                        }

                        // Grid of Numeric Controls
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 3
                            columnSpacing: 16
                            rowSpacing: 12

                            // 1. Threshold
                            StudioNumericField {
                                id: thresholdField
                                fieldName: "thresholdDbfs"
                                labelText: "THRESHOLD"
                                unitText: "dBFS"
                                rawText: root.viewModel ? root.viewModel.thresholdText : "-24.0"
                                viewModel: root.viewModel
                                compact: true
                            }

                            // 2. Ratio
                            StudioNumericField {
                                id: ratioField
                                fieldName: "ratio"
                                labelText: "RATIO"
                                unitText: ": 1"
                                rawText: root.viewModel ? root.viewModel.ratioText : "2.00"
                                viewModel: root.viewModel
                                compact: true
                            }

                            // 3. Knee
                            StudioNumericField {
                                id: kneeField
                                fieldName: "kneeDb"
                                labelText: "KNEE"
                                unitText: "dB"
                                rawText: root.viewModel ? root.viewModel.kneeText : "6.0"
                                viewModel: root.viewModel
                                compact: true
                            }

                            // 4. Attack
                            StudioNumericField {
                                id: attackField
                                fieldName: "attackMs"
                                labelText: "ATTACK"
                                unitText: "ms"
                                rawText: root.viewModel ? root.viewModel.attackText : "30.0"
                                viewModel: root.viewModel
                                compact: true
                            }

                            // 5. Release
                            StudioNumericField {
                                id: releaseField
                                fieldName: "releaseMs"
                                labelText: "RELEASE"
                                unitText: "ms"
                                rawText: root.viewModel ? root.viewModel.releaseText : "200.0"
                                viewModel: root.viewModel
                                compact: true
                            }

                            // 6. RMS Time Constant (Applicability bound)
                            StudioNumericField {
                                id: rmsTimeField
                                fieldName: "rmsTimeConstantMs"
                                labelText: root.viewModel && root.viewModel.rmsTimeEffective ? "RMS TIME" : "RMS TIME (PEAK)"
                                unitText: "ms"
                                rawText: root.viewModel ? root.viewModel.rmsTimeConstantText : "50.0"
                                viewModel: root.viewModel
                                compact: true
                                opacity: root.viewModel ? (root.viewModel.rmsTimeEffective ? 1.0 : 0.4) : 1.0
                                enabled: root.viewModel ? root.viewModel.rmsTimeEffective : true
                            }

                            // 7. Lookahead
                            StudioNumericField {
                                id: lookAheadField
                                fieldName: "lookAheadMs"
                                labelText: "LOOKAHEAD"
                                unitText: "ms"
                                rawText: root.viewModel ? root.viewModel.lookAheadText : "5.0"
                                viewModel: root.viewModel
                                compact: true
                            }

                            // 8. Mix
                            StudioNumericField {
                                id: mixField
                                fieldName: "mixPercent"
                                labelText: "MIX"
                                unitText: "%"
                                rawText: root.viewModel ? root.viewModel.mixPercentText : "100.0"
                                viewModel: root.viewModel
                                compact: true
                            }

                            // 9. Make-up Gain
                            StudioNumericField {
                                id: makeupField
                                fieldName: "makeupGainDb"
                                labelText: "MAKE-UP"
                                unitText: "dB"
                                rawText: root.viewModel ? root.viewModel.makeupGainText : "0.0"
                                viewModel: root.viewModel
                                compact: true
                            }
                        }

                        // Validation Error Display
                        Text {
                            id: validationErrorLabel
                            objectName: "compressorValidationError"
                            text: root.validationErrorText
                            color: root.error
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            visible: root.hasValidationError
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }
    }
}
