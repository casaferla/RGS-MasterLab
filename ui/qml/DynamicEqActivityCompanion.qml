import QtQuick
import QtQuick.Window
// Activity only: no change to the Parametric-derived spectrum or graph.
Window {
    id: root
    objectName: "dynamicEqActivityCompanion"
    property var activityViewModel: null
    width: 900
    height: 360
    minimumWidth: 680
    minimumHeight: 290
    color: "#081824"
    title: "RGS Dynamic EQ — C01 Activity (Research)"
    flags: Qt.Tool
    Rectangle {
        anchors.fill: parent
        anchors.margins: 5
        radius: 6
        color: "#0E1F2E"
        border.color: "#345771"
        border.width: 1
        DynamicEqActivityHistoryGraph {
            objectName: "dynamicEqActivityHistoryGraph"
            anchors.fill: parent
            anchors.margins: 5
            activityViewModel: root.activityViewModel
        }
    }
}
