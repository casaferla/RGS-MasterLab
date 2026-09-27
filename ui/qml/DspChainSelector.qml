import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspChainSelector"

    implicitWidth: isCompact ? 164 : 180
    implicitHeight: isCompact ? 36 : 300

    property int selectedIndex: 0
    property var eqViewModel: null
    property bool isCompact: false

    color: "#0B1622"
    border.color: "#1E354A"
    border.width: 1
    radius: 6

    // Header label (Standard mode)
    Rectangle {
        id: chainHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 28
        color: "#0F2030"
        border.color: "#1E354A"
        border.width: 1
        visible: !root.isCompact

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            Text {
                text: "DSP CHAIN"
                color: "#8A9EA8"
                font.family: "Segoe UI"
                font.pixelSize: 10
                font.weight: Font.Bold
            }
        }
    }

    // ScrollView (Standard mode) or Grid Container (Compact mode)
    Item {
        id: chainContent
        anchors.top: root.isCompact ? parent.top : chainHeader.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.isCompact ? 2 : 4

        // Parametric EQ Row
        Rectangle {
            id: eqRow
            objectName: "dspChainRow_0"
            anchors.fill: parent
            radius: 4

            property bool isSelected: root.selectedIndex === 0
            property bool isDefaultState: root.eqViewModel ? root.eqViewModel.isDefault : true
            property bool isBypassed: root.eqViewModel ? root.eqViewModel.bypass : false
            property bool hasError: root.eqViewModel ? (root.eqViewModel.previewStatus === "ERROR") : false

            color: isSelected ? "#1A324A" : (rowMouse.containsMouse ? "#122538" : "#0D1D2B")
            border.color: isSelected ? "#00C8FF" : "#1A324A"
            border.width: isSelected ? 2 : 1

            activeFocusOnTab: true

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    root.selectedIndex = 0
                    eqRow.forceActiveFocus()
                }
            }

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                    root.selectedIndex = 0
                    event.accepted = true
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                // Configuration Indicator LED (Default = OFF/dark, Manual = Green #00D47A)
                Rectangle {
                    objectName: "dspChainConfigLed_0"
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    radius: 4
                    color: eqRow.isDefaultState ? "#273A4D" : "#00D47A"
                    border.color: eqRow.isDefaultState ? "#3A4D60" : "#00FF94"
                    border.width: 1

                    ToolTip.text: eqRow.isDefaultState ? "Canonical Default / Flat" : "Manual Non-Default"
                    ToolTip.visible: ledMouse.containsMouse

                    MouseArea {
                        id: ledMouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: root.isCompact ? 0 : 2

                    Text {
                        text: "Parametric EQ"
                        color: eqRow.isSelected ? "#F5F8FC" : "#C4D4E0"
                        font.family: "Segoe UI"
                        font.pixelSize: root.isCompact ? 11 : 12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        text: eqRow.isDefaultState ? "Flat Default" : "Manual Edit"
                        color: "#7A8E9E"
                        font.family: "Segoe UI"
                        font.pixelSize: 9
                        visible: !root.isCompact || eqRow.isDefaultState
                    }
                }

                // Bypass Badge ("BYP")
                Rectangle {
                    objectName: "dspChainBypassBadge_0"
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
                    objectName: "dspChainErrorBadge_0"
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
