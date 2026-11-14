// Read-only analysis measurement, sharing StudioNumericField's Segoe/Consolas
// text grammar and numeric-well geometry, but never accepting user edits.
import QtQuick
Item {
    id: root
    objectName: "studioReadOnlyMeasurement"
    property string labelText: ""
    property string rawText: ""
    property string unitText: ""
    property bool dataAvailable: false
    property color accentColor: "#00C8FF"
    // Opt-in large readout based directly on InputGainEditor.gainDbDisplay.
    // Measurements are observational, never editable.
    property bool heroReadout: false
    readonly property int numericPixelSize: root.heroReadout ? 36 : 13
    readonly property bool readOnly: true
    implicitWidth: 150
    implicitHeight: root.heroReadout ? 102 : 48
    Column {
        anchors.fill: parent
        spacing: root.heroReadout ? 5 : 3
        Text {
            width: parent.width; height: root.heroReadout ? 30 : 14
            text: root.labelText
            color: "#A1B5C9"; font.family: "Segoe UI"
            font.pixelSize: root.heroReadout ? 11 : 10
            font.bold: true
            horizontalAlignment: root.heroReadout ? Text.AlignHCenter : Text.AlignLeft
            wrapMode: root.heroReadout ? Text.WordWrap : Text.NoWrap
            verticalAlignment: Text.AlignVCenter
            elide: root.heroReadout ? Text.ElideNone : Text.ElideRight
        }
        Row {
            width: parent.width; height: root.heroReadout ? 50 : 26; spacing: 6
            Rectangle {
                width: Math.max(70,parent.width-suffix.width
                       -(suffix.visible ? parent.spacing : 0))
                height: parent.height; radius: 4
                color: root.heroReadout ? "transparent" : "#0E1F2E"
                border.width: root.heroReadout ? 0 : 1
                border.color: root.dataAvailable ? "#2C5A78" : "#314656"
                Text {
                    anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6
                    text: root.dataAvailable && root.rawText.length
                          ? root.rawText : (root.heroReadout ? "—" : "UNAVAILABLE")
                    color: root.dataAvailable ? "#F5F8FC" : "#9BAEBD"
                    font.family: "Consolas"
                    font.pixelSize: root.numericPixelSize
                    font.weight: root.heroReadout ? Font.Bold : Font.Normal
                    horizontalAlignment: root.heroReadout
                        ? Text.AlignHCenter : Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    Accessible.name: root.labelText + ": " + text
                }
            }
            Text {
                id: suffix
                text: root.unitText; visible: text.length > 0
                // Row excludes invisible children; let Text retain its
                // natural implicit width. Binding width to implicitWidth here
                // makes Qt Quick Text re-enter its own width calculation when
                // M19–M21 are selected (strict GUI smoke warning).
                height: parent.height
                color: "#A1B5C9"; font.family: "Segoe UI"; font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
