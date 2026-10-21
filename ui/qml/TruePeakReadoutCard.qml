import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shared M16/M17 readout primitive. Its parent owns responsive positioning.
Rectangle {
    id: root
    property string heading: ""
    property string reading: "—"
    property string detail: ""
    // State, qualification, realization identity and output format remain
    // discoverable even without the large redundant provenance card.
    property string additionalTooltip: ""
    property bool available: false
    property bool detailTooltipEnabled: false
    // M17 two-column only: keep extra breathing room around each readout.
    // Standalone M16 card dimensions and padding are not changed.
    property bool roomyLimiterReadout: false
    readonly property bool tightM17Height: root.roomyLimiterReadout
        && root.height < 90
    StudioTooltipDwell {
        id: cardTooltipDwell
        anchors.fill: parent
        canShow: root.detailTooltipEnabled
    }
    ToolTip.text: root.heading + "\n" + root.reading + "\n" + root.detail
        + (root.additionalTooltip.length > 0
            ? "\nMEASUREMENT / PROVENANCE\n" + root.additionalTooltip : "")
    ToolTip.visible: cardTooltipDwell.ready
    ToolTip.delay: 0
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
        objectName: "truePeakReadoutTextColumn"
        anchors.fill: parent
        anchors.margins: root.tightM17Height ? 5
            : root.roomyLimiterReadout ? 10
            : root.height < 95 ? 8 : 12
        spacing: root.tightM17Height ? 2
            : root.roomyLimiterReadout ? 4
            : root.height < 95 ? 2 : 5
        Text {
            text: root.heading
            color: root.useFamilyAccent ? root.familyAccent : "#91AEC2"
            font.family: "Segoe UI"
            font.pixelSize: root.tightM17Height ? 10 : 11
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
            font.pixelSize: root.tightM17Height ? 18 : 22
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Text {
            objectName: "truePeakReadoutDetailText"
            text: root.detail
            color: "#8AA4B3"
            font.family: "Segoe UI"
            font.pixelSize: root.tightM17Height ? 9 : 10
            Layout.fillWidth: true
            // The combined M17 rail must reflow complete status/provenance
            // descriptions rather than abbreviating meaningful text.
            // Standalone M16 retains its established single-line behaviour.
            wrapMode: root.roomyLimiterReadout ? Text.WordWrap : Text.NoWrap
            // At reduced actual Main height the full diagnostic remains
            // available in the parent stationary-hover tooltip. Limit only
            // the visual subordinate detail to keep measured digits visible.
            maximumLineCount: root.tightM17Height ? 2 : 2147483647
            elide: root.tightM17Height ? Text.ElideRight
                : root.roomyLimiterReadout ? Text.ElideNone : Text.ElideRight
        }
    }
}
