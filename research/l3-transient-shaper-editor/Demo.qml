import QtQuick
import QtQuick.Controls
import "../../ui/qml" as Studio

// DEMONSTRATION ONLY: generated curves are NOT DSP or actual PCM telemetry.
// The production editor must remain unavailable until a truthful render-sidecar
// and real audible realization owner are connected after L3 closure.
ApplicationWindow {
    id: demo
    width: 920
    height: 820
    visible: true
    title: "RGS Transient Shaper - ISOLATED SYNTHETIC DEMO"
    color: "#081824"
    property var buckets: []
    property int tick: 0
    readonly property int framesPerBucket: 2400

    QtObject {
        id: model
        property real attackAmountDb: 6.0
        property real sustainAmountDb: -3.0
        property bool manualEditable: true
        function setAttackAmountDb(v) { attackAmountDb = v }
        function setSustainAmountDb(v) { sustainAmountDb = v }
        function resetToDefault() {
            attackAmountDb = 0.0
            sustainAmountDb = 0.0
        }
    }
    Timer {
        interval: 50
        running: true
        repeat: true
        onTriggered: {
            demo.tick += 1
            const t = demo.tick / 20.0
            const beat = Math.pow(Math.max(0, Math.sin(t * Math.PI * 2)), 10)
            const attack = model.attackAmountDb * (0.12 + 0.58 * beat)
            const sustain = model.sustainAmountDb * (0.30 + 0.25 * (1 - beat))
            const b = demo.buckets.slice()
            b.push({
                frameBegin: (demo.tick - 1) * demo.framesPerBucket,
                frameEndExclusive: demo.tick * demo.framesPerBucket,
                attackMinDb: Math.min(0, attack),
                attackMaxDb: Math.max(0, attack),
                attackLastDb: attack,
                sustainMinDb: Math.min(0, sustain),
                sustainMaxDb: Math.max(0, sustain),
                sustainLastDb: sustain
            })
            if (b.length > 105) b.shift()
            demo.buckets = b
        }
    }

    Text {
        x: 20; y: 12
        color: "#F2B632"; font.pixelSize: 14; font.bold: true
        text: "SYNTHETIC UI DEMO — no audio processing or real telemetry"
    }
    Studio.TransientShaperEditor {
        x: 16; y: 44
        width: parent.width - 32
        height: parent.height - 60
        transientModule: model
        activityHistory: demo.buckets
        activityTelemetryAvailable: true
        telemetryRealizationId: "synthetic-demo-only"
        audibleRealizationId: "synthetic-demo-only"
        audibleFrame: demo.tick * demo.framesPerBucket
        activitySampleRateHz: 48000
        auditioningProcessed: true
        shaperBypassed: false
    }
}
