import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var viewModel: null

    implicitWidth: 992
    implicitHeight: 612

    color: "transparent"

    readonly property bool isCompactActionMode: width < 916

    readonly property var bandColors: [
        "#2ED3FF", // 1 Cyan
        "#2FD98F", // 2 Emerald
        "#FFD84A", // 3 Warm Yellow
        "#FF6B6B", // 4 Coral Red
        "#4F7CFF", // 5 Cobalt Blue
        "#F5F8FC"  // 6 Neutral White
    ]

    function getFilterAbbrev(token) {
        if (token === "BELL") return "BELL"
        if (token === "NOTCH") return "NOTCH"
        if (token === "LOW_SHELF") return "LS"
        if (token === "HIGH_SHELF") return "HS"
        if (token === "HIGH_PASS") return "HP"
        if (token === "LOW_PASS") return "LP"
        return token
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        // Top Toolbar Strip (Reset Flat & Overall)
        RowLayout {
            objectName: "eqTopToolbarRegion"
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            spacing: 8

            Item { Layout.fillWidth: true }

            StudioButton {
                objectName: "eqResetFlatButton"
                text: "Reset Flat"
                minimumControlWidth: 88
                enabled: root.viewModel !== null && root.viewModel !== undefined
                onClicked: if (root.viewModel) root.viewModel.resetToFlat()
                Accessible.name: "Reset EQ to Flat"
                ToolTip.text: "Reset all EQ parameters to Flat baseline"
                ToolTip.visible: hovered
            }

            StudioButton {
                objectName: "eqOverallToggleButton"
                text: "Overall"
                selected: root.viewModel ? root.viewModel.showCombinedResponse : false
                tone: root.viewModel && root.viewModel.showCombinedResponse ? "primary" : "secondary"
                accentColor: "#A989F2"
                minimumControlWidth: 72
                enabled: root.viewModel !== null && root.viewModel !== undefined
                onClicked: {
                    if (root.viewModel) {
                        root.viewModel.showCombinedResponse = !root.viewModel.showCombinedResponse
                    }
                }
                Accessible.name: "Toggle Overall combined response curve"
                ToolTip.text: "Show or hide the combined total response curve of all active EQ bands"
                ToolTip.visible: hovered
            }
        }

        // Band Selector Strip (40 lp)
        Rectangle {
            objectName: "eqBandStripRegion"
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
                    model: root.viewModel ? root.viewModel.bandCount : 0
                    delegate: StudioButton {
                        required property int index

                        readonly property var bandSummary: (root.viewModel && root.viewModel.bandSummaries && index < root.viewModel.bandSummaries.length) ? root.viewModel.bandSummaries[index] : null
                        readonly property bool isSelectedBand: root.viewModel !== null && root.viewModel.selectedIndex === index
                        readonly property bool isEnabledBand: bandSummary ? bandSummary.enabled : true
                        readonly property string filterType: bandSummary ? bandSummary.filter : "BELL"
                        readonly property string routingType: bandSummary ? bandSummary.routing : "STEREO"
                        readonly property color bandHue: root.bandColors[index % root.bandColors.length]

                        objectName: "bandSelectorButton_" + index
                        text: (index + 1) + " " + root.getFilterAbbrev(filterType)
                        selected: isSelectedBand
                        tone: isSelectedBand ? "primary" : "secondary"
                        accentColor: bandHue
                        minimumControlWidth: 90
                        contentPadding: 8
                        activeFocusOnTab: true
                        opacity: isEnabledBand ? 1.0 : 0.5

                        onClicked: if (root.viewModel) root.viewModel.selectBand(index)

                        Accessible.name: "Band " + (index + 1) + " " + filterType.replace("_", " ") + (isEnabledBand ? "" : " (disabled)")
                        ToolTip.text: "Select Band " + (index + 1) + " (" + filterType.replace("_", " ") + ", " + routingType + ")"
                        ToolTip.visible: hovered
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: "#2C5A78" }

                StudioButton {
                    objectName: "addBandButton"
                    text: root.isCompactActionMode ? "+ Add" : "+ Add Band"
                    minimumControlWidth: root.isCompactActionMode ? 96 : 128
                    enabled: root.viewModel !== null && root.viewModel !== undefined && root.viewModel.addAvailable
                    onClicked: if (root.viewModel) root.viewModel.addBand()
                    Accessible.name: "Add Band"
                }

                StudioButton {
                    objectName: "removeBandButton"
                    text: root.isCompactActionMode ? "- Remove" : "- Remove Band"
                    minimumControlWidth: root.isCompactActionMode ? 96 : 128
                    enabled: root.viewModel !== null && root.viewModel !== undefined && root.viewModel.removeAvailable
                    onClicked: if (root.viewModel) root.viewModel.removeSelectedBand()
                    Accessible.name: "Remove Band"
                }
            }
        }

        // Response Graph (Flexible min 200 lp)
        ParametricEqGraph {
            objectName: "parametricEqGraph"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.isCompactActionMode ? 170 : 200
            viewModel: root.viewModel
        }

        // Selected-Band Inspector Panel (128 lp Fixed)
        Rectangle {
            objectName: "eqInspectorRegion"
            Layout.fillWidth: true
            Layout.preferredHeight: root.isCompactActionMode ? 118 : 128
            color: "#12243F"
            border.color: "#2C5A78"
            border.width: 1
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 6
                anchors.bottomMargin: 8
                spacing: 8

                // Left Column (160 lp): Band Enabled Toggle
                ColumnLayout {
                    Layout.preferredWidth: root.isCompactActionMode ? 112 : 160
                    Layout.fillHeight: true
                    spacing: 4

                    StudioToggle {
                        objectName: "bandEnabledToggle"
                        text: "Band " + (root.viewModel ? root.viewModel.selectedIndex + 1 : 1) + " Enabled"
                        checked: root.viewModel ? root.viewModel.enabled : true
                        onClicked: if (root.viewModel) root.viewModel.setEnabled(checked)
                        Accessible.name: "Enable or disable band"
                    }

                    Item { Layout.fillHeight: true }
                }

                Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#2C5A78" }

                // Right Column: Two Subrows + Numeric Tier
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 4

                    // Subrow 1: Filter Selection (581 lp group)
                    RowLayout {
                        objectName: "eqFilterGroup"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        spacing: 8

                        Text {
                            text: "FILTER:"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            Layout.preferredWidth: 56
                        }

                        RowLayout {
                            spacing: 1
                            Repeater {
                                model: [
                                    { label: "Bell", token: "BELL" },
                                    { label: "Notch", token: "NOTCH" },
                                    { label: "Low Shelf", token: "LOW_SHELF" },
                                    { label: "High Shelf", token: "HIGH_SHELF" },
                                    { label: "High Pass", token: "HIGH_PASS" },
                                    { label: "Low Pass", token: "LOW_PASS" }
                                ]
                                delegate: StudioSegmentButton {
                                    required property var modelData
                                    objectName: "filterButton_" + modelData.token
                                    text: modelData.label
                                    minimumControlWidth: root.isCompactActionMode ? 66 : 96
                                    contentPadding: root.isCompactActionMode ? 2 : 4
                                    selected: root.viewModel && root.viewModel.filter === modelData.token
                                    tone: root.viewModel && root.viewModel.filter === modelData.token ? "primary" : "secondary"
                                    onClicked: if (root.viewModel) root.viewModel.setFilter(modelData.token)
                                    Accessible.name: "Filter type " + modelData.label
                                    ToolTip.text: "Set filter type to " + modelData.label
                                    ToolTip.visible: hovered
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // Subrow 2: Routing Selection (404 lp group) + Mixed Badge (164 lp)
                    RowLayout {
                        objectName: "eqRoutingGroup"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        spacing: 8

                        Text {
                            text: "ROUTING:"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            Layout.preferredWidth: 56
                        }

                        RowLayout {
                            spacing: 1
                            Repeater {
                                model: [
                                    { label: "Stereo", token: "STEREO" },
                                    { label: "Mid", token: "MID" },
                                    { label: "Side", token: "SIDE" },
                                    { label: "Left", token: "LEFT" },
                                    { label: "Right", token: "RIGHT" }
                                ]
                                delegate: StudioSegmentButton {
                                    required property var modelData
                                    objectName: "routingButton_" + modelData.token
                                    text: modelData.label
                                    minimumControlWidth: root.isCompactActionMode ? 68 : 80
                                    contentPadding: root.isCompactActionMode ? 2 : 4
                                    enabled: modelData.token === "STEREO" || (root.viewModel && root.viewModel.routeAvailable)
                                    selected: root.viewModel && root.viewModel.routing === modelData.token
                                    tone: root.viewModel && root.viewModel.routing === modelData.token ? "primary" : "secondary"
                                    onClicked: if (root.viewModel) root.viewModel.setRouting(modelData.token)
                                    Accessible.name: "Routing " + modelData.label
                                    ToolTip.text: "Set channel routing to " + modelData.label
                                    ToolTip.visible: hovered
                                }
                            }
                        }

                        // Mixed Routing Badge (164 x 28 lp)
                        Rectangle {
                            objectName: "mixedRoutingBadge"
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

                        Item { Layout.fillWidth: true }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1C3A59" }

                    // Numeric Fields Tier (50 lp)
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.isCompactActionMode ? 46 : 50
                        spacing: root.isCompactActionMode ? 8 : 16

                        StudioNumericField {
                            objectName: "frequencyField"
                            compact: root.isCompactActionMode
                            labelText: "FREQUENCY"
                            unitText: "Hz"
                            fieldName: "frequency"
                            rawText: root.viewModel ? root.viewModel.frequencyText : "1000"
                            viewModel: root.viewModel
                        }

                        StudioNumericField {
                            objectName: "gainField"
                            compact: root.isCompactActionMode
                            visible: root.viewModel ? root.viewModel.gainApplicable : true
                            labelText: "GAIN"
                            unitText: "dB"
                            fieldName: "gain"
                            rawText: root.viewModel ? root.viewModel.gainText : "0"
                            viewModel: root.viewModel
                        }

                        StudioNumericField {
                            objectName: "qField"
                            compact: root.isCompactActionMode
                            visible: root.viewModel ? root.viewModel.qApplicable : true
                            labelText: "Q"
                            unitText: ""
                            fieldName: "q"
                            rawText: root.viewModel ? root.viewModel.qText : "0.707"
                            viewModel: root.viewModel
                        }

                        StudioNumericField {
                            objectName: "shelfSlopeField"
                            compact: root.isCompactActionMode
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
                                delegate: StudioSegmentButton {
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
        }

        // Validation & Preview Error Strip (24 lp)
        Rectangle {
            objectName: "eqStatusRegion"
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

                ToolTip.text: text
                ToolTip.visible: messageMouseArea.containsMouse && text.length > 0

                Accessible.name: text
                Accessible.description: text

                MouseArea {
                    id: messageMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
        }
    }
}
