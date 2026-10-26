import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

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
    property bool limiterControlsIncluded: false
    property bool limiterBypassed: true
    property bool limiterSonicControlsEnabled: false
    property var limiterViewModel: null
    property int lowerControlBandHeight: 198
    // Continuous vertical fit into actual Main workspace. All canonical
    // >=548px heights remain byte-identical to the accepted M17 layout.
    readonly property int m17ReadoutHeight: Math.min(100, Math.max(
        height < 420 ? 42 : 56,
        Math.floor((height - lowerControlBandHeight - 44) / 3)))
    readonly property int m17CeilingHeight: height < 420
        ? 68
        : Math.min(100, Math.max(70,
            Math.round(70 + (lowerControlBandHeight - 145) * 30 / 53)))
    readonly property int m17TrimHeight:
        lowerControlBandHeight - m17CeilingHeight - 6
    readonly property string provenanceDetails: root.measurementProvider
        ? root.measurementProvider.stateText + "\n"
            + root.measurementProvider.qualificationText + "\n"
            + root.measurementProvider.realizationText + "\n"
            + root.measurementProvider.sampleRateText
        : "NO SOURCE\nNO QUALIFIED MEASUREMENT\nNO PUBLISHED REALIZATION\n—"
    property bool showProvenanceTooltips: true
    property bool useFamilyAccent: false
    property color familyAccent: "#6CBCCF"
    readonly property bool hasMeasurement: measurementProvider
        ? measurementProvider.hasMeasurement : false
    readonly property bool nonconformingAudition: root.limiterControlsIncluded
        && root.limiterViewModel
        && root.limiterViewModel.audioStatus === "ACTIVE · NON-CONFORMING AUDITION"
    readonly property bool hasMeasuredCeilingMiss:
        root.limiterControlsIncluded && root.limiterViewModel
        && typeof root.limiterViewModel.nonconformingPeakText === "string"
        && root.limiterViewModel.nonconformingPeakText.length > 0
        && root.limiterViewModel.audioStatus.indexOf("NON-CONFORMING") >= 0
    implicitWidth: 316
    implicitHeight: 330
    readonly property bool compactRail: framedForLimiter || height < 530
    property color fullResourceAccent: "#6CBCCF"

    // A complete panel boundary, not just four individually outlined cards.
    Rectangle {
        objectName: "truePeakSharedRailFrame"
        anchors.fill: parent
        visible: true
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
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.verticalCenter: undefined
        anchors.margins: root.height < 420 ? 5 : 8
        height: undefined
        // M17 lower band (198px) + three 100px reflow cards must fit the
        // 548px minimum editor height with TWO nonzero centering springs.
        // Six outer gaps at 4px fit; at 6px they exhaust the min-height
        // free space. Keep the standalone M16 spacing untouched.
        spacing: 4
        // This upper spacer and its lower twin share extra height equally.
        Item {
            objectName: "m17ReadoutUpperSpacer"
            visible: false
            Layout.fillHeight: false
            Layout.preferredHeight: 0
            Layout.minimumHeight: 0
        }

        TruePeakReadoutCard {
            objectName: "truePeakRecentCard"
            roomyLimiterReadout: true
            additionalTooltip: root.limiterControlsIncluded
                ? root.provenanceDetails : ""
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: false
            useFamilyAccent: root.useFamilyAccent
            familyAccent: root.familyAccent
            Layout.fillWidth: true
            Layout.preferredHeight: root.m17ReadoutHeight
            Layout.maximumHeight: root.m17ReadoutHeight
            Layout.minimumHeight: root.m17ReadoutHeight
            heading: "RECENT / LIVE TRUE PEAK"
            reading: root.liveProvider && root.liveProvider.recentAvailable
                ? root.liveProvider.peakText : "UNAVAILABLE"
            detail: root.nonconformingAudition
                ? "NON-CONFORMING PCM · LIVE NOT CERTIFIED"
                : root.liveProvider && root.liveProvider.monitoringAttempted
                    ? root.liveProvider.statusText
                    : "NOT MONITORED · select PROCESSED and press Play"
            available: root.liveProvider ? root.liveProvider.recentAvailable : false
        }
        TruePeakReadoutCard {
            objectName: "truePeakHoldCard"
            roomyLimiterReadout: true
            additionalTooltip: root.limiterControlsIncluded
                ? root.provenanceDetails : ""
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: false
            useFamilyAccent: root.useFamilyAccent
            familyAccent: root.familyAccent
            Layout.fillWidth: true
            Layout.preferredHeight: root.m17ReadoutHeight
            Layout.maximumHeight: root.m17ReadoutHeight
            Layout.minimumHeight: root.m17ReadoutHeight
            heading: "SHORT HOLD / 1 SECOND"
            keyColor: "#F2C66F"
            valueColor: "#F2C66F"
            reading: root.liveProvider && root.liveProvider.holdAvailable
                ? root.liveProvider.holdText : "UNAVAILABLE"
            detail: root.nonconformingAudition
                ? "NON-CONFORMING PCM · LIVE NOT CERTIFIED"
                : root.liveProvider && root.liveProvider.monitoringAttempted
                    ? "Max over the last fully covered 1 s of PROCESSED"
                    : "NOT MONITORED"
            available: root.liveProvider ? root.liveProvider.holdAvailable : false
        }
        TruePeakReadoutCard {
            objectName: "truePeakFullResourceCard"
            roomyLimiterReadout: true
            additionalTooltip: root.limiterControlsIncluded
                ? root.provenanceDetails : ""
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: false
            useFamilyAccent: root.useFamilyAccent
            familyAccent: root.familyAccent
            Layout.fillWidth: true
            Layout.preferredHeight: root.m17ReadoutHeight
            Layout.maximumHeight: root.m17ReadoutHeight
            Layout.minimumHeight: root.m17ReadoutHeight
            // The complete-source M16 analysis does NOT become a complete
            // post-M17 peak certificate when Ceiling changes. Keep it
            // independent from verified post-limiter RECENT/HOLD evidence.
            heading: root.hasMeasuredCeilingMiss
                ? "COMPLETE POST-LIMITER TP · NON-CONFORMING"
                : root.limiterControlsIncluded
                    ? "COMPLETE PRE-LIMITER TP"
                    : "COMPLETE PROCESSED RESOURCE"
            valueColor: root.hasMeasuredCeilingMiss ? "#F27683"
                : root.fullResourceAccent
            reading: root.hasMeasuredCeilingMiss
                ? root.limiterViewModel.nonconformingPeakText
                : root.hasMeasurement
                    ? root.measurementProvider.peakText : "—"
            // Same bounded text width as the accepted 100px M17 card.
            // PRE-LIMITER scope is explicit in heading and full tooltip;
            // long provisional status strings must not overflow the card.
            detail: root.hasMeasuredCeilingMiss
                ? "OVER " + root.limiterViewModel.nonconformingExcessText
                    + " · target " + root.limiterViewModel.nonconformingTargetText
                : root.hasMeasurement
                    ? "Peak channel: " + root.measurementProvider.channelText
                    : "No complete, source-bound measurement"
            available: root.hasMeasuredCeilingMiss || root.hasMeasurement
        }
        TruePeakStatusCard {
            objectName: "truePeakProvenanceCard"
            visible: false
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: false
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
        // A second spring centers all three readouts IN THE UPPER AREA.
        Item {
            objectName: "m17ReadoutLowerSpacer"
            visible: false
            Layout.fillHeight: false
            Layout.preferredHeight: 0
            Layout.minimumHeight: 0
        }
        Rectangle {
            objectName: "m17LimiterControlsDivider"
            // Fixed shared topology. M16 reserves exactly the same right-
            // rail lower band as M17 so the first three cards never jump.
            visible: true
            Layout.fillWidth: true
            Layout.minimumHeight: 1
            Layout.preferredHeight: 1
            Layout.maximumHeight: 1
            color: Qt.alpha(root.familyAccent, 0.45)
        }
        Item {
            objectName: "m16UnusedLowerControlBand"
            visible: !root.limiterControlsIncluded
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.minimumHeight: root.lowerControlBandHeight
            Layout.preferredHeight: root.lowerControlBandHeight
            Layout.maximumHeight: root.lowerControlBandHeight
        }
        ColumnLayout {
            objectName: "m17LimiterLowerControlGroup"
            visible: root.limiterControlsIncluded
            Layout.fillWidth: true
            Layout.minimumHeight: root.lowerControlBandHeight
            Layout.preferredHeight: root.lowerControlBandHeight
            Layout.maximumHeight: root.lowerControlBandHeight
            spacing: 6
            Rectangle {
                id: ceilingCard
                objectName: "m17CeilingCard"
                Layout.fillWidth: true
                opacity: root.limiterBypassed ? 0.5 : 1.0
                // The same dwell policy as every other hint. The card only
                // explains disabled state; editable controls own their hints.
                StudioTooltipDwell {
                    id: ceilingCardTooltipDwell
                    anchors.fill: parent
                    canShow: root.limiterBypassed
                }
                ToolTip.text: "True Peak Ceiling is retained, but cannot be edited while bypassed."
                ToolTip.visible: ceilingCardTooltipDwell.ready
                ToolTip.delay: 0
                Layout.minimumHeight: root.m17CeilingHeight
                Layout.preferredHeight: root.m17CeilingHeight
                radius: 6
                color: "#10273A"
                border.color: root.familyAccent
                border.width: 1
            
                StudioNumericSliderPair {
                id: ceilingControl
                objectName: "m17CeilingControl"
                fieldObjectName: "m17CeilingNumericField"
                sliderObjectName: "m17CeilingSlider"
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 10
                enabled: root.limiterSonicControlsEnabled
                opacity: root.limiterBypassed ? 1.0
                    : root.limiterSonicControlsEnabled ? 1.0 : 0.48
                viewModel: root.limiterViewModel
                labelText: "CEILING"
                unitText: "dBTP"
                fieldName: "ceilingDbTp"
                interactionHint: "Type an exact Ceiling in [-12.0, 0.0] dBTP; Enter confirms, Esc cancels."
                sliderInteractionHint: "Adjust Ceiling from -12.0 to 0.0 dBTP; pointer release commits once."
                rawText: root.limiterViewModel
                    ? root.limiterViewModel.ceilingDbTpText : "-1.00"
                from: -12.0
                to: 0.0
                value: root.limiterViewModel
                    ? root.limiterViewModel.ceilingDbTp : -1.0
                stepSize: 0.1
                compact: true
                compactFieldWidth: 120
                fillFromOrigin: true
                positionFillOrigin: 0.0
                positionFillColor: Qt.alpha("#C4774A", 0.38)
                accentColor: root.familyAccent
                focusAccentOnly: true
                sliderAccessibleName: "True Peak Ceiling"
                }
            }
            Rectangle {
                objectName: "m17VerificationTrim"
                Layout.fillWidth: true
                Layout.preferredHeight: root.m17TrimHeight
                Layout.minimumHeight: root.m17TrimHeight
                radius: 6
                color: "transparent"
                border.width: 0
                opacity: root.limiterBypassed ? 0.5 : 1.0
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 7
                    Text {
                        text: "VERIFICATION TRIM"
                        color: root.familyAccent
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    Text {
                        objectName: "m17TrimProvenanceStatus"
                        StudioTooltipDwell {
                            id: trimTooltipDwell
                            anchors.fill: parent
                        }
                        ToolTip.text: "Verification Trim is a distinct measured observation, not an adjustable sonic control. Status remains unavailable until verified."
                        ToolTip.visible: trimTooltipDwell.ready
                        ToolTip.delay: 0
                        text: "UNAVAILABLE · NOT VERIFIED"
                        color: "#B3C9D9"
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        Layout.fillWidth: true
                        // Compact 46px Trim well cannot accommodate
                        // a two-line status beneath its title and margins.
                        // The full exact status remains in its dwell tooltip.
                        wrapMode: root.height < 420 ? Text.NoWrap : Text.WordWrap
                        elide: root.height < 420 ? Text.ElideRight : Text.ElideNone
                    }
                }
            }
        }

    }
}
