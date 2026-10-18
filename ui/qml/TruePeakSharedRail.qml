import QtQuick
import QtQuick.Layouts

// The one common True Peak informational rail intended for M16/M17.
// M17 must bind the verified actual POST-TRIM result, not copy M16 input.
Item {
    id: root
    objectName: "truePeakSharedRail"
    property var measurementProvider: null
    property var liveProvider: null
    // M17 only: turn four separate readout cards into an equal-height,
    // coherent second column. Standalone M16 keeps its exact prior layout.
    property bool framedForLimiter: false
    property bool showProvenanceTooltips: framedForLimiter
    property bool useFamilyAccent: false
    property color familyAccent: "#6CBCCF"
    readonly property bool hasMeasurement: measurementProvider
        ? measurementProvider.hasMeasurement : false
    implicitWidth: 316
    implicitHeight: 330
    readonly property bool compactRail: framedForLimiter || height < 530
    property color fullResourceAccent: "#6CBCCF"

    // A complete panel boundary, not just four individually outlined cards.
    Rectangle {
        objectName: "truePeakSharedRailFrame"
        anchors.fill: parent
        visible: root.framedForLimiter
        radius: 8
        color: "#0A1C2D"
        border.color: root.familyAccent
        border.width: 1
    }

    // M16 is centered as before; M17 fills the panel and distributes height.
    // Cards retain their natural size rather than growing to fill empty space.
    ColumnLayout {
        id: railContents
        objectName: "truePeakSharedRailContents"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: root.framedForLimiter ? parent.top : undefined
        anchors.bottom: root.framedForLimiter ? parent.bottom : undefined
        anchors.verticalCenter: root.framedForLimiter ? undefined : parent.verticalCenter
        anchors.margins: root.framedForLimiter ? 8 : 0
        height: root.framedForLimiter ? undefined : implicitHeight
        spacing: root.framedForLimiter ? 6 : root.compactRail ? 3 : 10

        TruePeakReadoutCard {
            objectName: "truePeakRecentCard"
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: root.framedForLimiter
            useFamilyAccent: root.useFamilyAccent
            familyAccent: root.familyAccent
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactRail ? 68 : 108
            Layout.minimumHeight: 66
            heading: "RECENT / LIVE TRUE PEAK"
            reading: root.liveProvider && root.liveProvider.recentAvailable
                ? root.liveProvider.peakText : "UNAVAILABLE"
            detail: root.liveProvider && root.liveProvider.monitoringAttempted
                ? root.liveProvider.statusText
                : "NOT MONITORED · select PROCESSED and press Play"
            available: root.liveProvider ? root.liveProvider.recentAvailable : false
        }
        TruePeakReadoutCard {
            objectName: "truePeakHoldCard"
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: root.framedForLimiter
            useFamilyAccent: root.useFamilyAccent
            familyAccent: root.familyAccent
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactRail ? 68 : 108
            Layout.minimumHeight: 66
            heading: "SHORT HOLD / 1 SECOND"
            keyColor: "#F2C66F"
            valueColor: "#F2C66F"
            reading: root.liveProvider && root.liveProvider.holdAvailable
                ? root.liveProvider.holdText : "UNAVAILABLE"
            detail: root.liveProvider && root.liveProvider.monitoringAttempted
                ? "Max over the last fully covered 1 s of PROCESSED"
                : "NOT MONITORED"
            available: root.liveProvider ? root.liveProvider.holdAvailable : false
        }
        TruePeakReadoutCard {
            objectName: "truePeakFullResourceCard"
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: root.framedForLimiter
            useFamilyAccent: root.useFamilyAccent
            familyAccent: root.familyAccent
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactRail ? 68 : 108
            Layout.minimumHeight: 66
            heading: "COMPLETE PROCESSED RESOURCE"
            valueColor: root.fullResourceAccent
            reading: root.hasMeasurement
                ? root.measurementProvider.peakText : "—"
            detail: root.hasMeasurement
                ? "Peak channel: " + root.measurementProvider.channelText
                : "No complete, source-bound measurement"
            available: root.hasMeasurement
        }
        TruePeakStatusCard {
            objectName: "truePeakProvenanceCard"
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: root.framedForLimiter
            useFamilyAccent: root.useFamilyAccent
            familyAccent: root.familyAccent
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactRail ? 88 : 132
            Layout.minimumHeight: 84
            stateLabel: root.measurementProvider
                ? root.measurementProvider.stateText : "NO SOURCE"
            qualification: root.measurementProvider
                ? root.measurementProvider.qualificationText
                : "NO QUALIFIED MEASUREMENT"
            identity: root.measurementProvider
                ? root.measurementProvider.realizationText
                : "NO PUBLISHED REALIZATION"
            formatLabel: root.measurementProvider
                ? root.measurementProvider.sampleRateText : "—"
        }
        Text {
            text: "RECENT requires exact sink-consumed PROCESSED ownership. "
                + "Unproven SRC/handoff or loop boundary = UNAVAILABLE; verified loop interior = LIVE. Gate G OPEN."
            color: "#91A8B7"
            font.family: "Segoe UI"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            // Keep the provenance note visibly below the card at
            // canonical minimum height without resizing the card itself.
            Layout.topMargin: root.framedForLimiter ? 1 : root.compactRail ? 8 : 10
            Layout.fillWidth: true
        }

    }
}
