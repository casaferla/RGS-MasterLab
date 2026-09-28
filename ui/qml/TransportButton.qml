import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string iconKind: "play"
    property bool primary: false
    property bool compact: false
    property color accentColor: "#00C8FF"

    readonly property int resolvedControlSize: compact ? (primary ? 40 : 32) : (primary ? 68 : 44)
    readonly property int resolvedIconSize: compact ? (primary ? 20 : 16) : (primary ? 28 : 18)
    readonly property int focusHaloExtra: compact ? 4 : (primary ? 12 : 6)

    implicitWidth: resolvedControlSize
    implicitHeight: resolvedControlSize
    activeFocusOnTab: true
    hoverEnabled: true
    padding: 0

    contentItem: StudioIcon {
        width: control.resolvedIconSize
        height: width
        anchors.centerIn: parent
        kind: control.iconKind
        strokeWidth: control.primary ? 1.6 : 1.8
        strokeColor: !control.enabled ? "#65717C"
                   : control.down ? "#B8C7D9" : "#F4F7FA"
        fillColor: strokeColor
    }

    background: Item {
        Rectangle {
            anchors.centerIn: parent
            width: parent.width + control.focusHaloExtra
            height: width
            radius: width / 2
            color: "transparent"
            border.width: control.activeFocus ? (control.primary ? 2 : 1) : 0
            border.color: control.accentColor
            opacity: control.activeFocus ? 0.72 : 0.0
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width
            height: width
            radius: width / 2
            color: !control.enabled ? "#0A1218"
                 : control.down ? "#07131B"
                 : control.hovered ? "#102936" : "#0B202B"
            border.width: control.primary ? 2 : 1
            border.color: !control.enabled ? "#26343E"
                        : control.primary ? (control.hovered ? "#66DCFF" : control.accentColor)
                        : (control.hovered ? "#527084" : "#324A5B")
            opacity: control.enabled ? 1.0 : 0.55
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width - (control.primary ? 10 : 8)
            height: width
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: control.enabled && control.primary ? "#20566B" : "#263844"
            opacity: control.down ? 0.45 : 0.85
        }
    }
}
