import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var viewModel: null

    implicitWidth: 992
    implicitHeight: 612

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // Header Strip (48 lp)
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            spacing: 12

            ColumnLayout {
                spacing: 1
                Text {
                    text: "Parametric EQ"
                    color: "#F5F8FC"
                    font.family: "Segoe UI"
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "Manual Mastering"
                    color: "#A1B5C9"
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                }
            }

            Item { Layout.fillWidth: true }

            // Preview Status Badge
            Rectangle {
                Layout.preferredHeight: 28
                Layout.preferredWidth: statusText.implicitWidth + 24
                radius: 6
                color: {
                    if (!root.viewModel) return "#0F2236"
                    const status = root.viewModel.previewStatus
                    if (status === "RENDERING") return "#2A2814"
                    if (status === "READY") return "#1400D47A"
                    if (status === "ERROR") return "#14F27683"
                    return "#0F2236"
                }
                border.color: {
                    if (!root.viewModel) return "#27465F"
                    const status = root.viewModel.previewStatus
                    if (status === "RENDERING") return "#F2B632"
                    if (status === "READY") return "#00D47A"
                    if (status === "ERROR") return "#F27683"
                    return "#27465F"
                }
                border.width: 1

                Text {
                    id: statusText
                    anchors.centerIn: parent
                    text: {
                        if (!root.viewModel) return "NO PREVIEW"
                        const status = root.viewModel.previewStatus
                        if (status === "NO_PREPARED_REALIZATION" || status === "IDLE") return "NO PREVIEW"
                        return status
                    }
                    color: {
                        if (!root.viewModel) return "#A1B5C9"
                        const status = root.viewModel.previewStatus
                        if (status === "RENDERING") return "#F2B632"
                        if (status === "READY") return "#00D47A"
                        if (status === "ERROR") return "#F27683"
                        return "#A1B5C9"
                    }
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
            }

            // A/B Controls (32 lp)
            RowLayout {
                spacing: 2

                StudioButton {
                    objectName: "abButtonActive"
                    text: "A: EQ Active"
                    selected: root.viewModel ? !root.viewModel.bypass : true
                    tone: "primary"
                    accentColor: "#00C8FF"
                    onClicked: if (root.viewModel) root.viewModel.setBypass(false)
                    Accessible.name: "A: EQ Active"
                    ToolTip.text: "Activate Parametric EQ processing"
                    ToolTip.visible: hovered
                }

                StudioButton {
                    objectName: "abButtonBypass"
                    text: "B: Bypass"
                    selected: root.viewModel ? root.viewModel.bypass : false
                    tone: "gold"
                    accentColor: "#F2B632"
                    onClicked: if (root.viewModel) root.viewModel.setBypass(true)
                    Accessible.name: "B: Bypass"
                    ToolTip.text: "Bypass Parametric EQ module"
                    ToolTip.visible: hovered
                }
            }
        }

        // Band Selector Strip (40 lp)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#12243F"
            border.color: "#2C5A78"
            border.width: 1
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Repeater {
                    model: root.viewModel ? root.viewModel.bandSummaries : []
                    delegate: StudioButton {
                        required property var modelData
                        required property int index

                        readonly property bool isSelectedBand: root.viewModel !== null && root.viewModel.selectedIndex === index
                        readonly property bool isEnabledBand: modelData.enabled

                        objectName: "bandSelectorButton_" + index
                        text: (index + 1) + " " + modelData.filter.replace("_", " ")
                        selected: isSelectedBand
                        tone: isSelectedBand ? "primary" : "secondary"
                        minimumControlWidth: 90
                        contentPadding: 8
                        activeFocusOnTab: true
                        opacity: isEnabledBand ? 1.0 : 0.5

                        onClicked: if (root.viewModel) root.viewModel.selectBand(index)

                        Accessible.name: "Band " + (index + 1) + " " + modelData.filter.replace("_", " ") + (isEnabledBand ? "" : " (disabled)")
                        ToolTip.text: "Select Band " + (index + 1) + " (" + modelData.filter.replace("_", " ") + ", " + modelData.routing + ")"
                        ToolTip.visible: hovered
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: "#2C5A78" }

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
        }

        // Response Graph (Flexible 324 lp default / 244 lp min)
        ParametricEqGraph {
            objectName: "parametricEqGraph"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 244
            viewModel: root.viewModel
        }

        // Selected-Band Inspector Panel (128 lp Fixed)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 128
            color: "#12243F"
            border.color: "#2C5A78"
            border.width: 1
            radius: 6

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 6
                anchors.bottomMargin: 8
                spacing: 4

                // Filter & Routing Row
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    spacing: 8

                    // Left Column: Band Enabled
                    StudioToggle {
                        objectName: "bandEnabledToggle"
                        text: "Band " + (root.viewModel ? root.viewModel.selectedIndex + 1 : 1) + " Enabled"
                        checked: root.viewModel ? root.viewModel.enabled : true
                        onClicked: if (root.viewModel) root.viewModel.setEnabled(checked)
                        Accessible.name: "Enable or disable band"
                    }

                    Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: "#2C5A78" }

                    // Filter Selection (6 segments x 96 lp)
                    RowLayout {
                        spacing: 1
                        Text { text: "FILTER:"; color: "#A1B5C9"; font.family: "Segoe UI"; font.pixelSize: 12; font.weight: Font.Bold; Layout.rightMargin: 4 }
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
                                minimumControlWidth: 96
                                contentPadding: 4
                                selected: root.viewModel && root.viewModel.filter === modelData.token
                                tone: root.viewModel && root.viewModel.filter === modelData.token ? "primary" : "secondary"
                                onClicked: if (root.viewModel) root.viewModel.setFilter(modelData.token)
                                Accessible.name: "Filter type " + modelData.label
                                ToolTip.text: "Set filter type to " + modelData.label
                                ToolTip.visible: hovered
                            }
                        }
                    }

                    Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: "#2C5A78" }

                    // Routing Selection (5 segments x 80 lp)
                    RowLayout {
                        spacing: 1
                        Text { text: "ROUTING:"; color: "#A1B5C9"; font.family: "Segoe UI"; font.pixelSize: 12; font.weight: Font.Bold; Layout.rightMargin: 4 }
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
                                minimumControlWidth: 80
                                contentPadding: 4
                                enabled: modelData.token === "STEREO" || (root.viewModel && root.viewModel.routeAvailable)
                                selected: root.viewModel && root.viewModel.routing === modelData.token
                                tone: root.viewModel && root.viewModel.routing === modelData.token ? "primary" : "secondary"
                                onClicked: if (root.viewModel) root.viewModel.setRouting(modelData.token)
                                Accessible.name: "Routing " + modelData.label
                                ToolTip.text: "Set channel routing to " + modelData.label
                                ToolTip.visible: hovered
                            }
                        }

                        // Mixed Routing Badge (164 x 28 lp)
                        Rectangle {
                            visible: root.viewModel !== null && root.viewModel !== undefined && root.viewModel.mixedRouting
                            Layout.preferredHeight: 28
                            Layout.preferredWidth: 164
                            radius: 8
                            color: "#336C4EA6"
                            border.color: "#B3A989F2"
                            border.width: 1

                            Text {
                                id: mixedText
                                objectName: "mixedText"
                                anchors.centerIn: parent
                                text: "MIXED ROUTING ACTIVE"
                                color: "#EEE9DCFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1C3A59" }

                // Numeric Fields Tier (50 lp)
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
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

                    // HP/LP Slope DB/OCT (6 segments x 80 lp)
                    RowLayout {
                        visible: root.viewModel ? root.viewModel.slopeApplicable : false
                        spacing: 1
                        Text { text: "SLOPE:"; color: "#A1B5C9"; font.family: "Segoe UI"; font.pixelSize: 12; font.weight: Font.Bold; Layout.rightMargin: 4 }
                        Repeater {
                            model: [6, 12, 18, 24, 36, 48]
                            delegate: StudioButton {
                                required property int modelData
                                objectName: "slopeButton_" + modelData
                                text: modelData + " dB"
                                minimumControlWidth: 80
                                contentPadding: 4
                                selected: root.viewModel && root.viewModel.slopeDbPerOct === modelData
                                tone: root.viewModel && root.viewModel.slopeDbPerOct === modelData ? "primary" : "secondary"
                                onClicked: {
                                    if (root.viewModel) {
                                        root.viewModel.setDraftSlopeDbPerOct(modelData)
                                        root.viewModel.commitDraft()
                                    }
                                }
                                Accessible.name: "Slope " + modelData + " dB per octave"
                                ToolTip.text: "Set filter slope to " + modelData + " dB/octave"
                                ToolTip.visible: hovered
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Validation & Preview Error Strip (24 lp)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
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
                font.pixelSize: 12
                visible: text.length > 0
                elide: Text.ElideRight
            }
        }
    }
}
