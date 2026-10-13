import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// B4a read-only M16 analysis surface. No numerical True-Peak math in QML.
// Final M16/M17 shared spatial anchors remain subject to PO layout review.
Item {
    id: root
    objectName: "truePeakMeterSurface"
    property var measurementProvider: null
    readonly property bool hasMeasurement: measurementProvider
        ? measurementProvider.hasMeasurement : false

    ScrollView {
        id: meterScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: meterScroll.availableWidth
            spacing: 9

            Text {
                text: "TRUE PEAK · READ-ONLY ANALYSIS"
                color: "#7ED0E2"
                font.family: "Segoe UI"
                font.pixelSize: 12
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            TruePeakReadoutCard {
                objectName: "truePeakRecentCard"
                Layout.fillWidth: true
                Layout.preferredHeight: 108
                heading: "RECENT / LIVE TRUE PEAK"
                reading: "UNAVAILABLE"
                detail: "Live PCM/cursor observation not yet qualified (C2)"
                available: false
            }

            TruePeakReadoutCard {
                objectName: "truePeakFullResourceCard"
                Layout.fillWidth: true
                Layout.preferredHeight: 108
                heading: "COMPLETE PUBLISHED PROCESSED RESOURCE"
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
                Layout.preferredHeight: 112
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
                text: "Published render is not proof of audible NEW. Gate G OPEN."
                color: "#9CB3C1"
                font.family: "Segoe UI"
                font.pixelSize: 10
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
    }
}
