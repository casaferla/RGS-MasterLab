import QtQuick

// M17 GR graph CHROME only. Axis references are not measurements.
// Until the verified, sink-bound applied-gain sidecar is available this
// Canvas MUST NOT paint a line, fill, fake zero-history or peak marker.
// Future audio integration must independently authenticate actual OUTPUT_RATE
// consumption and RealizationId before binding historical GR data.
Canvas {
    id: root
    objectName: "m17GainReductionPlot"
    readonly property bool verifiedSeriesAvailable: false
    property color familyAccent: "#C4774A"
    property bool bypassed: true
    implicitWidth: 160
    // Single stereo-linked GR history lane: identical native Canvas height
    // to CompressorEditor.qml's single-lane GR ribbon (54px).
    implicitHeight: 54

    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onFamilyAccentChanged: requestPaint()
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

        // A quiet, persistent scale in the Compressor's GR grammar:
        // 0 dB at top; attenuation grows downwards (3, then 6 dB).
        // No plot whatsoever is produced while history is unverified.
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
            ctx.fillText(String(tick * 3), right + 5, y + 3)
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
        ctx.setLineDash([])
        ctx.fillStyle = "#7895A6"
        ctx.font = "8px 'Segoe UI'"
        ctx.fillText("dB GR", Math.max(0, right - 21), height - 2)
        ctx.restore()
    }
}
