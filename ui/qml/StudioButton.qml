import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string tone: "secondary"
    property bool selected: false
    property string iconKind: ""
    property color accentColor: tone === "gold" ? "#F2B632" : "#00C8FF"
    property int minimumControlWidth: 80
    property int contentPadding: 16

    implicitWidth: Math.max(minimumControlWidth, contentRow.implicitWidth + contentPadding * 2)
    implicitHeight: 32
    activeFocusOnTab: true
    hoverEnabled: true
    padding: 0

    contentItem: Item {
        id: wrapperItem
        Row {
            id: contentRow
            objectName: "contentRow"
            anchors.centerIn: parent
            spacing: control.iconKind.length > 0 && label.visible ? 8 : 0
            StudioIcon {
                visible: control.iconKind.length > 0
                width: visible ? 16 : 0
                height: 16
                anchors.verticalCenter: parent.verticalCenter
                kind: control.iconKind
                strokeColor: label.color
                fillColor: label.color
            }
            Text {
                id: label
                visible: control.text.length > 0
                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                font.family: "Segoe UI"
                font.pixelSize: 12
                font.weight: control.selected || control.tone === "primary"
                    ? Font.DemiBold : Font.Normal
                color: !control.enabled ? "#6E7E8D"
                     : control.tone === "primary" ? "#F4F9FC"
                     : control.tone === "gold" && control.selected ? "#FFF4C8"
                     : control.down ? "#B8C7D9"
                     : control.hovered ? "#FFFFFF" : "#DDE6F3"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
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
            color: !control.enabled ? "#0B141C"
                 : control.tone === "primary" ? (control.down ? "#073344" : control.hovered ? "#0B4154" : "#0A3443")
                 : control.tone === "gold" && control.selected ? "#3B2D08"
                 : control.selected ? "#0A3443"
                 : control.down ? "#0B151E"
                 : control.hovered ? "#132433" : "#0E1A24"
            border.width: 1
            border.color: !control.enabled ? "#22303A"
                        : control.tone === "gold" && control.selected ? "#F2B632"
                        : control.selected ? control.accentColor
                        : control.hovered ? "#3A566B" : "#2A3F4F"
            opacity: control.enabled ? 1.0 : 0.4
        }
        Rectangle {
            visible: control.selected
            anchors.fill: parent
            anchors.margins: 3
            radius: 4
            color: "transparent"
            border.width: 1
            border.color: control.accentColor
            opacity: 0.32
        }
    }
}
