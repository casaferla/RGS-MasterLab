import QtQuick
import QtQuick.Layouts

// The one common True Peak informational rail intended for M16/M17.
// M17 must bind the verified actual POST-TRIM result, not copy M16 input.
Item {
    id: root
    objectName: "truePeakSharedRail"
    property var measurementProvider: null
    property var liveProvider: null
    readonly property bool hasMeasurement: measurementProvider
        ? measurementProvider.hasMeasurement : false
    implicitWidth: 316
    implicitHeight: 420
    readonly property bool compactRail: height < 530

    // A common M16/M17 information group centred vertically at any height.
    // Cards retain their natural size rather than growing to fill empty space.
    ColumnLayout {
        id: railContents
        objectName: "truePeakSharedRailContents"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: implicitHeight
        spacing: root.compactRail ? 7 : 10

        TruePeakReadoutCard {
            objectName: "truePeakRecentCard"
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactRail ? 82 : 108
            Layout.minimumHeight: 80
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
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactRail ? 82 : 108
            Layout.minimumHeight: 80
            heading: "SHORT HOLD / 1 SECOND"
            showColorKey: true
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
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactRail ? 82 : 108
            Layout.minimumHeight: 80
            heading: "COMPLETE PROCESSED RESOURCE"
            reading: root.hasMeasurement
                ? root.measurementProvider.peakText : "—"
            detail: root.hasMeasurement
                ? "Peak channel: " + root.measurementProvider.channelText
                : "No complete, source-bound measurement"
            available: root.hasMeasurement
        }
        TruePeakStatusCard {
            objectName: "truePeakProvenanceCard"
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactRail ? 112 : 132
            Layout.minimumHeight: 108
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
                + "Active/unproven SRC, loop or handoff = UNAVAILABLE. Gate G OPEN."
            color: "#91A8B7"
            font.family: "Segoe UI"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

    }
}
