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
    // M17 only: Compressor-like GR ribbon in the left meter well. The
    // standalone M16 Meter keeps its original uninterrupted geometry.
    property bool showLimiterGrHistory: false
    property bool showLimiterControls: false
    property bool limiterBypassed: true
    property bool limiterSonicControlsEnabled: false
    property var limiterGrProvider: null
    property var limiterViewModel: null
    // The entire lower band is fixed. In this combined layout,
    // GR and Ceiling+Verification Trim use the SAME 198px height.
    readonly property int lowerControlBandHeight: 100 + 92 + 6
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

                    // Deliberately the SAME visual grammar as Compressor:
                    // one continuous dark well, full-width horizontal rule,
                    // then GR HISTORY label/state and an unframed Canvas.
                    // The rule is a divider, not an extra boxed DSP card.
                    Rectangle {
                        objectName: "m17GainReductionDivider"
                        visible: root.showLimiterGrHistory
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        Layout.minimumHeight: 1
                        color: Qt.alpha(root.familyAccent, 0.45)
                    }
                    ColumnLayout {
                        id: grHistory
                        objectName: "m17GainReductionHistory"
                        visible: root.showLimiterGrHistory
                        Layout.fillWidth: true
                        // Intrinsic height exactly follows Compressor
                        // GR ribbon: 10px header + 2px spacing + 54px
                        // single linked-lane Canvas. Never consume surplus
                        // vertical space: the L/R meter RowLayout does.
                        Layout.minimumHeight: root.showLimiterControls
                            ? root.lowerControlBandHeight : implicitHeight
                        Layout.preferredHeight: root.showLimiterControls
                            ? root.lowerControlBandHeight : implicitHeight
                        Layout.maximumHeight: root.showLimiterControls
                            ? root.lowerControlBandHeight : implicitHeight
                        spacing: 2
                        opacity: root.limiterBypassed ? 0.5 : 1.0
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                objectName: "m17GrHistoryHeading"
                                text: "GR HISTORY"
                                color: "#B3C9D9"
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                                font.weight: Font.Bold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                objectName: "m17GrProvenanceStatus"
                                text: root.limiterBypassed ? "BYPASS · NO ACTIVE GR"
                                    : grHistoryGraph.verifiedSeriesAvailable
                                        ? "VERIFIED · CONSUMED GR"
                                        : "UNAVAILABLE · NO VERIFIED GR"
                                color: "#91A8B7"
                                font.family: "Segoe UI"
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                HoverHandler { id: grStatusHover }
                                ToolTip.text: "The grid is a reference, not an observed gain-reduction series. Verified applied limiter gain and sink-consumed OUTPUT identity are required before drawing GR."
                                ToolTip.visible: (grStatusHover.hovered) && stationaryTooltipDwell1.ready
                                ToolTip.delay: 0
                                StudioTooltipDwell {
                                    id: stationaryTooltipDwell1
                                    anchors.fill: parent
                                }
                            }
                        }
                        TruePeakGrHistoryGraph {
                            id: grHistoryGraph
                            Layout.fillWidth: true
                            Layout.fillHeight: root.showLimiterControls
                            Layout.minimumHeight: implicitHeight
                            Layout.preferredHeight: root.showLimiterControls
                                ? -1 : implicitHeight
                            Layout.maximumHeight: root.showLimiterControls
                                ? -1 : implicitHeight
                            familyAccent: root.familyAccent
                            backendVerified: root.limiterGrProvider
                                ? root.limiterGrProvider.verified : false
                            verifiedHistory: root.limiterGrProvider
                                ? root.limiterGrProvider.verifiedSeries : []
                            bypassed: root.limiterBypassed
                        }
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
                limiterControlsIncluded: root.showLimiterControls
                limiterViewModel: root.limiterViewModel
                limiterBypassed: root.limiterBypassed
                limiterSonicControlsEnabled: root.limiterSonicControlsEnabled
                lowerControlBandHeight: root.lowerControlBandHeight
                fullResourceAccent: root.useFamilyAccent ? root.familyAccent : "#6CBCCF"
            }
    }
}
