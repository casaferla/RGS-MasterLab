import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string iconKind: ""
    property string tone: "secondary"
    property color accentColor: "#00C8FF"
    property int controlSize: 32
    property int iconSize: 18

    implicitWidth: controlSize
    implicitHeight: controlSize
    activeFocusOnTab: true
    hoverEnabled: true
    padding: 0

    contentItem: StudioIcon {
        width: control.iconSize
        height: control.iconSize
        anchors.centerIn: parent
        kind: control.iconKind
        strokeColor: !control.enabled ? "#667380"
                   : control.down ? "#B8C7D9"
                   : control.hovered ? "#FFFFFF" : "#DDE6F3"
        fillColor: strokeColor
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: 8
            color: "transparent"
            border.width: control.activeFocus ? 2 : 0
            border.color: control.accentColor
        }
        Rectangle {
            anchors.fill: parent
            radius: 6
            color: !control.enabled ? "#0B141C"
                 : control.tone === "close" && control.hovered ? (control.down ? "#A82332" : "#C42B3A")
                 : control.down ? "#0B151E"
                 : control.hovered ? "#132433" : "#0E1A24"
            border.width: 1
            border.color: !control.enabled ? "#22303A"
                        : control.activeFocus ? "#3A566B"
                        : control.hovered ? "#3A566B" : "#2A3F4F"
            opacity: control.enabled ? 1.0 : 0.6
        }
    }
}
