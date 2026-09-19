import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    implicitWidth: label.implicitWidth + 12 + 40
    implicitHeight: 32
    activeFocusOnTab: true
    hoverEnabled: true
    spacing: 12
    padding: 0

    contentItem: Text {
        id: label
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: control.text
        color: !control.enabled ? "#6E7E8D"
             : control.down ? "#B8C7D9"
             : control.hovered ? "#FFFFFF" : "#DDE6F3"
        font.family: "Segoe UI"
        font.pixelSize: 12
        verticalAlignment: Text.AlignVCenter
    }

    indicator: Item {
        implicitWidth: 44
        implicitHeight: 26
        x: control.width - width
        y: (control.height - height) / 2
        Rectangle {
            anchors.centerIn: parent
            width: 44
            height: 26
            radius: 13
            color: "transparent"
            border.width: control.activeFocus ? 2 : 0
            border.color: "#00C8FF"
        }
        Rectangle {
            anchors.centerIn: parent
            width: 40
            height: 22
            radius: 11
            color: control.checked ? "#005A6B" : "#1A2A36"
            border.width: 1
            border.color: control.hovered ? "#3A566B" : "#2A3F4F"
            opacity: control.enabled ? 1.0 : 0.4
            Rectangle {
                width: 16
                height: 16
                radius: 8
                y: 3
                x: control.checked ? 21 : 3
                color: "#E6ECF3"
            }
        }
    }
}
