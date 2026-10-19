import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// M17 shared three-column UI candidate. No production limiter adapter or
// source-bound post-limiter telemetry is claimed by this UI-only batch.
// A missing authoritative parameter VM must NOT permit a fake Ceiling edit.
Item {
    id: root
    objectName: "truePeakLimiterSurface"
    property var limiterModule: null
    property bool showLocalValidation: true
    property var limiterViewModel: null
    property var measurementProvider: null
    property var liveProvider: null
    readonly property bool bypassed: !limiterModule || limiterModule.bypass
    readonly property color familyAccent: bypassed ? "#6CBCCF" : "#C4774A"
    // Only authentic ViewModel validation is eligible. No invented DSP GR
    // or post-trim errors and no layout-derived error messages.
    readonly property string validationField: limiterViewModel
        ? (limiterViewModel.validationField || "") : ""
    readonly property string validationMessage: limiterViewModel
        ? (limiterViewModel.validationMessage || "") : ""
    readonly property bool hasValidationError: validationMessage.length > 0
    readonly property bool sonicControlsEnabled: !bypassed
        && limiterViewModel !== null && limiterViewModel !== undefined
    implicitWidth: 710
    implicitHeight: 460

    RowLayout {
        objectName: "m17PermanentColumns"
        anchors.fill: parent
        spacing: 8

        // Reuse M16's genuine two meter lanes and four permanent cards.
        TruePeakMeterEditor {
            id: sharedMeter
            objectName: "m17SharedMeter"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 505
            compactForLimiter: true
            useFamilyAccent: true
            familyAccent: root.familyAccent
            measurementProvider: root.measurementProvider
            liveProvider: root.liveProvider
        }

        Rectangle {
            id: limiterColumn
            objectName: "m17LimiterColumn"
            Layout.minimumWidth: 190
            Layout.preferredWidth: 190
            Layout.maximumWidth: 190
            Layout.fillHeight: true
            radius: 8
            color: "#0A1C2D"
            border.color: root.familyAccent
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Text {
                    objectName: "m17LimiterHeading"
                    Layout.fillWidth: true
                    text: "TRUE PEAK LIMITER"
                    color: root.familyAccent
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
                Text {
                    objectName: "m17LimiterState"
                    Layout.fillWidth: true
                    HoverHandler { id: limiterStateHover }
                    ToolTip.text: root.bypassed
                        ? "Limiter bypassed: identity processing. Retained Ceiling is not editable."
                        : root.sonicControlsEnabled
                            ? "Limiter enabled with an authoritative parameter binding."
                            : "No production sonic binding: controls remain disabled."
                    ToolTip.visible: limiterStateHover.hovered
                    ToolTip.delay: 550
                    text: root.bypassed ? "BYPASS · IDENTITY"
                        : root.sonicControlsEnabled ? "ACTIVE" : "NO SONIC BINDING"
                    color: "#C9DBE5"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }

                // Disabled until the real Ceiling ViewModel is connected.
                // The only parameter uses the canonical shared field/slider,
                // zero-origin fill from RIGHT, and no M17 wheel gesture.
                Rectangle {
                    id: ceilingCard
                    objectName: "m17CeilingCard"
                    Layout.fillWidth: true
                    HoverHandler { id: ceilingCardHover }
                    ToolTip.text: root.bypassed
                        ? "True Peak Ceiling is retained, but cannot be edited while bypassed."
                        : !root.sonicControlsEnabled
                            ? "Ceiling edit unavailable until the production limiter is connected."
                            : "Set the maximum output True Peak from -12.0 to 0.0 dBTP."
                    ToolTip.visible: ceilingCardHover.hovered
                    ToolTip.delay: 550
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
                    enabled: root.sonicControlsEnabled
                    opacity: root.sonicControlsEnabled ? 1.0 : 0.48
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
                    objectName: "m17GainReductionHistory"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 240
                    Layout.minimumHeight: 166
                    radius: 6
                    color: "#081521"
                    border.color: root.familyAccent
                    opacity: root.bypassed ? 0.5 : 1.0
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 5
                        Text {
                            text: "GAIN REDUCTION"
                            color: root.familyAccent
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        // Permanent graph, not a fabricated series. The plot
                        // will get a real applied-gain feed only after an
                        // independently verified M17 sink-consumed binding.
                        TruePeakGrHistoryGraph {
                            id: grHistoryGraph
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 72
                            familyAccent: root.familyAccent
                            bypassed: root.bypassed
                        }
                        Text {
                            objectName: "m17GrProvenanceStatus"
                            HoverHandler { id: grStatusHover }
                            ToolTip.text: "Applied GR history is not connected yet. The 0/3/6 dB labels are an axis, not measured attenuation. Future values require the exact audible terminal identity and consumed output samples."
                            ToolTip.visible: grStatusHover.hovered
                            ToolTip.delay: 550
                            text: root.bypassed ? "BYPASS · NO ACTIVE GR"
                                : "UNAVAILABLE · NO VERIFIED GR"
                            color: "#B3C9D9"
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            Layout.maximumHeight: 32
                        }
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
                    opacity: root.bypassed ? 0.5 : 1.0
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
                            HoverHandler { id: trimStatusHover }
                            ToolTip.text: "Verification Trim is a distinct measured observation, not an adjustable sonic control. Status remains unavailable until verified."
                            ToolTip.visible: trimStatusHover.hovered
                            ToolTip.delay: 550
                            text: "UNAVAILABLE · NOT VERIFIED"
                            color: "#B3C9D9"
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                Text {
                    objectName: "m17UnqualifiedNotice"
                    Layout.fillWidth: true
                    HoverHandler { id: qualificationNoticeHover }
                    ToolTip.text: "UI candidate only; terminal DSP integration and final qualification are not certified."
                    ToolTip.visible: qualificationNoticeHover.hovered
                    ToolTip.delay: 550
                    text: "M17 UI CANDIDATE · NOT QUALIFIED"
                    color: "#8EAAB9"
                    font.family: "Segoe UI"
                    font.pixelSize: 9
                    wrapMode: Text.WordWrap
                }
            }

            // Parametric EQ / Compressor grammar: a transient 16px status
            // OVERLAY, not a ColumnLayout child. The panel, controls and
            // shared Meter remain at byte-identical coordinates when errors
            // appear, disappear or are corrected.
            Rectangle {
                id: validationStatus
                objectName: "m17LimiterValidationStatusRegion"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 8
                height: 16
                z: 20
                radius: 3
                color: "#E012243F"
                visible: root.showLocalValidation && root.hasValidationError

                Text {
                    id: validationText
                    objectName: "m17LimiterValidationMessage"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 4
                    anchors.rightMargin: 4
                    text: root.validationMessage
                    color: "#F27683"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    HoverHandler { id: validationHover }
                    ToolTip.text: validationText.text
                    ToolTip.visible: validationHover.hovered && validationText.text.length > 0
                    ToolTip.delay: 450
                    Accessible.name: validationText.text
                    Accessible.description: validationText.text
                }
            }
        }
    }
}
