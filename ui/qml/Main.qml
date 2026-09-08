import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 1280
    height: 720
    minimumWidth: 640
    minimumHeight: 360
    visible: true
    title: "RGS MasterLab"
    color: "#17191d"

    Column {
        anchors.centerIn: parent
        spacing: 10

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#f1f3f5"
            font.pixelSize: 32
            font.weight: Font.DemiBold
            text: "RGS MasterLab"
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#8f969e"
            font.pixelSize: 14
            text: "Bootstrap build"
        }
    }
}
