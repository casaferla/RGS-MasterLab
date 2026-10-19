import QtQuick

// M17 GR graph CHROME only. Axis references are not measurements.
// Until the verified, sink-bound applied-gain sidecar is available this
// Canvas MUST NOT paint a line, fill, fake zero-history or peak marker.
// Future audio integration must independently authenticate actual OUTPUT_RATE
// consumption and RealizationId before binding historical GR data.
Canvas {
    id: root
    objectName: "m17GainReductionPlot"
    // Only a backend-bound authenticated GR provider may populate history.
    // The current production default stays NULL / NO GRAPH, not fake zero.
    property var verifiedHistory: []
    property bool backendVerified: false
    readonly property bool verifiedSeriesAvailable: backendVerified
        && !bypassed && verifiedHistory && verifiedHistory.length > 0
    property color familyAccent: "#C4774A"
    property bool bypassed: true
    implicitWidth: 160
    // Single stereo-linked GR history lane: identical native Canvas height
    // to CompressorEditor.qml's single-lane GR ribbon (54px).
    implicitHeight: 54

    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onFamilyAccentChanged: requestPaint()
    onVerifiedHistoryChanged: requestPaint()
    onBackendVerifiedChanged: requestPaint()
    onBypassedChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        if (width < 68 || height < 40)
            return
        const left = 6
        const top = 9
        const right = width - 34
        const bottom = height - 13
        const plotWidth = Math.max(1, right - left)
        const plotHeight = Math.max(1, bottom - top)

        // No signal curve is drawn unless the backend explicitly marks
        // an authentic gain witness bound to consumed OUTPUT_RATE frames.
        const history = root.verifiedSeriesAvailable ? root.verifiedHistory : []
        let valid = history.length > 0 && history.length <= 256
        let axisMax = 6
        if (valid) {
            for (let i = 0; i < history.length; ++i) {
                const sample = history[i]
                if (!sample || typeof sample.peakReductionDb !== "number"
                    || !isFinite(sample.peakReductionDb)
                    || sample.peakReductionDb < 0) {
                    valid = false
                    break
                }
                axisMax = Math.max(axisMax, sample.peakReductionDb)
            }
        }
        axisMax = Math.ceil(axisMax / 3) * 3
        const rgb = bypassed ? "108, 188, 207" : "196, 119, 74"
        ctx.save()
        ctx.lineWidth = 1
        for (let tick = 0; tick <= 2; ++tick) {
            const y = top + plotHeight * tick / 2
            ctx.strokeStyle = "rgba(" + rgb + ", 0.19)"
            ctx.setLineDash([2, 3])
            ctx.beginPath()
            ctx.moveTo(left, Math.round(y) + 0.5)
            ctx.lineTo(right, Math.round(y) + 0.5)
            ctx.stroke()
            ctx.fillStyle = "#7895A6"
            ctx.font = "9px 'Segoe UI'"
            ctx.fillText(String(axisMax * tick / 2), right + 5, y + 3)
        }
        for (let tick = 1; tick <= 3; ++tick) {
            const x = left + plotWidth * tick / 4
            ctx.strokeStyle = "rgba(" + rgb + ", 0.12)"
            ctx.setLineDash([2, 3])
            ctx.beginPath()
            ctx.moveTo(Math.round(x) + 0.5, top)
            ctx.lineTo(Math.round(x) + 0.5, bottom)
            ctx.stroke()
        }
        if (valid) {
            ctx.setLineDash([])
            ctx.strokeStyle = "#C4774A"
            ctx.lineWidth = 1.5
            ctx.beginPath()
            for (let i = 0; i < history.length; ++i) {
                const x = left + plotWidth * (i + 0.5) / history.length
                const y = top + plotHeight
                    * history[i].peakReductionDb / axisMax
                if (i === 0) ctx.moveTo(x, y)
                else ctx.lineTo(x, y)
            }
            ctx.stroke()
        }
        ctx.setLineDash([])
        ctx.fillStyle = "#7895A6"
        ctx.font = "8px 'Segoe UI'"
        ctx.fillText("dB GR", Math.max(0, right - 21), height - 2)
        ctx.restore()
    }
}
