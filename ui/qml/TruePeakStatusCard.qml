import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shared M16/M17 read-only provenance/qualification display.
Rectangle {
    id: root
    property string stateLabel: "NO SOURCE"
    property string qualification: "NO QUALIFIED MEASUREMENT"
    property string identity: "NO PUBLISHED REALIZATION"
    property string formatLabel: "—"
    property bool detailTooltipEnabled: false
    StudioTooltipDwell {
        id: cardTooltipDwell
        anchors.fill: parent
        canShow: root.detailTooltipEnabled
    }
    ToolTip.text: "MEASUREMENT / PROVENANCE\n"
        + root.stateLabel + "\n" + root.qualification + "\n"
        + root.identity + "\n" + root.formatLabel
    ToolTip.visible: cardTooltipDwell.ready
    ToolTip.delay: 0
    property bool useFamilyAccent: false
    property color familyAccent: "#6CBCCF"
    implicitHeight: 112
    radius: 7
    color: "#10273A"
    border.color: root.useFamilyAccent ? root.familyAccent : "#294459"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.height < 115 ? 8 : 12
        spacing: root.height < 115 ? 3 : 5
        Text {
            text: "MEASUREMENT / PROVENANCE"
            color: root.useFamilyAccent ? root.familyAccent : "#91AEC2"
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
