import QtQuick
import QtQuick.Layouts

// Shared M16/M17 readout primitive. Its parent owns responsive positioning.
Rectangle {
    id: root
    property string heading: ""
    property string reading: "—"
    property string detail: ""
    property bool available: false
    implicitHeight: 108
    radius: 7
    color: "#10273A"
    border.color: "#294459"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 5
        Text {
            text: root.heading
            color: "#91AEC2"
            font.family: "Segoe UI"
            font.pixelSize: 11
            font.bold: true
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Text {
            text: root.reading
            color: root.available ? "#E9F7FF" : "#879CA9"
            font.family: "Segoe UI"
            font.pixelSize: 22
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Text {
            text: root.detail
            color: "#8AA4B3"
            font.family: "Segoe UI"
            font.pixelSize: 10
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
    }
}
