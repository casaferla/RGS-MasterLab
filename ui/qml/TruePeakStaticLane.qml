import QtQuick
import QtQuick.Layouts

// Full-resource source-bound maxima only. Not live/instantaneous.
// 0 dBTP is a factual digital-zero guide, not a user Ceiling.
Rectangle {
    id: root
    objectName: "truePeakStaticLane"
    property string channelName: "L"
    property string reading: "—"
    property real level: 0.0
    property bool available: false
    property bool aboveDigitalZero: false
    property string meterKind: "FULL RESOURCE"
    radius: 7
    color: "#091C2B"
    border.color: "#24425A"
    border.width: 1
    implicitWidth: 135
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
                width: 46
                anchors.left: parent.left
                anchors.leftMargin: 9
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
                    objectName: "truePeakLaneFill"
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 5
                    width: parent.width - 10
                    height: root.available
                        ? Math.max(0, (parent.height - 10)
                            * Math.max(0, Math.min(1, root.level))) : 0
                    radius: 2
                    visible: root.available && height > 0
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: root.aboveDigitalZero ? "#F27683" : "#9BBED2"
                        }
                        GradientStop {
                            position: 1.0
                            color: root.aboveDigitalZero ? "#B64A53" : "#527B9F"
                        }
                    }
                }
                Repeater {
                    // Grid intervals are physical measured dBTP, not a
                    // guessed threshold for enabling M17.
                    model: [-48, -36, -24, -12]
                    Rectangle {
                        required property int modelData
                        x: 1
                        width: track.width - 2
                        height: 1
                        y: Math.round(5 + (6 - modelData) / 66
                                      * (track.height - 10))
                        color: "#557389"
                        opacity: 0.30
                    }
                }
                Rectangle {
                    objectName: "truePeakDigitalZeroGuide"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 1
                    y: Math.round(5 + 6.0 / 66.0 * (parent.height - 10))
                    height: 1
                    color: "#F0BE82"
                    opacity: 0.96
                }
            }
            Item {
                objectName: "truePeakDbTpScale"
                anchors.left: track.right
                anchors.leftMargin: 6
                anchors.top: track.top
                anchors.bottom: track.bottom
                width: 38
                Repeater {
                    model: [6, 0, -12, -24, -36, -48, -60]
                    Text {
                        required property int modelData
                        text: modelData > 0 ? "+" + modelData : "" + modelData
                        width: parent.width
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        font.weight: modelData === 0 ? Font.DemiBold
                                                     : Font.Normal
                        color: modelData === 0 ? "#F0BE82" : "#95ACBD"
                        y: Math.max(0, Math.min(parent.height - height,
                                    5 + (6 - modelData) / 66
                                    * (parent.height - 10) - height / 2))
                    }
                }
            }
        }
        Text {
            objectName: "truePeakLaneReading"
            text: root.available ? root.reading : "—"
            font.family: "Segoe UI"
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: !root.available ? "#7F98A9"
                : root.aboveDigitalZero ? "#F39A9E" : "#EBF5FC"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Text {
            text: root.available ? root.meterKind : "NO MEASUREMENT"
            font.family: "Segoe UI"
            font.pixelSize: 9
            color: "#8FAAB9"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }
}
