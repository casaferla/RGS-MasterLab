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
    readonly property bool hasMeasurement: measurementProvider
        ? measurementProvider.hasMeasurement : false
    readonly property int measuredChannels: root.showLive
        ? root.liveProvider.channelCount
        : root.hasMeasurement ? root.measurementProvider.measuredChannelCount : 0
    readonly property bool isMono: root.measuredChannels === 1

    ScrollView {
        id: meterScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        RowLayout {
            width: meterScroll.availableWidth
            height: Math.max(460, meterScroll.availableHeight)
            spacing: 12

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
                    anchors.margins: 14
                    spacing: 12
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
                            meterKind: root.showLive ? "RECENT 100 MS" : "STATIC RESOURCE"
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
                            meterKind: root.showLive ? "RECENT 100 MS" : "STATIC RESOURCE"
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Text {
                        text: "Scale −60 to +6 dBTP · coral only above 0 dBTP · no Ceiling selected"
                        color: "#8EAAB9"
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        text: root.showLive
                            ? "LIVE · " + root.liveProvider.statusText
                              + " · HOLD 1 s: " + root.liveProvider.holdText
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
}
