import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var viewModel: null

    color: "#081218"
    border.color: "#2A3947"
    border.width: 1
    radius: 6

    implicitWidth: 1000
    implicitHeight: 620

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Header Strip
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                spacing: 1
                Text {
                    text: "Parametric EQ"
                    color: "#E6EEF0"
                    font.family: "Segoe UI"
                    font.pixelSize: 18
                    font.weight: Font.Bold
                }
                Text {
                    text: "Manual Mastering"
                    color: "#8A97A3"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                }
            }

            Item { Layout.fillWidth: true }

            // Status Badge
            Rectangle {
                Layout.preferredHeight: 26
                Layout.preferredWidth: statusText.width + 16
                radius: 4
                color: {
                    if (!root.viewModel) return "#1A2228"
                    const status = root.viewModel.previewStatus
                    if (status === "RENDERING") return "#2A2814"
                    if (status === "READY") return "#0E2B20"
                    if (status === "ERROR") return "#3A141A"
                    return "#1A2228"
                }
                border.color: {
                    if (!root.viewModel) return "#2A3947"
                    const status = root.viewModel.previewStatus
                    if (status === "RENDERING") return "#F2B632"
                    if (status === "READY") return "#00E6E6"
                    if (status === "ERROR") return "#F27683"
                    return "#2A3947"
                }

                Text {
                    id: statusText
                    anchors.centerIn: parent
                    text: root.viewModel ? root.viewModel.previewStatus : "NO_PREPARED_REALIZATION"
                    color: {
                        if (!root.viewModel) return "#8A97A3"
                        const status = root.viewModel.previewStatus
                        if (status === "RENDERING") return "#F2B632"
                        if (status === "READY") return "#00E6E6"
                        if (status === "ERROR") return "#F27683"
                        return "#8A97A3"
                    }
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }

            // A/B Controls
            RowLayout {
                spacing: 2

                StudioButton {
                    objectName: "abButtonActive"
                    text: "A: EQ Active"
                    tone: !root.viewModel || !root.viewModel.bypass ? "primary" : "secondary"
                    onClicked: if (root.viewModel) root.viewModel.setBypass(false)
                    Accessible.name: "A: EQ Active"
                }

                StudioButton {
                    objectName: "abButtonBypass"
                    text: "B: Bypass"
                    tone: root.viewModel && root.viewModel.bypass ? "warning" : "secondary"
                    onClicked: if (root.viewModel) root.viewModel.setBypass(true)
                    Accessible.name: "B: Bypass"
                }
            }
        }

        // Band Selector Strip
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "BANDS"
                color: "#8A97A3"
                font.family: "Segoe UI"
                font.pixelSize: 10
                font.weight: Font.Bold
            }

            Repeater {
                model: root.viewModel ? root.viewModel.bandSummaries : []
                delegate: Rectangle {
                    required property var modelData
                    required property int index

                    readonly property bool isSelected: root.viewModel !== null && root.viewModel.selectedIndex === index
                    readonly property bool isEnabled: modelData.enabled

                    Layout.preferredWidth: 90
                    Layout.preferredHeight: 32
                    radius: 4
                    color: isSelected ? "#8B42C0" : (isEnabled ? "#0F1820" : "#0A1015")
                    border.color: isSelected ? "#E6EEF0" : "#2A3947"
                    border.width: isSelected ? 2 : 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 4

                        Text {
                            text: index + 1
                            color: isSelected ? "#FFFFFF" : (isEnabled ? "#00C8FF" : "#586773")
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.filter.replace("_", " ")
                            color: isSelected ? "#E6EEF0" : (isEnabled ? "#8A97A3" : "#485866")
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: if (root.viewModel) root.viewModel.selectBand(index)
                    }
                }
            }

            Item { Layout.fillWidth: true }

            StudioButton {
                objectName: "addBandButton"
                text: "+ Add Band"
                enabled: root.viewModel !== null && root.viewModel !== undefined && root.viewModel.addAvailable
                onClicked: if (root.viewModel) root.viewModel.addBand()
                Accessible.name: "Add Band"
            }

            StudioButton {
                objectName: "removeBandButton"
                text: "- Remove Band"
                enabled: root.viewModel !== null && root.viewModel !== undefined && root.viewModel.removeAvailable
                onClicked: if (root.viewModel) root.viewModel.removeSelectedBand()
                Accessible.name: "Remove Band"
            }
        }

        // Response Graph
        ParametricEqGraph {
            objectName: "parametricEqGraph"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 240
            viewModel: root.viewModel
        }

        // Selected-Band Inspector Panel
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            color: "#0F1820"
            border.color: "#2A3947"
            radius: 5

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Row 1: Filter Type & Routing
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    StudioToggle {
                        objectName: "bandEnabledToggle"
                        text: "Band " + (root.viewModel ? root.viewModel.selectedIndex + 1 : 1) + " Enabled"
                        checked: root.viewModel ? root.viewModel.enabled : true
                        onClicked: if (root.viewModel) root.viewModel.setEnabled(checked)
                        Accessible.name: "Enable or disable band"
                    }

                    Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: "#2A3947" }

                    RowLayout {
                        spacing: 4
                        Text { text: "FILTER:"; color: "#8A97A3"; font.family: "Segoe UI"; font.pixelSize: 10; font.weight: Font.Bold }
                        Repeater {
                            model: [
                                { label: "Bell", token: "BELL" },
                                { label: "Notch", token: "NOTCH" },
                                { label: "Low Shelf", token: "LOW_SHELF" },
                                { label: "High Shelf", token: "HIGH_SHELF" },
                                { label: "High Pass", token: "HIGH_PASS" },
                                { label: "Low Pass", token: "LOW_PASS" }
                            ]
                            delegate: StudioButton {
                                required property var modelData
                                objectName: "filterButton_" + modelData.token
                                text: modelData.label
                                tone: root.viewModel && root.viewModel.filter === modelData.token ? "primary" : "secondary"
                                onClicked: if (root.viewModel) root.viewModel.setFilter(modelData.token)
                                Accessible.name: "Filter type " + modelData.label
                            }
                        }
                    }

                    Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: "#2A3947" }

                    RowLayout {
                        spacing: 4
                        Text { text: "ROUTING:"; color: "#8A97A3"; font.family: "Segoe UI"; font.pixelSize: 10; font.weight: Font.Bold }
                        Repeater {
                            model: [
                                { label: "Stereo", token: "STEREO" },
                                { label: "Mid", token: "MID" },
                                { label: "Side", token: "SIDE" },
                                { label: "Left", token: "LEFT" },
                                { label: "Right", token: "RIGHT" }
                            ]
                            delegate: StudioButton {
                                required property var modelData
                                objectName: "routingButton_" + modelData.token
                                text: modelData.label
                                enabled: modelData.token === "STEREO" || (root.viewModel && root.viewModel.routeAvailable)
                                tone: root.viewModel && root.viewModel.routing === modelData.token ? "primary" : "secondary"
                                onClicked: if (root.viewModel) root.viewModel.setRouting(modelData.token)
                                Accessible.name: "Routing " + modelData.label
                            }
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1A2834" }

                // Row 2: Numeric Fields
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    StudioNumericField {
                        objectName: "frequencyField"
                        labelText: "FREQUENCY"
                        unitText: "Hz"
                        fieldName: "frequency"
                        rawText: root.viewModel ? root.viewModel.frequencyText : "1000"
                        viewModel: root.viewModel
                    }

                    StudioNumericField {
                        objectName: "gainField"
                        visible: root.viewModel ? root.viewModel.gainApplicable : true
                        labelText: "GAIN"
                        unitText: "dB"
                        fieldName: "gain"
                        rawText: root.viewModel ? root.viewModel.gainText : "0"
                        viewModel: root.viewModel
                    }

                    StudioNumericField {
                        objectName: "qField"
                        visible: root.viewModel ? root.viewModel.qApplicable : true
                        labelText: "Q"
                        unitText: ""
                        fieldName: "q"
                        rawText: root.viewModel ? root.viewModel.qText : "0.707"
                        viewModel: root.viewModel
                    }

                    StudioNumericField {
                        objectName: "shelfSlopeField"
                        visible: root.viewModel ? root.viewModel.shelfSlopeApplicable : false
                        labelText: "SHELF SLOPE"
                        unitText: ""
                        fieldName: "shelfSlope"
                        rawText: root.viewModel ? root.viewModel.shelfSlopeText : "1"
                        viewModel: root.viewModel
                    }

                    // Slope dB/oct buttons for High Pass / Low Pass
                    RowLayout {
                        visible: root.viewModel ? root.viewModel.slopeApplicable : false
                        spacing: 4
                        Text { text: "SLOPE:"; color: "#8A97A3"; font.family: "Segoe UI"; font.pixelSize: 10; font.weight: Font.Bold }
                        Repeater {
                            model: [6, 12, 18, 24, 36, 48]
                            delegate: StudioButton {
                                required property int modelData
                                objectName: "slopeButton_" + modelData
                                text: modelData + " dB"
                                tone: root.viewModel && root.viewModel.slopeDbPerOct === modelData ? "primary" : "secondary"
                                onClicked: if (root.viewModel) root.viewModel.setDraftSlopeDbPerOct(modelData)
                                Accessible.name: "Slope " + modelData + " dB per octave"
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Validation & Preview Error Message Strip
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            color: "transparent"

            Text {
                id: messageText
                objectName: "validationMessageText"
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    if (!root.viewModel) return ""
                    if (root.viewModel.validationMessage.length > 0) return root.viewModel.validationMessage
                    if (root.viewModel.previewStatus === "ERROR") return root.viewModel.previewError
                    return ""
                }
                color: "#F27683"
                font.family: "Segoe UI"
                font.pixelSize: 11
                visible: text.length > 0
            }
        }
    }
}
