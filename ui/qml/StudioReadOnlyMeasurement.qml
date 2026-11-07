// Read-only analysis measurement, sharing StudioNumericField's Segoe/Consolas
// text grammar and numeric-well geometry, but never accepting user edits.
import QtQuick
Item {
    id: root
    objectName: "studioReadOnlyMeasurement"
    property string labelText: ""
    property string rawText: ""
    property string unitText: ""
    property bool dataAvailable: false
    property color accentColor: "#00C8FF"
    readonly property bool readOnly: true
    implicitWidth: 150
    implicitHeight: 48
    Column {
        anchors.fill: parent
        spacing: 3
        Text {
            width: parent.width; height: 14
            text: root.labelText
            color: "#A1B5C9"; font.family: "Segoe UI"
            font.pixelSize: 10; font.bold: true; elide: Text.ElideRight
        }
        Row {
            width: parent.width; height: 26; spacing: 6
            Rectangle {
                width: Math.max(70,parent.width-suffix.width
                       -(suffix.visible ? parent.spacing : 0))
                height: parent.height; radius: 4; color: "#0E1F2E"
                border.width: 1
                border.color: root.dataAvailable ? "#2C5A78" : "#314656"
                Text {
                    anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6
                    text: root.dataAvailable && root.rawText.length
                          ? root.rawText : "UNAVAILABLE"
                    color: root.dataAvailable ? "#F5F8FC" : "#9BAEBD"
                    font.family: "Consolas"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    Accessible.name: root.labelText + ": " + text
                }
            }
            Text {
                id: suffix
                text: root.unitText; visible: text.length > 0
                width: visible ? implicitWidth : 0; height: parent.height
                color: "#A1B5C9"; font.family: "Segoe UI"; font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
