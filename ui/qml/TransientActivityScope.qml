import QtQuick

// Post-L3 isolated editor candidate. Pure rendering of authoritative
// signed Attack/Sustain applied-gain telemetry. Never estimates DSP activity.
Item {
    id: root
    objectName: "transientActivityScope"
    property var history: []
    property bool telemetryAvailable: false
    property string telemetryRealizationId: ""
    property string audibleRealizationId: ""
    property real audibleFrame: -1
    property int sampleRateHz: 48000
    property bool auditioningProcessed: false
    property bool bypassed: false
    property string stateText: "TELEMETRY UNAVAILABLE"
    readonly property real secondsVisible: 5.0
    readonly property int maxBuckets: 4096
    readonly property color attackHue: "#C4774A"
    readonly property color sustainHue: "#45BFAE"

    // Display only buckets COMPLETED before the audible playhead boundary:
    // do not expose future gain data from the remainder of an active bucket.
    // Future renderer sidecar contract: ascending absolute half-open frame
    // buckets containing six real per-bucket statistics. Last values are
    // actual DSP states from the LAST sample of each bucket.
    // Identity must match the accepted audible PCM realization.
    readonly property bool identityCurrent: telemetryAvailable
        && auditioningProcessed && !bypassed
        && telemetryRealizationId.length > 0
        && telemetryRealizationId === audibleRealizationId
        && isFinite(audibleFrame) && audibleFrame >= 0
        && sampleRateHz > 0
    readonly property bool bucketsValid: {
        if (!identityCurrent || !history || history.length === 0
            || history.length > maxBuckets) return false
        let prevEnd = -1
        for (let i = 0; i < history.length; ++i) {
            const p = history[i]
            if (!p || !Number.isSafeInteger(p.frameBegin)
                || !Number.isSafeInteger(p.frameEndExclusive)
                || p.frameBegin < 0 || p.frameEndExclusive <= p.frameBegin
                || (prevEnd >= 0 && p.frameBegin !== prevEnd))
                return false
            const fields = ["attackMinDb", "attackMaxDb", "attackLastDb",
                            "sustainMinDb", "sustainMaxDb", "sustainLastDb"]
            for (let j = 0; j < fields.length; ++j) {
                const v = p[fields[j]]
                if (typeof v !== "number" || !isFinite(v) || Math.abs(v) > 12.000001)
                    return false
            }
            if (p.attackMinDb > p.attackLastDb || p.attackLastDb > p.attackMaxDb
                || p.sustainMinDb > p.sustainLastDb
                || p.sustainLastDb > p.sustainMaxDb) return false
            prevEnd = p.frameEndExclusive
        }
        // A whole-track or a bounded 5-second slice may be supplied, but
        // never treat an entirely future series as played telemetry.
        const windowStart = Math.max(0,
            audibleFrame - sampleRateHz * secondsVisible)
        return history.some(p =>
            p.frameEndExclusive > windowStart
            && p.frameEndExclusive <= audibleFrame)
    }
    readonly property bool graphAvailable: identityCurrent && bucketsValid
    function currentValue(field) {
        if (!graphAvailable) return NaN
        for (let i = history.length - 1; i >= 0; --i) {
            const p = history[i]
            const windowStart = Math.max(0,
                audibleFrame - sampleRateHz * secondsVisible)
            if (p.frameEndExclusive <= audibleFrame
                && p.frameEndExclusive > windowStart)
                return p[field]
        }
        return NaN
    }
    readonly property real currentAttackDb: currentValue("attackLastDb")
    readonly property real currentSustainDb: currentValue("sustainLastDb")
    function signedDb(v) {
        return isFinite(v) ? (v > 0 ? "+" : "") + v.toFixed(1) + " dB" : "—"
    }

    Text {
        id: caption
        x: 12; y: 5
        text: "TRANSIENT ACTIVITY  ·  LAST 5 SECONDS"
        color: "#A1B5C9"; font.family: "Segoe UI"
        font.bold: true; font.pixelSize: 10
    }
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 12
        y: 5
        text: root.graphAvailable ? root.stateText : "TELEMETRY UNAVAILABLE"
        color: root.graphAvailable ? "#45BFAE" : "#72899A"
        font.family: "Segoe UI"; font.pixelSize: 10
    }
    Text {
        x: 14; y: 32; text: "ATTACK"
        color: root.attackHue; font.pixelSize: 10; font.bold: true
    }
    Text {
        anchors.right: parent.right; anchors.rightMargin: 14
        y: 32
        text: root.signedDb(root.currentAttackDb)
        color: root.attackHue; font.family: "Consolas"; font.pixelSize: 12
    }
    Text {
        x: 14; y: Math.round(parent.height / 2) + 6
        text: "SUSTAIN"; color: root.sustainHue
        font.pixelSize: 10; font.bold: true
    }
    Text {
        anchors.right: parent.right; anchors.rightMargin: 14
        y: Math.round(parent.height / 2) + 6
        text: root.signedDb(root.currentSustainDb)
        color: root.sustainHue; font.family: "Consolas"; font.pixelSize: 12
    }

    Canvas {
        id: plot
        anchors.fill: parent
        anchors.topMargin: 26
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections {
            target: root
            function onHistoryChanged() { plot.requestPaint() }
            function onAudibleFrameChanged() { plot.requestPaint() }
            function onGraphAvailableChanged() { plot.requestPaint() }
            function onBypassedChanged() { plot.requestPaint() }
        }
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const left = 64, right = Math.max(left + 1, width - 12)
            const plotW = right - left
            const laneHeight = Math.max(12, height / 2 - 14)
            const laneCenters = [laneHeight * 0.56, height / 2 + laneHeight * 0.65]
            for (let lane = 0; lane < 2; ++lane) {
                const yc = laneCenters[lane]
                ctx.strokeStyle = "rgba(124,156,175,0.33)"
                ctx.lineWidth = 1; ctx.setLineDash([3, 4])
                ctx.beginPath()
                ctx.moveTo(left, yc); ctx.lineTo(right, yc); ctx.stroke()
                ctx.setLineDash([])
                ctx.fillStyle = "#7895A6"; ctx.font = "9px Consolas"
                ctx.fillText("+12", 32, yc - laneHeight * 0.32)
                ctx.fillText("0", 48, yc + 3)
                ctx.fillText("−12", 32, yc + laneHeight * 0.37)
            }
            if (!root.graphAvailable) return
            const windowFrames = Math.max(1, root.sampleRateHz * root.secondsVisible)
            const windowStart = root.audibleFrame - windowFrames
            const xAt = frame => left + ((frame - windowStart) / windowFrames) * plotW
            // Each bucket draws a true observed min/max ribbon. The drawn
            // center trace is the last actual DSP gain in that bucket.
            for (let lane = 0; lane < 2; ++lane) {
                const attack = lane === 0
                const minKey = attack ? "attackMinDb" : "sustainMinDb"
                const maxKey = attack ? "attackMaxDb" : "sustainMaxDb"
                const lastKey = attack ? "attackLastDb" : "sustainLastDb"
                const hue = attack ? root.attackHue : root.sustainHue
                const yc = laneCenters[lane]
                const mapY = value => yc - (value / 12.0) * (laneHeight * 0.38)
                ctx.strokeStyle = hue
                ctx.fillStyle = attack ? "rgba(196,119,74,0.36)"
                                       : "rgba(69,191,174,0.34)"
                ctx.lineWidth = 1.4
                ctx.beginPath()
                let started = false
                for (let i = 0; i < root.history.length; ++i) {
                    const p = root.history[i]
                    if (p.frameEndExclusive <= windowStart
                        || p.frameEndExclusive > root.audibleFrame) continue
                    const x0 = Math.max(left, xAt(p.frameBegin))
                    const x1 = Math.min(right, xAt(p.frameEndExclusive))
                    if (x1 < left || x0 > right) continue
                    const yTop = mapY(p[maxKey]), yBottom = mapY(p[minKey])
                    ctx.fillRect(x0, yTop, Math.max(1.0, x1 - x0), Math.max(1.0, yBottom-yTop))
                    const x = x0 + (x1 - x0) / 2
                    const y = mapY(p[lastKey])
                    if (!started) { ctx.moveTo(x, y); started = true }
                    else ctx.lineTo(x, y)
                }
                if (started) ctx.stroke()
            }
            // The existing audio playback clock owns this position.
            ctx.fillStyle = "#F2B632"
            ctx.fillRect(right - 1.5, 0, 2, height - 2)
        }
    }
}
