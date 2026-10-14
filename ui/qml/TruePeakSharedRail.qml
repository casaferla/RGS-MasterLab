import QtQuick
import QtQuick.Layouts

// The one common True Peak informational rail intended for M16/M17.
// M17 must bind the verified actual POST-TRIM result, not copy M16 input.
Item {
    id: root
    objectName: "truePeakSharedRail"
    property var measurementProvider: null
    readonly property bool hasMeasurement: measurementProvider
        ? measurementProvider.hasMeasurement : false
    implicitWidth: 316
    implicitHeight: 400

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        TruePeakReadoutCard {
            objectName: "truePeakRecentCard"
            Layout.fillWidth: true
            Layout.preferredHeight: 108
            heading: "RECENT / LIVE TRUE PEAK"
            reading: "UNAVAILABLE"
            detail: "Audible-cursor observation pending C2"
            available: false
        }
        TruePeakReadoutCard {
            objectName: "truePeakFullResourceCard"
            Layout.fillWidth: true
            Layout.preferredHeight: 108
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
            Layout.preferredHeight: 132
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
            text: "Static results are not evidence of audible NEW. Gate G OPEN."
            color: "#91A8B7"
            font.family: "Segoe UI"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Item { Layout.fillHeight: true }
    }
}
