import QtQuick
import QtQuick.Controls

Button {
    id: control

    property bool selected: false
    property string tone: "secondary"
    property color accentColor: tone === "gold" ? "#F2B632" : "#00C8FF"
    property int minimumControlWidth: 80
    property int contentPadding: 4

    implicitWidth: Math.max(minimumControlWidth, label.implicitWidth + contentPadding * 2)
    implicitHeight: 28
    activeFocusOnTab: true
    hoverEnabled: true
    padding: 0

    contentItem: Text {
        id: label
        text: control.text
        font.family: "Segoe UI"
        font.pixelSize: 12
        font.weight: control.selected || control.tone === "primary" ? Font.DemiBold : Font.Normal
        color: !control.enabled ? "#62788F"
             : control.tone === "primary" ? "#F5F8FC"
             : control.tone === "gold" && control.selected ? "#FFF4C8"
             : control.down ? "#A1B5C9"
             : control.hovered ? "#FFFFFF" : "#A1B5C9"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: 6
            color: "transparent"
            border.width: control.activeFocus ? 2 : 0
            border.color: control.accentColor
        }

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: !control.enabled ? "#66000000"
                 : control.tone === "primary" ? (control.down ? "#073344" : control.hovered ? "#0B4154" : "#0A3443")
                 : control.tone === "gold" && control.selected ? "#3B2D08"
                 : control.selected ? "#0A3443"
                 : control.down ? "#26000000"
                 : control.hovered ? "#1400C8FF" : "#0E1F2E"
            border.width: 1
            border.color: !control.enabled ? "#2C5A78"
                        : control.tone === "gold" && control.selected ? "#F2B632"
                        : control.selected ? control.accentColor
                        : control.hovered ? "#5900C8FF" : "#2C5A78"
        }
    }
}
