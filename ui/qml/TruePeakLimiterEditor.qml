import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Gate M: M16 and M17 share exactly the same physical TP meter components.
// The only mode-dependent content lives in the LOWER band.
// A missing authoritative parameter VM must NOT permit a fake Ceiling edit.
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

    // Combined two-column editor: one elastic meter/GR well and one
    // fixed-width TP readout/limiter controls rail. No third column.
    TruePeakMeterEditor {
        id: sharedMeter
        objectName: "m17SharedMeter"
        anchors.fill: parent
        compactForLimiter: true
        showLimiterGrHistory: true
        showLimiterControls: true
        limiterBypassed: root.bypassed
        limiterGrProvider: root.limiterGrProvider
        limiterViewModel: root.limiterViewModel
        limiterSonicControlsEnabled: root.sonicControlsEnabled
        useFamilyAccent: true
        familyAccent: root.familyAccent
        measurementProvider: root.measurementProvider
        liveProvider: root.liveProvider
    }

    // Product has a single centralized footer. Isolated UI fixtures may
    // retain this non-layout-changing validation overlay as before.
    // Parametric EQ / Compressor grammar: a transient 16px status
    // OVERLAY, not a ColumnLayout child. The panel, controls and
    // shared Meter remain at byte-identical coordinates when errors
    // appear, disappear or are corrected.
    Rectangle {
        id: validationStatus
        objectName: "m17LimiterValidationStatusRegion"
        anchors.right: parent.right
        width: 225
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
            ToolTip.visible: (validationHover.hovered && validationText.text.length > 0) && stationaryTooltipDwell1.ready
            ToolTip.delay: 0
            StudioTooltipDwell {
                id: stationaryTooltipDwell1
                anchors.fill: parent
            }
            Accessible.name: validationText.text
            Accessible.description: validationText.text
        }
    }

}
