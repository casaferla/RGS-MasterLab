import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "stereoMsEditor"
    property var viewModel: null

    readonly property color seaGreen: "#4A9A88"
    readonly property color widthHue: "#2ED3FF"
    readonly property color cutoffHue: "#2FD98F"
    readonly property color lowHue: "#FFD84A"
    readonly property color sliderFill: Qt.rgba(74 / 255, 154 / 255, 136 / 255, 0.38)

    // EQ-like top expansive region, but a separate near-square, strictly
    // observational M/S output well: its coordinates must never be stretched.
    ColumnLayout {
        anchors.fill: parent
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 27
            spacing: 8
            Text {
                text: "SPATIAL CONTROL"
                color: "#A1B5C9"
                font.family: "Segoe UI"
                font.pixelSize: 11
                font.weight: Font.Bold
            }
            Rectangle { width: 2; height: 14; color: root.seaGreen }
            Text {
                text: "Width · Mono Bass"
                color: "#D1E2E8"
                font.family: "Segoe UI"
                font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
            Text {
                text: root.viewModel ? root.viewModel.validationMessage : ""
                visible: text.length > 0
                color: "#F27683"
                font.family: "Segoe UI"
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.maximumWidth: 250
            }
            StudioButton {
                objectName: "stereoMsResetButton"
                text: "Reset to Default"
                minimumControlWidth: 140
                enabled: root.viewModel && root.viewModel.available
                onClicked: if (root.viewModel) root.viewModel.resetToDefault()
                Accessible.name: "Reset Stereo M/S parameters"
            }
        }

        RowLayout {
            id: visualWells
            objectName: "stereoMsVisualWells"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 150
            spacing: 9

            StereoMsResponseGraph {
                id: responseGraph
                objectName: "stereoMsActiveSurface"
                viewModel: root.viewModel
                Layout.minimumWidth: 260
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 580
            }

            Rectangle {
                id: dynamicWell
                objectName: "stereoMsDynamicWell"
                Layout.minimumWidth: 210
                Layout.preferredWidth: Math.min(330, Math.max(210, visualWells.height - 8))
                Layout.maximumWidth: 350
                Layout.fillHeight: true
                radius: 6
                color: "#081824"
                border.width: 1
                border.color: "#1A3E55"

                // The observational square lives in the middle band; text
                // above and telemetry/status below keep fixed edge positions.
                readonly property real axesBandTop: goniometerTitle.y
                    + goniometerTitle.height + 8
                readonly property real axesBandBottom: telemetryStatus.y - 8

                Text {
                    id: goniometerTitle
                    objectName: "stereoMsGoniometerTitle"
                    x: 12
                    y: 8
                    text: "OUTPUT M/S GONIOMETER"
                    color: "#A1B5C9"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }

                Item {
                    id: axes
                    objectName: "stereoMsStaticAxes"
                    width: Math.max(0, Math.min(parent.width - 30,
                        dynamicWell.axesBandBottom - dynamicWell.axesBandTop))
                    height: width
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: dynamicWell.axesBandTop + Math.max(0,
                        (dynamicWell.axesBandBottom - dynamicWell.axesBandTop - height) / 2)

                    Rectangle {
                        anchors.centerIn: parent
                        width: 1
                        height: parent.height
                        color: "#5F4A9A88"
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width
                        height: 1
                        color: "#5F4A9A88"
                    }
                    Rectangle {
                        width: Math.hypot(parent.width, parent.height)
                        height: 1
                        color: "#324A9A88"
                        anchors.centerIn: parent
                        rotation: 45
                    }
                    Rectangle {
                        width: Math.hypot(parent.width, parent.height)
                        height: 1
                        color: "#324A9A88"
                        anchors.centerIn: parent
                        rotation: -45
                    }
                    Text {
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "M"
                        color: "#A1B5C9"
                        font.family: "Consolas"
                        font.pixelSize: 10
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "S"
                        color: "#A1B5C9"
                        font.family: "Consolas"
                        font.pixelSize: 10
                    }
                    // NO synthetic cloud or independent particle animation.
                    // Output-stage realization-bound telemetry is not yet wired.
                }

                Text {
                    id: telemetryStatus
                    objectName: "stereoMsTelemetryStatus"
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: telemetryRow.top
                    anchors.bottomMargin: 9
                    text: "REAL-TIME TELEMETRY UNAVAILABLE"
                    color: "#9EB6B9"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                }

                RowLayout {
                    id: telemetryRow
                    objectName: "stereoMsTelemetryPlaceholder"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 10
                    spacing: 10
                    Text {
                        text: "CORR  --"
                        color: "#A1B5C9"
                        font.family: "Consolas"
                        font.pixelSize: 10
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "SIDE LOW  -- / --"
                        color: "#A1B5C9"
                        font.family: "Consolas"
                        font.pixelSize: 10
                    }
                }
            }
        }

        Rectangle {
            objectName: "stereoMsControlsRegion"
            Layout.fillWidth: true
            Layout.preferredHeight: 166
            Layout.minimumHeight: 166
            radius: 6
            color: "#12243F"
            border.color: "#2C5A78"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 9
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        StudioNumericField {
                            id: widthNumericField
                            objectName: "stereoMsWidthField"
                            compact: true
                            compactFieldWidth: 86
                            labelText: "WIDTH"
                            unitText: "%"
                            fieldName: "widthPercent"
                            rawText: root.viewModel ? root.viewModel.draftWidthPercent.toFixed(1) : "100.0"
                            viewModel: root.viewModel
                            interactionHint: "Drag the cyan handle vertically, or use slider. Enter to apply."
                        }
                        StudioParameterSlider {
                            objectName: "stereoMsWidthSlider"
                            // Match the NUMERIC BOX only, not its unit suffix or
                            // the freely expanding column (Compressor precedent).
                            Layout.fillWidth: false
                            Layout.alignment: Qt.AlignLeft
                            Layout.minimumWidth: widthNumericField.fieldWidth
                            Layout.preferredWidth: widthNumericField.fieldWidth
                            Layout.maximumWidth: widthNumericField.fieldWidth
                            compact: true
                            from: 0
                            to: 200
                            stepSize: 0.5
                            value: root.viewModel ? root.viewModel.draftWidthPercent : 100
                            viewModel: root.viewModel
                            fieldName: "widthPercent"
                            positionFillColor: root.sliderFill
                            hasSemanticAccent: true
                            semanticAccent: root.widthHue
                            accessibleName: "Broadband stereo width"
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        StudioNumericField {
                            id: cutoffNumericField
                            objectName: "stereoMsCutoffField"
                            compact: true
                            compactFieldWidth: 86
                            labelText: "MONO BASS CUTOFF"
                            unitText: "Hz"
                            fieldName: "monoBassCutoffHz"
                            rawText: root.viewModel ? root.viewModel.monoBassCutoffHz.toFixed(1) : "120.0"
                            viewModel: root.viewModel
                        }
                        StudioParameterSlider {
                            objectName: "stereoMsCutoffSlider"
                            // Match the NUMERIC BOX only, not its unit suffix or
                            // the freely expanding column (Compressor precedent).
                            Layout.fillWidth: false
                            Layout.alignment: Qt.AlignLeft
                            Layout.minimumWidth: cutoffNumericField.fieldWidth
                            Layout.preferredWidth: cutoffNumericField.fieldWidth
                            Layout.maximumWidth: cutoffNumericField.fieldWidth
                            compact: true
                            from: 40
                            to: 300
                            stepSize: 1
                            value: root.viewModel ? root.viewModel.monoBassCutoffHz : 120
                            viewModel: root.viewModel
                            fieldName: "monoBassCutoffHz"
                            positionFillColor: root.sliderFill
                            hasSemanticAccent: true
                            semanticAccent: root.cutoffHue
                            accessibleName: "Mono Bass cutoff frequency"
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        StudioNumericField {
                            id: lowWidthNumericField
                            objectName: "stereoMsLowWidthField"
                            compact: true
                            compactFieldWidth: 86
                            labelText: "LOW WIDTH"
                            unitText: "%"
                            fieldName: "lowBandWidthPercent"
                            rawText: root.viewModel ? root.viewModel.lowBandWidthPercent.toFixed(1) : "100.0"
                            viewModel: root.viewModel
                        }
                        StudioParameterSlider {
                            objectName: "stereoMsLowWidthSlider"
                            // Match the NUMERIC BOX only, not its unit suffix or
                            // the freely expanding column (Compressor precedent).
                            Layout.fillWidth: false
                            Layout.alignment: Qt.AlignLeft
                            Layout.minimumWidth: lowWidthNumericField.fieldWidth
                            Layout.preferredWidth: lowWidthNumericField.fieldWidth
                            Layout.maximumWidth: lowWidthNumericField.fieldWidth
                            compact: true
                            from: 0
                            to: 100
                            stepSize: 1
                            value: root.viewModel ? root.viewModel.lowBandWidthPercent : 100
                            viewModel: root.viewModel
                            fieldName: "lowBandWidthPercent"
                            positionFillColor: root.sliderFill
                            hasSemanticAccent: true
                            semanticAccent: root.lowHue
                            accessibleName: "Low-frequency Side width"
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#304E64"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    StudioNumericField {
                        objectName: "stereoMsMidGainField"
                        compact: true
                        compactFieldWidth: 78
                        labelText: "MID GAIN"
                        unitText: "dB"
                        fieldName: "midGainDb"
                        rawText: root.viewModel ? root.viewModel.midGainDb.toFixed(1) : "0.0"
                        viewModel: root.viewModel
                    }
                    StudioNumericField {
                        objectName: "stereoMsSideGainField"
                        compact: true
                        compactFieldWidth: 78
                        labelText: "SIDE GAIN"
                        unitText: "dB"
                        fieldName: "sideGainDb"
                        rawText: root.viewModel ? root.viewModel.sideGainDb.toFixed(1) : "0.0"
                        viewModel: root.viewModel
                    }

                    ColumnLayout {
                        spacing: 3
                        Text {
                            text: "MONO BASS"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        RowLayout {
                            spacing: 3
                            Repeater {
                                model: ["OFF", "LR12", "LR24"]
                                delegate: StudioSegmentButton {
                                    required property string modelData
                                    text: modelData
                                    minimumControlWidth: 58
                                    selected: root.viewModel && root.viewModel.monoBassMode === modelData
                                    accentColor: root.seaGreen
                                    enabled: root.viewModel && root.viewModel.available
                                    onClicked: if (root.viewModel
                                            && root.viewModel.setDraftMonoBassMode(modelData))
                                        root.viewModel.commitDraft()
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    StudioToggle {
                        objectName: "stereoMsSideMutedToggle"
                        text: "Side Muted"
                        checked: root.viewModel ? root.viewModel.sideMuted : false
                        enabled: root.viewModel && root.viewModel.available
                        onClicked: {
                            if (root.viewModel && root.viewModel.setDraftSideMuted(checked))
                                root.viewModel.commitDraft()
                        }
                    }
                }
            }
        }
    }
}
