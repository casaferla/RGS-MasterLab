import QtQuick
import QtQuick.Controls
import "../../ui/qml" as Studio

// Real C00 PCM test observations; PlaybackSnapshot is simulated, not live audio.
ApplicationWindow {
    id: window
    objectName: "transientResearchWindow"
    width: 920
    height: 820
    visible: true
    color: "#081824"
    title: "RGS Transient Shaper — actual DSP / test playback snapshot"
    // Exact parser regression in real QML, not an independent JS mock.
    readonly property bool numericDraftParsingSafe:
        !isFinite(researchEditor.parseNumeric("", -12, 12))
        && !isFinite(researchEditor.parseNumeric("   ", -12, 12))
        && !isFinite(researchEditor.parseNumeric("13", -12, 12))
        && !isFinite(researchEditor.parseNumeric("1,2,3", -12, 12))
        && !isFinite(researchEditor.parseNumeric("0xA", -12, 12))
        && !isFinite(researchEditor.parseNumeric("1e1", -12, 12))
        && !isFinite(researchEditor.parseNumeric("1_0", -12, 12))
        && !isFinite(researchEditor.parseNumeric("1.2,3", -12, 12))
        && Math.abs(researchEditor.parseNumeric(",5", -12, 12) - 0.5) < 1e-9
        && Math.abs(researchEditor.parseNumeric("1,2", -12, 12) - 1.2) < 1e-9
        && Math.abs(researchEditor.parseNumeric("-12.0", -12, 12) + 12) < 1e-9
        && Math.abs(researchEditor.parseNumeric("12.0", -12, 12) - 12) < 1e-9
    Text {
        x: 16; y: 12
        text: "REAL C00 DSP — SIMULATED PLAYBACK SNAPSHOT — RESEARCH ONLY"
        color: "#F2B632"
        font.family: "Segoe UI"
        font.pixelSize: 12
        font.bold: true
    }
    Studio.TransientShaperEditor {
        id: researchEditor
        objectName: "transientShaperEditorResearch"
        x: 16; y: 44
        width: parent.width - 32
        height: parent.height - 60
        transientModule: researchControls
        activityHistory: researchTelemetry.activityHistory
        activityTelemetryAvailable: researchTelemetry.activityTelemetryAvailable
        telemetryRealizationId: researchTelemetry.telemetryRealizationId
        audibleRealizationId: researchTelemetry.audibleRealizationId
        audibleFrame: researchTelemetry.audibleFrame
        activitySampleRateHz: researchTelemetry.activitySampleRateHz
        auditioningProcessed: researchTelemetry.auditioningProcessed
        shaperBypassed: researchTelemetry.shaperBypassed
    }
}
