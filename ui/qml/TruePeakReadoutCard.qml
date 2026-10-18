import QtQuick
import QtQuick.Layouts

// Shared M16/M17 readout primitive. Its parent owns responsive positioning.
Rectangle {
    id: root
    property string heading: ""
    property string reading: "—"
    property string detail: ""
    property bool available: false
    property bool useFamilyAccent: false
    property color familyAccent: "#6CBCCF"
    property color valueColor: "#E9F7FF"
    property bool showColorKey: false
    property color keyColor: "#F2C66F"
    function alignedDbtp(value) {
        // Exact silence uses the finite readout's stable unit column;
        // no invented fractional value, no semantic change to −∞.
        if (value === "−∞ dBTP" || value === "-∞ dBTP") {
            const symbol = value.slice(0, 2)
            return " ".repeat(4 - symbol.length) + symbol
                    + "   " + "  dBTP"
        }
        const match = /^(-?[0-9]+)\.([0-9]{2}) dBTP$/.exec(value)
        if (!match)
            return value
        return " ".repeat(Math.max(0, 4 - match[1].length))
                + match[1] + "." + match[2] + "  dBTP"
    }
    implicitHeight: 108
    radius: 7
    color: "#10273A"
    border.color: root.useFamilyAccent ? root.familyAccent : "#294459"
    border.width: 1

    Rectangle {
        objectName: "truePeakHoldLegendLine"
        visible: root.showColorKey
        anchors.right: parent.right
        anchors.rightMargin: 13
        anchors.top: parent.top
        anchors.topMargin: 18
        width: 20
        height: 3
        radius: 1.5
        color: root.keyColor
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.height < 95 ? 8 : 12
        spacing: root.height < 95 ? 2 : 5
        Text {
            text: root.heading
            color: root.useFamilyAccent ? root.familyAccent : "#91AEC2"
            font.family: "Segoe UI"
            font.pixelSize: 11
            font.bold: true
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Text {
            objectName: "truePeakNumericValueText"
            // Decimal and unit are stationary; the minus is attached
            // directly to the integer (including two-digit transitions).
            text: root.available ? root.alignedDbtp(root.reading) : root.reading
            color: root.available ? root.valueColor : "#879CA9"
            font.family: "Consolas"
            font.pixelSize: 22
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Text {
            text: root.detail
            color: "#8AA4B3"
            font.family: "Segoe UI"
            font.pixelSize: 10
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
    }
}
