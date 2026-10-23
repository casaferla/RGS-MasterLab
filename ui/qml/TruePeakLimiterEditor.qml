import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Gate M Option B (PO 2026-10-23): M16 owns the ONE consistent set of
// True Peak L/R meters, recent/hold/complete measurements and provenance.
// M17 owns ONLY offline finalization, authentic GR, Ceiling and verification.
// Never mount scaled copies of TruePeakMeterEditor or TruePeakStaticLane here.
Item {
    id: root
    objectName: "truePeakLimiterSurface"
    property var limiterModule: null
    property bool showLocalValidation: true
    property var limiterGrProvider: null
    property var limiterViewModel: null
    property var measurementProvider: null
    property var liveProvider: null
    readonly property bool bypassed: !limiterModule || limiterModule.bypass
    readonly property color familyAccent: bypassed ? "#6CBCCF" : "#C4774A"
    readonly property string validationMessage: limiterViewModel
        ? (limiterViewModel.validationMessage || "") : ""
    readonly property bool hasValidationError: validationMessage.length > 0
    readonly property bool sonicControlsEnabled: !bypassed
        && limiterViewModel !== null && limiterViewModel !== undefined
    implicitWidth: 710
    implicitHeight: 330

    RowLayout {
        objectName: "m17FinalizationOnlyLayout"
        anchors.fill: parent
        spacing: 10

        Rectangle {
            id: grBay
            objectName: "m17GrOnlyBay"
            Layout.fillWidth: true
            Layout.minimumWidth: 250
            Layout.fillHeight: true
            radius: 8
            color: "#0A1C2D"
            border.color: root.familyAccent
            border.width: 1
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Text {
                    text: "GAIN REDUCTION · OFFLINE"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: root.familyAccent
                    Layout.fillWidth: true
                }
                Text {
                    objectName: "m17MeterOwnerHint"
                    text: "True Peak L/R, RECENT, HOLD and complete measurement: True Peak Meter (M16)"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    color: "#9CB4C5"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Text {
                    objectName: "m17PreparationState"
                    text: root.limiterViewModel
                        ? root.limiterViewModel.audioStatus : "UNAVAILABLE"
                    color: "#B3C9D9"
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Item {
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                }
                Rectangle {
                    objectName: "m17GainReductionDivider"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Qt.alpha(root.familyAccent, 0.45)
                }
                ColumnLayout {
                    id: grHistory
                    objectName: "m17GainReductionHistory"
                    Layout.fillWidth: true
                    Layout.minimumHeight: 77
                    Layout.preferredHeight: 77
                    Layout.maximumHeight: 77
                    opacity: root.bypassed ? 0.5 : 1.0
                    spacing: 5
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
                            text: root.bypassed ? "BYPASS · NO ACTIVE GR"
                                : grHistoryGraph.verifiedSeriesAvailable
                                    ? "VERIFIED · CONSUMED GR"
                                    : "UNAVAILABLE · NO VERIFIED GR"
                            color: "#91A8B7"
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                            HoverHandler { id: grHover }
                            ToolTip.text: "Grid is a reference, not a measured GR trace. Only sink-consumed OUTPUT_RATE gain witnesses are eligible."
                            ToolTip.visible: grHover.hovered && grDwell.ready
                            ToolTip.delay: 0
                            StudioTooltipDwell { id: grDwell; anchors.fill: parent }
                        }
                    }
                    TruePeakGrHistoryGraph {
                        id: grHistoryGraph
                        Layout.fillWidth: true
                        Layout.minimumHeight: 54
                        Layout.preferredHeight: 54
                        Layout.maximumHeight: 54
                        familyAccent: root.familyAccent
                        backendVerified: root.limiterGrProvider
                            ? root.limiterGrProvider.verified : false
                        verifiedHistory: root.limiterGrProvider
                            ? root.limiterGrProvider.verifiedSeries : []
                        bypassed: root.bypassed
                    }
                }
            }
        }

        Rectangle {
            id: controlBay
            objectName: "m17FinalizationControlsBay"
            Layout.minimumWidth: 268
            Layout.preferredWidth: 300
            Layout.maximumWidth: 340
            Layout.fillHeight: true
            radius: 8
            color: "#0A1C2D"
            border.color: root.familyAccent
            border.width: 1
            ColumnLayout {
                objectName: "m17LimiterLowerControlGroup"
                anchors.fill: parent
                anchors.margins: 10
                spacing: 9

                Rectangle {
                    id: ceilingCard
                    objectName: "m17CeilingCard"
                    Layout.fillWidth: true
                    Layout.minimumHeight: 130
                    Layout.preferredHeight: 130
                    radius: 6
                    opacity: root.bypassed ? 0.5 : 1.0
                    color: "#10273A"
                    border.color: root.familyAccent
                    border.width: 1
                    StudioTooltipDwell {
                        id: ceilingDwell
                        anchors.fill: parent
                        canShow: root.bypassed
                    }
                    ToolTip.text: "Ceiling is retained while bypassed, but only Prepare creates a verified candidate."
                    ToolTip.visible: ceilingDwell.ready
                    ToolTip.delay: 0
                    StudioNumericSliderPair {
                        objectName: "m17CeilingControl"
                        fieldObjectName: "m17CeilingNumericField"
                        sliderObjectName: "m17CeilingSlider"
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.margins: 12
                        enabled: root.sonicControlsEnabled
                        opacity: root.sonicControlsEnabled ? 1.0 : 0.48
                        viewModel: root.limiterViewModel
                        labelText: "CEILING"
                        unitText: "dBTP"
                        fieldName: "ceilingDbTp"
                        interactionHint: "Exact Ceiling [-12.0, 0.0] dBTP; Enter confirms, Esc cancels."
                        sliderInteractionHint: "Adjust Ceiling [-12, 0] dBTP; pointer release commits once."
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
                    Layout.minimumHeight: 84
                    Layout.preferredHeight: 84
                    radius: 6
                    color: "#081521"
                    border.color: root.familyAccent
                    opacity: root.bypassed ? 0.5 : 1.0
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 5
                        Text {
                            text: "VERIFICATION TRIM"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.bold: true
                            color: root.familyAccent
                            Layout.fillWidth: true
                        }
                        Text {
                            objectName: "m17TrimProvenanceStatus"
                            text: "UNAVAILABLE · NOT VERIFIED"
                            color: "#B3C9D9"
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            StudioTooltipDwell { id: trimDwell; anchors.fill: parent }
                            ToolTip.text: "Verification Trim is measured, not a user sonic control. No verified observation is currently bound."
                            ToolTip.visible: trimDwell.ready
                            ToolTip.delay: 0
                        }
                    }
                }
                Item { Layout.fillHeight: true; Layout.minimumHeight: 0 }
            }
        }
    }

    // Host uses one fixed global error footer. Isolated fixture-only overlay
    // must not change the control or GR rectangles on appearance/disappearance.
    Rectangle {
        id: validationStatus
        objectName: "m17LimiterValidationStatusRegion"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        width: 225
        height: 16
        z: 20
        radius: 3
        color: "#E012243F"
        visible: root.showLocalValidation && root.hasValidationError
        Text {
            objectName: "m17LimiterValidationMessage"
            anchors.fill: parent
            anchors.margins: 3
            text: root.validationMessage
            color: "#F27683"
            font.family: "Segoe UI"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            HoverHandler { id: validationHover }
            ToolTip.text: parent ? root.validationMessage : ""
            ToolTip.visible: validationHover.hovered && validationDwell.ready
            ToolTip.delay: 0
            StudioTooltipDwell { id: validationDwell; anchors.fill: parent }
        }
    }
}
