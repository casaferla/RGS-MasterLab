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
    readonly property bool showLive: liveProvider
        ? liveProvider.monitoringAttempted : false
    readonly property bool liveAvailable: root.showLive
        && liveProvider.recentAvailable
    readonly property bool holdAvailable: root.showLive
        && liveProvider.holdAvailable
    readonly property bool hasMeasurement: measurementProvider
        ? measurementProvider.hasMeasurement : false
    readonly property int measuredChannels: root.showLive
        ? root.liveProvider.channelCount
        : root.hasMeasurement ? root.measurementProvider.measuredChannelCount : 0
    readonly property bool isMono: root.measuredChannels === 1

    // Canonical minimum window is a fully visible editor, never a scroller.
    // The Qt layout itself distributes the available pixel height.
    RowLayout {
        objectName: "truePeakContentLayout"
        anchors.fill: parent
        spacing: 10

            Rectangle {
                id: meterBay
                objectName: "truePeakStaticMeterBay"
                Layout.fillWidth: true
                Layout.minimumWidth: 340
                Layout.fillHeight: true
                radius: 8
                color: "#0A1C2D"
                border.color: "#31506B"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 9
                    spacing: 5
                    Text {
                        text: root.showLive ? "LIVE PROCESSED TRUE PEAK"
                                            : "STATIC FULL-RESOURCE TRUE PEAK"
                        color: "#B3C9D9"
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 12
                        Item { Layout.fillWidth: true }
                        TruePeakStaticLane {
                            objectName: "truePeakLeftLane"
                            Layout.fillHeight: true
                            Layout.preferredWidth: 135
                            channelName: root.isMono ? "C" : "L"
                            reading: root.showLive
                                ? root.liveProvider.leftText
                                : root.hasMeasurement
                                    ? root.measurementProvider.fullLeftText : "—"
                            available: root.showLive ? root.liveAvailable : root.hasMeasurement
                            level: root.showLive
                                ? root.liveProvider.leftLevel
                                : root.hasMeasurement
                                    ? root.measurementProvider.fullLeftMeterLevel : 0
                            aboveDigitalZero: root.showLive
                                ? root.liveAvailable && root.liveProvider.leftOverZero
                                : root.hasMeasurement
                                    && root.measurementProvider.fullLeftOverZero
                            meterKind: root.showLive ? "RECENT 100 ms" : "STATIC RESOURCE"
                            holdAvailable: root.holdAvailable
                            holdLevel: root.holdAvailable
                                ? root.liveProvider.leftHoldLevel : 0.0
                        }
                        TruePeakStaticLane {
                            objectName: "truePeakRightLane"
                            Layout.fillHeight: true
                            Layout.preferredWidth: 135
                            visible: !root.isMono
                            channelName: "R"
                            reading: root.showLive
                                ? root.liveProvider.rightText
                                : root.measuredChannels === 2
                                    ? root.measurementProvider.fullRightText : "—"
                            available: root.showLive
                                ? root.liveAvailable && root.measuredChannels === 2
                                : root.measuredChannels === 2
                            level: root.showLive
                                ? root.liveProvider.rightLevel
                                : root.measuredChannels === 2
                                    ? root.measurementProvider.fullRightMeterLevel : 0
                            aboveDigitalZero: root.showLive
                                ? root.liveAvailable && root.liveProvider.rightOverZero
                                : root.measuredChannels === 2
                                    && root.measurementProvider.fullRightOverZero
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
                        text: root.showLive
                            ? "RECENT 100 ms · " + root.liveProvider.statusText
                            : "Full-resource maxima · STATIC, not audible LIVE."
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
                Layout.minimumWidth: 278
                Layout.preferredWidth: 318
                Layout.maximumWidth: 330
                Layout.fillHeight: true
                measurementProvider: root.measurementProvider
                liveProvider: root.liveProvider
            }
    }
}
