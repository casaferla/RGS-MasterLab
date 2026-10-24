import QtQuick
import QtQuick.Layouts

// Full-resource source-bound maxima only. Not live/instantaneous.
// 0 dBTP is a factual digital-zero guide, not a user Ceiling.
Rectangle {
    id: root
    objectName: "truePeakStaticLane"
    property string channelName: "L"
    property string reading: "—"
    property real level: 0.0
    property bool available: false
    property bool aboveDigitalZero: false
    property bool holdAvailable: false
    property real holdLevel: 0.0
    property string meterKind: "FULL RESOURCE"
    property bool useFamilyAccent: false
    property color familyAccent: "#6CBCCF"
    // The shared M17 Meter bar always uses the Meter/Analysis fill, BOTH
    // in BYP and ON. M17 frame/control accents remain Dynamics Copper in ON;
    // real >0 dBTP keeps its independent coral alert semantics.
    property bool preserveMeterBarPalette: false
    // M17 can shrink the same two physical L/R lanes when a real Main
    // window has less height than the legacy isolated 548px capture.
    // M16 keeps its original 120px minimum scale track and layout.
    property bool compactVertical: false
    radius: 7
    color: "#091C2B"
    border.color: root.useFamilyAccent ? root.familyAccent : "#24425A"
    border.width: 1
    implicitWidth: 135
    implicitHeight: 270

    function alignedDbtp(value) {
        // Preserve the same fixed decimal/unit columns for exact silence.
        // −∞ has no decimal places: blank-fill them, never invent digits.
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 4
        Text {
            objectName: "truePeakChannelLabel"
            text: root.channelName
            font.family: "Segoe UI"
            font.pixelSize: 17
            font.weight: Font.DemiBold
            color: "#DEEAF2"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: false
            // Align the channel glyph to the actual meter TRACK, not to
            // the entire card (which also contains the dBTP scale).
            Layout.preferredWidth: 46
            Layout.leftMargin: 9
        }
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: root.compactVertical ? 75 : 120
            Rectangle {
                id: track
                objectName: "truePeakTrack"
                width: 46
                anchors.left: parent.left
                anchors.leftMargin: 9
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                radius: 4
                color: "#061421"
                border.color: "#36546A"
                border.width: 1
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    color: "#142D40"
                    opacity: 0.28
                    radius: 2
                }
                Rectangle {
                    objectName: "truePeakLaneFill"
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 5
                    width: parent.width - 10
                    height: root.available
                        ? Math.max(0, (parent.height - 10)
                            * Math.max(0, Math.min(1, root.level))) : 0
                    radius: 2
                    visible: root.available && height > 0
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: root.aboveDigitalZero ? "#F27683"
                                : root.preserveMeterBarPalette ? "#6CBCCF"
                                : root.useFamilyAccent ? root.familyAccent : "#9BBED2"
                        }
                        GradientStop {
                            position: 1.0
                            color: root.aboveDigitalZero ? "#B64A53"
                                : root.preserveMeterBarPalette ? Qt.darker("#6CBCCF", 1.35)
                                : root.useFamilyAccent ? Qt.darker(root.familyAccent, 1.35) : "#527B9F"
                        }
                    }
                }
                Repeater {
                    // Grid intervals are physical measured dBTP, not a
                    // guessed threshold for enabling M17.
                    model: [-48, -36, -24, -12]
                    Rectangle {
                        required property int modelData
                        x: 1
                        width: track.width - 2
                        height: 1
                        y: Math.round(5 + (6 - modelData) / 66
                                      * (track.height - 10))
                        color: "#557389"
                        opacity: 0.30
                    }
                }
                Rectangle {
                    objectName: "truePeakDigitalZeroGuide"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 1
                    y: Math.round(5 + 6.0 / 66.0 * (parent.height - 10))
                    height: 1
                    color: "#7894A8"
                    opacity: 0.96
                }
                // One channel-specific marker for the rolling 1s maximum.
                // It uses the same dBTP axis as RECENT and carries no
                // artificial signal delay or unauthorised 1s lookahead.
                Rectangle {
                    objectName: "truePeakHoldLine"
                    z: 3
                    x: 2
                    width: track.width - 4
                    height: 3
                    radius: 1.5
                    y: 5 + (1 - Math.max(0, Math.min(1, root.holdLevel)))
                        * (track.height - 10) - height / 2
                    visible: root.holdAvailable
                    color: "#F2C66F"
                }
            }
            Item {
                objectName: "truePeakDbTpScale"
                anchors.left: track.right
                anchors.leftMargin: 6
                anchors.top: track.top
                anchors.bottom: track.bottom
                width: 38
                Repeater {
                    model: [6, 0, -12, -24, -36, -48, -60]
                    Text {
                        required property int modelData
                        objectName: modelData === 6 ? "truePeakPlusSixMarker" : "truePeakScaleOtherMarker"
                        text: modelData > 0 ? "+" + modelData : "" + modelData
                        width: parent.width
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        font.weight: modelData === 0 ? Font.DemiBold
                                                     : Font.Normal
                        color: modelData === 0 ? "#A4BCCB" : "#95ACBD"
                        // +6 is the top endpoint: center the glyph on
                        // the actual top of the meter; do not clamp its
                        // top edge down to the track's first interior pixel.
                        // Do not force 0 dBTP down to y>=15 at small
                        // heights: that collides with the -12 label.
                        y: modelData === 6 ? 5 - height / 2
                            : Math.max(0, Math.min(parent.height - height,
                                5 + (6 - modelData) / 66
                                * (parent.height - 10) - height / 2))
                    }
                }
            }
        }
        Text {
            objectName: "truePeakLaneReading"
            // Right-aligned signed integer in a fixed four-character field;
            // the minus remains glued to its integer. Decimal and dBTP
            // positions never move even on -9.99 -> -10.00 crossings.
            text: root.available ? root.alignedDbtp(root.reading) : "—"
            font.family: "Consolas"
            font.pixelSize: 13
            font.weight: Font.DemiBold
            color: !root.available ? "#7F98A9"
                : root.aboveDigitalZero ? "#F39A9E" : "#EBF5FC"
            horizontalAlignment: Text.AlignLeft
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Text {
            text: root.available ? root.meterKind : "NO MEASUREMENT"
            font.family: "Segoe UI"
            font.pixelSize: 9
            color: "#8FAAB9"
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }
}
