import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string iconKind: ""
    property string tone: "secondary"
    // Opt-in for shared DSP utility cluster: family-colored Canvas glyphs,
    // no extra dark button tile. Other StudioIconButtons are unchanged.
    property bool familyHeaderUtility: false
    property color accentColor: "#00C8FF"
    property int controlSize: 32
    property int iconSize: 18

    implicitWidth: controlSize
    implicitHeight: controlSize
    activeFocusOnTab: true
    hoverEnabled: true
    padding: 0

    // Controls owns the contentItem geometry. Keep the actual header
    // painter in a separate fixed square, just like the Dock glyph.
    contentItem: Item {
        implicitWidth: control.iconSize
        implicitHeight: control.iconSize
        StudioIcon {
            width: control.familyHeaderUtility ? control.iconSize : parent.width
            height: control.familyHeaderUtility ? control.iconSize : parent.height
            anchors.centerIn: parent
            kind: control.iconKind
            strokeColor: !control.enabled ? "#667380"
                       : control.familyHeaderUtility
                         ? (control.hovered ? Qt.lighter(control.accentColor, 1.35)
                                            : control.accentColor)
                       : control.down ? "#B8C7D9"
                       : control.hovered ? "#FFFFFF" : "#DDE6F3"
            fillColor: strokeColor
        }
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
            color: control.familyHeaderUtility ? "transparent"
                 : !control.enabled ? "#0B141C"
                 : control.tone === "close" && control.hovered ? (control.down ? "#A82332" : "#C42B3A")
                 : control.down ? "#0B151E"
                 : control.hovered ? "#132433" : "#0E1A24"
            border.width: control.familyHeaderUtility ? 0 : 1
            border.color: !control.enabled ? "#22303A"
                        : control.activeFocus ? "#3A566B"
                        : control.hovered ? "#3A566B" : "#2A3F4F"
            opacity: control.enabled ? 1.0 : 0.6
        }
    }
}
