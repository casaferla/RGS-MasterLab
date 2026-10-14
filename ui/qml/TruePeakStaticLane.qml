import QtQuick
import QtQuick.Layouts

// M16 full-resource DISPLAY ONLY, never a live/instantaneous meter.
// M17 can reuse the rail, but M17's actual output measurement is independent.
Rectangle {
    id: root
    objectName: "truePeakStaticLane"
    property string channelName: "L"
    property string reading: "—"
    property real level: 0.0
    property bool available: false
    property bool aboveDigitalZero: false
    radius: 7
    color: "#091C2B"
    border.color: "#24425A"
    border.width: 1
    implicitWidth: 122
    implicitHeight: 360

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text {
            text: root.channelName
            font.family: "Segoe UI"
            font.pixelSize: 17
            font.weight: Font.DemiBold
            color: "#DEEAF2"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 165

            Rectangle {
                id: track
                objectName: "truePeakTrack"
                width: 52
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                radius: 4
                color: "#061421"
                border.color: "#36546A"
                border.width: 1
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    color: "#142D40"
                    opacity: 0.28
                    radius: 2
                }
                Rectangle {
                    id: levelFill
                    objectName: "truePeakLaneFill"
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 5
                    width: parent.width - 10
                    height: root.available
                        ? Math.max(0, (parent.height - 10)
                            * Math.max(0, Math.min(1, root.level))) : 0
                    radius: 2
                    color: root.aboveDigitalZero ? "#EF806D" : "#6683A1"
                    visible: root.available && height > 0
                }
                Rectangle {
                    // The dashed reference at zero is informational, not a
                    // predefined user Ceiling. This is a visual scale marker.
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 1
                    y: Math.round((parent.height - 10) * (6.0 / 66.0)) + 4
                    height: 1
                    color: "#B3BBC9"
                    opacity: 0.8
                }
            }
        }
        Text {
            objectName: "truePeakLaneReading"
            text: root.available ? root.reading : "—"
            font.family: "Segoe UI"
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: root.available ? "#EBF5FC" : "#7F98A9"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Text {
            text: root.available ? "FULL RESOURCE" : "NO MEASUREMENT"
            font.family: "Segoe UI"
            font.pixelSize: 9
            color: "#8FAAB9"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }
}
