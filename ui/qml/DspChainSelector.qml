import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspChainSelector"

    implicitWidth: 180
    implicitHeight: 300

    property int selectedIndex: 0
    property var gainViewModel: null
    property var eqViewModel: null

    color: "#0B1622"
    border.color: "#1E354A"
    border.width: 1
    radius: 6

    // Header label
    Rectangle {
        id: chainHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 28
        color: "#0F2030"
        border.color: "#1E354A"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Text {
                text: "DSP CHAIN"
                color: "#8A9EA8"
                font.family: "Segoe UI"
                font.pixelSize: 10
                font.weight: Font.Bold
            }
        }
    }

    ScrollView {
        id: chainScrollView
        anchors.top: chainHeader.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 4
        clip: true

        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: chainScrollView.availableWidth
            spacing: 4

            // Row 0 — Input Gain
            Rectangle {
                id: gainRow
                objectName: "dspChainRow_0"
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                radius: 4

                property bool isSelected: root.selectedIndex === 0
                property bool isDefaultState: root.gainViewModel ? (root.gainViewModel.gainDb === 0.0) : true
                property bool isBypassed: root.gainViewModel ? root.gainViewModel.bypass : false
                property bool hasError: root.gainViewModel ? (root.gainViewModel.previewStatus === "ERROR") : false

                color: isSelected ? "#1A324A" : (gainRowMouse.containsMouse ? "#122538" : "#0D1D2B")
                border.color: isSelected ? "#00C8FF" : "#1A324A"
                border.width: isSelected ? 2 : 1

                activeFocusOnTab: true

                MouseArea {
                    id: gainRowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.selectedIndex = 0
                        gainRow.forceActiveFocus()
                    }
                }

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                        root.selectedIndex = 0
                        event.accepted = true
                    } else if (event.key === Qt.Key_Down) {
                        root.selectedIndex = 1
                        eqRow.forceActiveFocus()
                        event.accepted = true
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    // Configuration Indicator LED
                    Rectangle {
                        objectName: "dspChainConfigLed_0"
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: gainRow.isDefaultState ? "#273A4D" : "#00D47A"
                        border.color: gainRow.isDefaultState ? "#3A4D60" : "#00FF94"
                        border.width: 1

                        ToolTip.text: gainRow.isDefaultState ? "0.0 dB Default" : "Manual Non-Default Gain"
                        ToolTip.visible: gainLedMouse.containsMouse

                        MouseArea {
                            id: gainLedMouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: "Input Gain"
                            color: gainRow.isSelected ? "#F5F8FC" : "#C4D4E0"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            objectName: "dspChainStateText_0"
                            text: {
                                if (!root.gainViewModel) return "0.0 dB Default"
                                if (gainRow.isDefaultState) return "0.0 dB Default"
                                const val = root.gainViewModel.gainDb
                                const prefix = val > 0 ? "+" : ""
                                return prefix + root.gainViewModel.gainDbText + " dB Manual"
                            }
                            color: "#7A8E9E"
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                            visible: true
                        }
                    }

                    // Bypass Badge ("BYP")
                    Rectangle {
                        objectName: "dspChainBypassBadge_0"
                        visible: gainRow.isBypassed
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 16
                        radius: 3
                        color: "#3A2A0D"
                        border.color: "#F2B632"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "BYP"
                            color: "#F2B632"
                            font.family: "Segoe UI"
                            font.pixelSize: 8
                            font.weight: Font.Bold
                        }
                    }

                    // Error Indicator
                    Rectangle {
                        objectName: "dspChainErrorBadge_0"
                        visible: gainRow.hasError
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: "#F27683"
                    }
                }
            }

            // Row 1 — Parametric EQ
            Rectangle {
                id: eqRow
                objectName: "dspChainRow_1"
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                radius: 4

                property bool isSelected: root.selectedIndex === 1
                property bool isDefaultState: root.eqViewModel ? root.eqViewModel.isDefault : true
                property bool isBypassed: root.eqViewModel ? root.eqViewModel.bypass : false
                property bool hasError: root.eqViewModel ? (root.eqViewModel.previewStatus === "ERROR") : false

                color: isSelected ? "#1A324A" : (eqRowMouse.containsMouse ? "#122538" : "#0D1D2B")
                border.color: isSelected ? "#00C8FF" : "#1A324A"
                border.width: isSelected ? 2 : 1

                activeFocusOnTab: true

                MouseArea {
                    id: eqRowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.selectedIndex = 1
                        eqRow.forceActiveFocus()
                    }
                }

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                        root.selectedIndex = 1
                        event.accepted = true
                    } else if (event.key === Qt.Key_Up) {
                        root.selectedIndex = 0
                        gainRow.forceActiveFocus()
                        event.accepted = true
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    // Configuration Indicator LED
                    Rectangle {
                        objectName: "dspChainConfigLed_1"
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: eqRow.isDefaultState ? "#273A4D" : "#00D47A"
                        border.color: eqRow.isDefaultState ? "#3A4D60" : "#00FF94"
                        border.width: 1

                        ToolTip.text: eqRow.isDefaultState ? "Canonical Default / Flat" : "Manual Non-Default"
                        ToolTip.visible: eqLedMouse.containsMouse

                        MouseArea {
                            id: eqLedMouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: "Parametric EQ"
                            color: eqRow.isSelected ? "#F5F8FC" : "#C4D4E0"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            objectName: "dspChainStateText_1"
                            text: eqRow.isDefaultState ? "Flat Default" : "Manual Edit"
                            color: "#7A8E9E"
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                            visible: true
                        }
                    }

                    // Bypass Badge ("BYP")
                    Rectangle {
                        objectName: "dspChainBypassBadge_1"
                        visible: eqRow.isBypassed
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 16
                        radius: 3
                        color: "#3A2A0D"
                        border.color: "#F2B632"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "BYP"
                            color: "#F2B632"
                            font.family: "Segoe UI"
                            font.pixelSize: 8
                            font.weight: Font.Bold
                        }
                    }

                    // Error Indicator
                    Rectangle {
                        objectName: "dspChainErrorBadge_1"
                        visible: eqRow.hasError
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: "#F27683"
                    }
                }
            }
        }
    }
}
