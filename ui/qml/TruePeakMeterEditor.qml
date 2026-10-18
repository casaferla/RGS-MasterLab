import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// M16 B4d physical candidate. Only a visual observer; no ProcessingChain DSP.
// The M16/M17 rail is shared, but specific pixel anchors await PO approval.
Item {
    id: root
    objectName: "truePeakMeterSurface"
    property var measurementProvider: null
    property var liveProvider: null
    // M17 opts into compact and family theme; M16 defaults remain unchanged.
    property bool compactForLimiter: false
    property bool useFamilyAccent: false
    property color familyAccent: "#6CBCCF"
    // The large channel bars are LIVE-only: a whole-resource future maximum
    // must never appear as a frozen "now" meter during Play or seek.
    // Full-resource STATIC remains independently visible in the shared rail.
    readonly property bool showLive: true
    readonly property bool liveAvailable: liveProvider
        ? liveProvider.recentAvailable : false
    readonly property bool holdAvailable: liveProvider
        ? liveProvider.holdAvailable : false
    readonly property bool hasMeasurement: measurementProvider
        ? measurementProvider.hasMeasurement : false
    readonly property int measuredChannels: liveProvider
        && liveProvider.channelCount > 0 ? liveProvider.channelCount
        : root.hasMeasurement ? root.measurementProvider.measuredChannelCount : 0
    readonly property bool isMono: root.measuredChannels === 1

    // Canonical minimum window is a fully visible editor, never a scroller.
    // The Qt layout itself distributes the available pixel height.
    RowLayout {
        objectName: "truePeakContentLayout"
        anchors.fill: parent
        spacing: root.compactForLimiter ? 8 : 10

            Rectangle {
                id: meterBay
                objectName: "truePeakStaticMeterBay"
                Layout.fillWidth: true
                Layout.minimumWidth: root.compactForLimiter ? 270 : 340
                Layout.fillHeight: true
                radius: 8
                color: "#0A1C2D"
                border.color: root.useFamilyAccent ? root.familyAccent : "#31506B"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 9
                    spacing: 5
                    Text {
                        text: "LIVE PROCESSED TRUE PEAK"
                        color: root.useFamilyAccent ? root.familyAccent : "#B3C9D9"
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: root.compactForLimiter ? 6 : 12
                        Item { Layout.fillWidth: true }
                        TruePeakStaticLane {
                            objectName: "truePeakLeftLane"
                            Layout.fillHeight: true
                            Layout.preferredWidth: root.compactForLimiter ? 117 : 135
                            channelName: root.isMono ? "C" : "L"
                            useFamilyAccent: root.useFamilyAccent
                            familyAccent: root.familyAccent
                            reading: root.liveAvailable
                                ? root.liveProvider.leftText : "—"
                            available: root.liveAvailable
                            level: root.liveAvailable
                                ? root.liveProvider.leftLevel : 0
                            aboveDigitalZero: root.liveAvailable
                                && root.liveProvider.leftOverZero
                            meterKind: "RECENT 100 ms"
                            holdAvailable: root.holdAvailable
                            holdLevel: root.holdAvailable
                                ? root.liveProvider.leftHoldLevel : 0.0
                        }
                        TruePeakStaticLane {
                            objectName: "truePeakRightLane"
                            Layout.fillHeight: true
                            Layout.preferredWidth: root.compactForLimiter ? 117 : 135
                            visible: !root.isMono
                            channelName: "R"
                            useFamilyAccent: root.useFamilyAccent
                            familyAccent: root.familyAccent
                            reading: root.liveAvailable
                                && root.measuredChannels === 2
                                ? root.liveProvider.rightText : "—"
                            available: root.liveAvailable
                                && root.measuredChannels === 2
                            level: root.liveAvailable
                                && root.measuredChannels === 2
                                ? root.liveProvider.rightLevel : 0
                            aboveDigitalZero: root.liveAvailable
                                && root.measuredChannels === 2
                                && root.liveProvider.rightOverZero
                            meterKind: root.showLive ? "RECENT 100 ms" : "STATIC RESOURCE"
                            holdAvailable: root.holdAvailable
                                && root.measuredChannels === 2
                            holdLevel: root.holdAvailable
                                && root.measuredChannels === 2
                                ? root.liveProvider.rightHoldLevel : 0.0
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Text {
                        text: "Coral fill > 0 dBTP · amber = 1 s HOLD · grey = 0 dBTP reference (not Ceiling)"
                        color: "#8EAAB9"
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        text: root.liveProvider && root.liveProvider.monitoringAttempted
                            ? "RECENT 100 ms · " + root.liveProvider.statusText
                            : "RECENT 100 ms · NOT MONITORED"
                        color: "#8EAAB9"
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }
            }

            TruePeakSharedRail {
                id: rail
                objectName: "truePeakMeterSharedRail"
                Layout.minimumWidth: root.compactForLimiter ? 225 : 278
                Layout.preferredWidth: root.compactForLimiter ? 225 : 318
                Layout.maximumWidth: root.compactForLimiter ? 240 : 330
                Layout.fillHeight: true
                measurementProvider: root.measurementProvider
                liveProvider: root.liveProvider
                useFamilyAccent: root.useFamilyAccent
                familyAccent: root.familyAccent
                framedForLimiter: root.compactForLimiter
                fullResourceAccent: root.useFamilyAccent ? root.familyAccent : "#6CBCCF"
            }
    }
}
