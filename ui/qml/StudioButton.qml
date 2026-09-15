import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string tone: "secondary"
    property bool selected: false
    property int contentPadding: 16

    implicitWidth: Math.max(44, contentItem.implicitWidth + contentPadding * 2)
    implicitHeight: 36
    activeFocusOnTab: true
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        font.family: "Segoe UI"
        font.pixelSize: 12
        font.weight: control.selected || control.tone === "primary"
            ? Font.DemiBold : Font.Medium
        color: !control.enabled ? "#6B7A87"
             : control.tone === "primary" ? "#07131B"
             : control.tone === "gold" && control.selected ? "#1B1504"
             : "#E6EEF0"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: !control.enabled ? "#101A22"
             : control.tone === "primary" ? (control.down ? "#209ACB" : "#43C7F3")
             : control.tone === "danger" && control.hovered ? "#8C2F3A"
             : control.tone === "gold" && control.selected ? "#E6B94A"
             : control.selected ? "#16384A"
             : control.down ? "#0A141D"
             : control.hovered ? "#13222F" : "#0F1820"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#3DA6FF"
                    : control.tone === "gold" && control.selected ? "#F2CC68"
                    : control.selected ? "#43C7F3" : "#2A3947"
        opacity: control.enabled ? 1.0 : 0.72
    }
}
