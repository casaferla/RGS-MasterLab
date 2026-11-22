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
    Text {
        x: 16; y: 12
        text: "REAL C00 DSP — SIMULATED PLAYBACK SNAPSHOT — RESEARCH ONLY"
        color: "#F2B632"
        font.family: "Segoe UI"
        font.pixelSize: 12
        font.bold: true
    }
    Studio.TransientShaperEditor {
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
