import QtQuick
import QtQuick.Layouts

// Shared M16/M17 read-only provenance/qualification display.
Rectangle {
    id: root
    property string stateLabel: "NO SOURCE"
    property string qualification: "NO QUALIFIED MEASUREMENT"
    property string identity: "NO PUBLISHED REALIZATION"
    property string formatLabel: "—"
    implicitHeight: 112
    radius: 7
    color: "#10273A"
    border.color: "#294459"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 5
        Text {
            text: "MEASUREMENT / PROVENANCE"
            color: "#91AEC2"
            font.family: "Segoe UI"
            font.pixelSize: 11
            font.bold: true
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Repeater {
            model: [root.stateLabel, root.qualification, root.identity, root.formatLabel]
            Text {
                required property string modelData
                text: modelData
                color: "#B9D1DE"
                font.family: "Segoe UI"
                font.pixelSize: 10
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
    }
}
