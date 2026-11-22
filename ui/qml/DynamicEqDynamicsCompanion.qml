import QtQuick
import QtQuick.Controls
import QtQuick.Window

// Companion window only: never overlays DynamicEqGraph's frequency axis.
// No Product instance is registered before L3 close.
Window {
    id: root
    objectName: "dynamicEqDynamicsCompanion"
    property var dynamicsViewModel: null
    width: 1050
    height: 174
    minimumWidth: 980
    maximumWidth: 1280
    minimumHeight: 174
    maximumHeight: 174
    color: "#081824"
    title: "RGS Dynamic EQ — Dynamic Controls (Research)"
    flags: Qt.Tool
    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        radius: 6
        color: "#0E1F2E"
        border.color: "#345771"
        border.width: 1
        Text {
            objectName: "dynamicEqControlsReadOnlyStatus"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.topMargin: 6
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.bold: true
            color: "#A1B5C9"
            text: root.dynamicsViewModel && root.dynamicsViewModel.manualEditable
                ? "C01 • RESEARCH MANUAL CONTROL"
                : "C01 • READ ONLY / L3 AUTOMATCH"
        }
        DynamicEqDynamicsStrip {
            objectName: "dynamicEqDynamicsStrip"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 7
            anchors.rightMargin: 7
            anchors.bottomMargin: 7
            dynamicsViewModel: root.dynamicsViewModel
        }
    }
}
