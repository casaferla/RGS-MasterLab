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
    readonly property string provenanceDetails: root.measurementProvider
        ? root.measurementProvider.stateText + "\n"
            + root.measurementProvider.qualificationText + "\n"
            + root.measurementProvider.realizationText + "\n"
            + root.measurementProvider.sampleRateText
        : "NO SOURCE\nNO QUALIFIED MEASUREMENT\nNO PUBLISHED REALIZATION\n—"
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
        anchors.margins: root.limiterControlsIncluded ? 9
            : root.framedForLimiter ? 8 : 0
        height: root.framedForLimiter ? undefined : implicitHeight
        // M17 lower band (198px) + three 100px reflow cards must fit the
        // 548px minimum editor height with TWO nonzero centering springs.
        // Six outer gaps at 4px fit; at 6px they exhaust the min-height
        // free space. Keep the standalone M16 spacing untouched.
        spacing: root.limiterControlsIncluded ? 4
            : root.framedForLimiter ? 6 : root.compactRail ? 3 : 10
        // This upper spacer and its lower twin share extra height equally.
        Item {
            objectName: "m17ReadoutUpperSpacer"
            visible: root.limiterControlsIncluded
            Layout.fillHeight: root.limiterControlsIncluded
            Layout.preferredHeight: 0
            Layout.minimumHeight: 0
        }

        TruePeakReadoutCard {
            objectName: "truePeakRecentCard"
            roomyLimiterReadout: root.limiterControlsIncluded
            additionalTooltip: root.limiterControlsIncluded
                ? root.provenanceDetails : ""
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: root.framedForLimiter && !root.limiterControlsIncluded
            useFamilyAccent: root.useFamilyAccent
            familyAccent: root.familyAccent
            Layout.fillWidth: true
            Layout.preferredHeight: root.limiterControlsIncluded ? 100
                : root.compactRail ? 68 : 108
            Layout.maximumHeight: root.limiterControlsIncluded ? 100 : -1
            Layout.minimumHeight: root.limiterControlsIncluded ? 100 : 66
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
            roomyLimiterReadout: root.limiterControlsIncluded
            additionalTooltip: root.limiterControlsIncluded
                ? root.provenanceDetails : ""
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: root.framedForLimiter && !root.limiterControlsIncluded
            useFamilyAccent: root.useFamilyAccent
            familyAccent: root.familyAccent
            Layout.fillWidth: true
            Layout.preferredHeight: root.limiterControlsIncluded ? 100
                : root.compactRail ? 68 : 108
            Layout.maximumHeight: root.limiterControlsIncluded ? 100 : -1
            Layout.minimumHeight: root.limiterControlsIncluded ? 100 : 66
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
            roomyLimiterReadout: root.limiterControlsIncluded
            additionalTooltip: root.limiterControlsIncluded
                ? root.provenanceDetails : ""
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: root.framedForLimiter && !root.limiterControlsIncluded
            useFamilyAccent: root.useFamilyAccent
            familyAccent: root.familyAccent
            Layout.fillWidth: true
            Layout.preferredHeight: root.limiterControlsIncluded ? 100
                : root.compactRail ? 68 : 108
            Layout.maximumHeight: root.limiterControlsIncluded ? 100 : -1
            Layout.minimumHeight: root.limiterControlsIncluded ? 100 : 66
            // The complete-source M16 analysis does NOT become a complete
            // post-M17 peak certificate when Ceiling changes. Keep it
            // independent from verified post-limiter RECENT/HOLD evidence.
            heading: root.limiterControlsIncluded
                ? "COMPLETE PRE-LIMITER TP"
                : "COMPLETE PROCESSED RESOURCE"
            valueColor: root.fullResourceAccent
            reading: root.hasMeasurement
                ? root.measurementProvider.peakText : "—"
            // Same bounded text width as the accepted 100px M17 card.
            // PRE-LIMITER scope is explicit in heading and full tooltip;
            // long provisional status strings must not overflow the card.
            detail: root.hasMeasurement
                ? "Peak channel: " + root.measurementProvider.channelText
                : "No complete, source-bound measurement"
            available: root.hasMeasurement
        }
        TruePeakStatusCard {
            objectName: "truePeakProvenanceCard"
            visible: !root.limiterControlsIncluded
            detailTooltipEnabled: root.showProvenanceTooltips
            Layout.fillHeight: root.framedForLimiter && !root.limiterControlsIncluded
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
            visible: !root.limiterControlsIncluded
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
        // A second spring centers all three readouts IN THE UPPER AREA.
        Item {
            objectName: "m17ReadoutLowerSpacer"
            visible: root.limiterControlsIncluded
            Layout.fillHeight: root.limiterControlsIncluded
            Layout.preferredHeight: 0
            Layout.minimumHeight: 0
        }
        Rectangle {
            objectName: "m17LimiterControlsDivider"
            visible: root.limiterControlsIncluded
            Layout.fillWidth: true
            Layout.minimumHeight: 1
            Layout.preferredHeight: 1
            Layout.maximumHeight: 1
            color: Qt.alpha(root.familyAccent, 0.45)
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
                Layout.minimumHeight: 100
                Layout.preferredHeight: 100
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
                Layout.preferredHeight: 92
                Layout.minimumHeight: 92
                radius: 6
                color: "#081521"
                border.color: root.familyAccent
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
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

    }
}
