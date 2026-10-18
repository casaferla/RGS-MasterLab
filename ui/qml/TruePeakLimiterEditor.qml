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
    property var limiterViewModel: null
    property var measurementProvider: null
    property var liveProvider: null
    readonly property bool bypassed: !limiterModule || limiterModule.bypass
    readonly property color familyAccent: bypassed ? "#6CBCCF" : "#C4774A"
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
                StudioNumericSliderPair {
                    id: ceilingControl
                    objectName: "m17CeilingControl"
                    Layout.alignment: Qt.AlignLeft
                    enabled: root.sonicControlsEnabled
                    opacity: root.sonicControlsEnabled ? 1.0 : 0.48
                    viewModel: root.limiterViewModel
                    labelText: "CEILING"
                    unitText: "dBTP"
                    fieldName: "ceilingDbTp"
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

                Rectangle {
                    objectName: "m17GainReductionHistory"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 114
                    Layout.minimumHeight: 114
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
                        Text {
                            objectName: "m17GrProvenanceStatus"
                            text: root.bypassed ? "BYPASS · NO ACTIVE GR"
                                : "UNAVAILABLE · NO VERIFIED APPLIED-GAIN SERIES"
                            color: "#B3C9D9"
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                    }
                }
                Rectangle {
                    objectName: "m17VerificationTrim"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    Layout.minimumHeight: 72
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
                            text: "UNAVAILABLE · NOT VERIFIED"
                            color: "#B3C9D9"
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                Item { Layout.fillHeight: true }
                Text {
                    objectName: "m17UnqualifiedNotice"
                    Layout.fillWidth: true
                    text: "M17 UI CANDIDATE · NOT QUALIFIED"
                    color: "#8EAAB9"
                    font.family: "Segoe UI"
                    font.pixelSize: 9
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
