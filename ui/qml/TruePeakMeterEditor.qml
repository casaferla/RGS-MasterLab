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
    property bool compactForLimiter: true
    // M17 only: Compressor-like GR ribbon in the left meter well. The
    // standalone M16 Meter keeps its original uninterrupted geometry.
    property bool showLimiterGrHistory: false
    property bool showLimiterControls: false
    property bool limiterBypassed: true
    property bool limiterSonicControlsEnabled: false
    property var limiterGrProvider: null
    property var limiterViewModel: null
    property var monitorViewModel: null
    readonly property bool devicePreparing: root.showLimiterGrHistory
        && root.monitorViewModel && root.monitorViewModel.monitorPreparing
    // The lower bay is stable: show PREPARATION only for an actual job.
    // READY, MONITOR, Return to Source, ERROR, BYPASS and NEEDS PREPARATION
    // all show the GR HISTORY chrome. In those states the graph is EMPTY
    // unless its independently authenticated OUTPUT-consumption witness
    // exists; a prepared MASTER is not itself proof of audible GR.
    readonly property string terminalAudioStatus: root.limiterViewModel
        ? root.limiterViewModel.audioStatus : ""
    readonly property bool limiterPreparationVisible: root.showLimiterGrHistory
        && (root.devicePreparing
            || (root.limiterViewModel
                && root.terminalAudioStatus.startsWith("Preparing")))
    // The entire lower band is fixed. In this combined layout,
    // GR and Ceiling+Verification Trim use the SAME 198px height.
    // Canonical 548px M17 editor retains its accepted 198px lower band.
    // In the real minimum-height Main window the editor may receive ~420px,
    // unlike the isolated 548px UI fixture. Compress VERTICALLY only:
    // no column count switch, clipping, scrolling or hidden sonic controls.
    // Preserve 198px at the canonical 548px editor, but do not force
    // readout cards outside the short physical Main window. This is
    // vertical-only compression; stereo L/R meter gets all spare height.
    // At minimum height the rail must reserve the complete 56px
    // numeric field+slider pair, its ceiling card padding AND the
    // distinct read-only Verification Trim. Reducing to 102px made the
    // two cards mathematically overlap even with no scrolling.
    readonly property int lowerControlBandHeight: height < 420
        ? Math.max(120, Math.round(height * 0.39))
        : Math.min(198, Math.max(145, Math.round(height * 0.365)))
    // The M16 provenance well and M17 GR well can give up their spare
    // blank space at the constrained 680x338 host. The right rail keeps
    // the complete Ceiling + Verification Trim reservation. Upper L/R
    // meter positions stay common to M16 and M17; no scroll or clipping.
    readonly property int leftLowerVisualHeight: root.height < 300
        ? Math.min(root.lowerControlBandHeight,
            Math.max(94, Math.floor(root.height * 0.43)))
        : root.lowerControlBandHeight
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
    // Read only the canonical installed-audio status. While the user has
    // requested ON but the candidate is Preparing, this editor still meters
    // MANUAL pre-limiter PCM; never relabel it as final post-limiter audio.
    readonly property bool nonconformingAudition: root.showLimiterControls
        && root.limiterViewModel
        && root.limiterViewModel.audioStatus === "ACTIVE · NON-CONFORMING AUDITION"
    readonly property bool postLimiterInstalled: root.showLimiterControls
        && root.limiterViewModel
        && (root.limiterViewModel.audioStatus === "ACTIVE · AUDITION UNQUALIFIED"
            || root.nonconformingAudition)
    // NON-CONFORMING audio is actually playing, but the current consumed-
    // output M16 observer admits VERIFIED-only private ownership. Refuse
    // fake pre-limiter LIVE readings and label the unavailable scope.
    readonly property string liveScopeHeading: !root.showLimiterControls
        ? "LIVE PROCESSED TRUE PEAK"
        : root.nonconformingAudition
            ? "POST-LIMITER LIVE TP · NOT CERTIFIED"
        : root.postLimiterInstalled ? "LIVE POST-LIMITER TRUE PEAK"
        : "LIVE PRE-LIMITER TRUE PEAK"

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
                Layout.minimumWidth: 270
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
                        text: root.liveScopeHeading
                        color: root.useFamilyAccent ? root.familyAccent : "#B3C9D9"
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
                            Layout.minimumHeight: root.height < 420 ? 94 : 140
                            Layout.preferredWidth: 117
                            channelName: root.isMono ? "C" : "L"
                            useFamilyAccent: root.useFamilyAccent
                            familyAccent: root.familyAccent
                            compactVertical: true
                            // Same BYP Meter L/R palette even when surrounding
                            // M17 editor/chrome is Dynamics Copper.
                            preserveMeterBarPalette: root.showLimiterControls
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
                            Layout.minimumHeight: root.height < 420 ? 94 : 140
                            Layout.preferredWidth: 117
                            visible: !root.isMono
                            channelName: "R"
                            useFamilyAccent: root.useFamilyAccent
                            familyAccent: root.familyAccent
                            compactVertical: true
                            // Same BYP Meter L/R palette even when surrounding
                            // M17 editor/chrome is Dynamics Copper.
                            preserveMeterBarPalette: root.showLimiterControls
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

                    // Deliberately the SAME visual grammar as Compressor:
                    // one continuous dark well, full-width horizontal rule,
                    // then GR HISTORY label/state and an unframed Canvas.
                    // The rule is a divider, not an extra boxed DSP card.
                    Rectangle {
                        objectName: "m17GainReductionDivider"
                        visible: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        Layout.minimumHeight: 1
                        color: Qt.alpha(root.familyAccent, 0.45)
                    }
                    // M16: authoritative provenance in the SAME lower-band
                    // height where M17 shows authentic gain reduction.
                    TruePeakStatusCard {
                        objectName: "truePeakProvenanceCard"
                        visible: !root.showLimiterControls
                        Layout.fillWidth: true
                        Layout.minimumHeight: root.showLimiterControls ? 0 : root.leftLowerVisualHeight
                        Layout.preferredHeight: root.showLimiterControls ? 0 : root.leftLowerVisualHeight
                        Layout.maximumHeight: root.showLimiterControls ? 0 : root.leftLowerVisualHeight
                        stateLabel: root.measurementProvider
                            ? root.measurementProvider.stateText : "NO SOURCE"
                        qualification: root.measurementProvider
                            ? root.measurementProvider.qualificationText : "NO QUALIFIED MEASUREMENT"
                        identity: root.measurementProvider
                            ? root.measurementProvider.realizationText : "NO PUBLISHED REALIZATION"
                        formatLabel: root.measurementProvider
                            ? root.measurementProvider.sampleRateText : "—"
                    }
                    ColumnLayout {
                        id: grHistory
                        objectName: "m17GainReductionHistory"
                        visible: root.showLimiterGrHistory
                        Layout.fillWidth: true
                        // The WHOLE lower bay must align with M17's two
                        // right-hand control cards, including at minimum
                        // window height. Only the GRAPH is the Compressor's
                        // fixed 54px: a flexible upper clearance keeps
                        // the waveform low and leaves extra growth to L/R.
                        Layout.minimumHeight: root.showLimiterGrHistory ? root.leftLowerVisualHeight : 0
                        Layout.preferredHeight: root.showLimiterGrHistory ? root.leftLowerVisualHeight : 0
                        Layout.maximumHeight: root.showLimiterGrHistory ? root.leftLowerVisualHeight : 0
                        spacing: 2
                        opacity: root.limiterBypassed ? 0.5 : 1.0
                        Item {
                            Layout.fillHeight: root.showLimiterControls
                            Layout.minimumHeight: 0
                            Layout.preferredHeight: 0
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                objectName: "m17GrHistoryHeading"
                                text: root.devicePreparing ? "PREPARING PLAYBACK"
                                    : root.limiterPreparationVisible ? "PREPARATION" : "GR HISTORY"
                                color: "#B3C9D9"
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                                font.weight: Font.Bold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                objectName: "m17GrProvenanceStatus"
                                text: root.devicePreparing ? "MASTER → DEVICE"
                                    : root.limiterPreparationVisible
                                    ? root.limiterBypassed ? "BYPASS · NOT RUNNING"
                                      : root.limiterViewModel
                                        && root.limiterViewModel.audioStatus.startsWith("Preparing")
                                          ? "OFFLINE · IN PROGRESS" : "OFFLINE · NOT RUNNING"
                                    : root.limiterBypassed ? "BYPASS · NO ACTIVE GR"
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
                        ColumnLayout {
                            objectName: "m17PreparationInGrRegion"
                            visible: root.limiterPreparationVisible
                            Layout.fillWidth: true
                            // Reserve real vertical space for phase, elapsed,
                            // central ETA and BOTH interval endpoints. Do not
                            // trade visible information for a hover tooltip.
                            // At the compact host this stays inside the
                            // dynamically reserved GR/Ceiling lower band.
                            readonly property int readableHeight:
                                Math.max(76, Math.min(98, root.leftLowerVisualHeight - 18))
                            Layout.minimumHeight: readableHeight
                            Layout.preferredHeight: readableHeight
                            Layout.maximumHeight: readableHeight
                            spacing: 2
                            ProgressBar {
                                id: m17PrepareProgress
                                objectName: "m17PreparationProgressBar"
                                Layout.fillWidth: true
                                Layout.preferredHeight: 14
                                from: 0
                                to: 1
                                value: root.devicePreparing
                                    ? root.monitorViewModel.devicePreparationFraction
                                    : root.limiterViewModel
                                        ? root.limiterViewModel.preparationFraction : 0
                                indeterminate: root.devicePreparing
                                    ? !root.monitorViewModel.devicePreparationDeterminate
                                    : root.limiterViewModel
                                        && root.limiterViewModel.audioStatus.startsWith("Preparing")
                                        && !root.limiterViewModel.preparationDeterminate
                                opacity: root.devicePreparing || (root.limiterViewModel
                                    && root.limiterViewModel.audioStatus.startsWith("Preparing"))
                                        ? 1.0 : 0.28
                                Accessible.name: root.devicePreparing
                                    ? "Master to playback device conversion progress"
                                    : "Offline limiter preparation progress"
                                // Dynamics copper, the same rounded geometry as Studio controls.
                                background: Rectangle {
                                    radius: 5
                                    color: "#0B1A29"
                                    border.width: 1
                                    border.color: "#6F564A"
                                }
                                contentItem: Item {
                                    clip: true
                                    Rectangle {
                                        visible: !m17PrepareProgress.indeterminate
                                        width: parent.width * m17PrepareProgress.visualPosition
                                        height: parent.height
                                        radius: 4
                                        color: "#C4774A"
                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            height: 2
                                            radius: 1
                                            color: "#EDB48D"
                                            opacity: 0.8
                                        }
                                    }
                                    Rectangle {
                                        id: m17BusyBar
                                        visible: m17PrepareProgress.indeterminate
                                        height: parent.height
                                        width: Math.max(22, parent.width * 0.30)
                                        radius: 4
                                        color: "#C4774A"
                                        NumberAnimation on x {
                                            from: -m17BusyBar.width
                                            to: m17PrepareProgress.width
                                            duration: 1250
                                            loops: Animation.Infinite
                                            running: m17PrepareProgress.indeterminate
                                        }
                                    }
                                }
                            }
                            Text {
                                id: m17PreparationProgressText
                                objectName: "m17PreparationProgressLabel"
                                Layout.fillWidth: true
                                // The backend owns all timings. QML only lays
                                // out the exact observed phase / elapsed /
                                // central estimate / interval on separate
                                // visible lines, including the FIRST Prepare.
                                text: {
                                    if (root.devicePreparing) {
                                        const v = root.monitorViewModel
                                        const rate = hz => (hz / 1000).toFixed(1)
                                            .replace(".0", "") + " kHz"
                                        const pair = rate(v.masterRateHz) + " → "
                                            + rate(v.pendingDeviceRateHz)
                                        const pct = v.devicePreparationDeterminate
                                            ? " · " + Math.floor(v.devicePreparationFraction * 100) + "%"
                                            : " · measuring"
                                        return pair + pct
                                            + (v.devicePreparationEta.length
                                               ? "\n" + v.devicePreparationEta : "")
                                    }
                                    const v = root.limiterViewModel
                                    if (!v) return "PREPARATION UNAVAILABLE"
                                    if (!v.audioStatus.startsWith("Preparing"))
                                        return v.audioStatus
                                    const pct = v.preparationDeterminate
                                        ? " · " + Math.floor(v.preparationFraction * 100) + "%"
                                        : " · measuring"
                                    let eta = v.preparationEta
                                    // A presentation split, never fabricated
                                    // progress: 'Elapsed ... · Remaining ...'
                                    // becomes separate permanently visible
                                    // lines. Preserve the backend interval.
                                    eta = eta.replace(" · ", "\n")
                                    const intervalAt = eta.lastIndexOf(" (")
                                    if (intervalAt >= 0 && eta.endsWith(")"))
                                        eta = eta.slice(0, intervalAt)
                                            + "\nInterval "
                                            + eta.slice(intervalAt + 2, -1)
                                    return v.preparationStage + pct
                                        + (eta.length ? "\n" + eta : "")
                                }
                                color: "#A1B5C9"
                                font.family: "Segoe UI"
                                font.pixelSize: root.height < 420 ? 9 : 10
                                wrapMode: Text.WordWrap
                                elide: Text.ElideNone
                                Layout.minimumHeight: implicitHeight
                                Layout.preferredHeight: implicitHeight
                                Accessible.name: text
                            }
                        }
                        TruePeakGrHistoryGraph {
                            id: grHistoryGraph
                            visible: !root.limiterPreparationVisible
                            Layout.fillWidth: true
                            // Fixed single linked 54px GR lane, just like
                            // Compressor. Never stretch with window height.
                            Layout.fillHeight: false
                            Layout.minimumHeight: root.height < 420 ? 42 : 54
                            Layout.preferredHeight: root.height < 420 ? 42 : 54
                            Layout.maximumHeight: root.height < 420 ? 42 : 54
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
                Layout.minimumWidth: 225
                Layout.preferredWidth: 240
                Layout.maximumWidth: 260
                Layout.fillHeight: true
                measurementProvider: root.measurementProvider
                liveProvider: root.liveProvider
                useFamilyAccent: root.useFamilyAccent
                familyAccent: root.familyAccent
                framedForLimiter: true
                limiterControlsIncluded: root.showLimiterControls
                limiterViewModel: root.limiterViewModel
                limiterBypassed: root.limiterBypassed
                limiterSonicControlsEnabled: root.limiterSonicControlsEnabled
                lowerControlBandHeight: root.lowerControlBandHeight
                fullResourceAccent: root.useFamilyAccent ? root.familyAccent : "#6CBCCF"
            }
    }
}
