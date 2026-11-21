import QtQuick

// One M17 GR HISTORY: sampled actual applied gains across the WHOLE
// immutable MASTER. There is no GR Live chart and no Live/History toggle.
// The yellow cursor is a SOURCE/MASTER navigation position, NOT a Windows
// consumed-output or M16 True Peak certificate.
Canvas {
    id: root
    objectName: "m17GainReductionPlot"
    property var offlineHistory: []
    property bool backendOfflineAvailable: false
    property color familyAccent: "#C4774A"
    property bool bypassed: true
    // -1 means no authoritative position. The hosting Main waveform owns
    // the one clock; the GR chart must not run an independent timer.
    property real timelineFraction: -1
    property string timelineScope: ""

    readonly property bool offlineSeriesValid: {
        if (!root.backendOfflineAvailable || !root.offlineHistory
            || root.offlineHistory.length < 1
            || root.offlineHistory.length > 256)
            return false
        let nextBegin = 0
        for (let i = 0; i < root.offlineHistory.length; ++i) {
            const p = root.offlineHistory[i]
            if (!p || typeof p.peakReductionDb !== "number"
                || !isFinite(p.peakReductionDb) || p.peakReductionDb < 0
                || typeof p.frameBegin !== "number"
                || typeof p.frameEndExclusive !== "number"
                || !isFinite(p.frameBegin) || !isFinite(p.frameEndExclusive)
                || !Number.isSafeInteger(p.frameBegin)
                || !Number.isSafeInteger(p.frameEndExclusive)
                || p.frameBegin !== nextBegin
                || p.frameEndExclusive <= p.frameBegin)
                return false
            nextBegin = p.frameEndExclusive
        }
        return nextBegin > 0
    }
    readonly property bool offlineSeriesAvailable:
        !root.bypassed && root.offlineSeriesValid
    readonly property bool playheadAvailable: root.offlineSeriesAvailable
        && isFinite(root.timelineFraction)
        && root.timelineFraction >= 0 && root.timelineFraction <= 1
        && (root.timelineScope === "SOURCE" || root.timelineScope === "MASTER")

    implicitWidth: 160
    // Same fixed one-lane GR ribbon geometry as the M17/M14 layouts.
    implicitHeight: 54
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onOfflineHistoryChanged: requestPaint()
    onBackendOfflineAvailableChanged: requestPaint()
    onOfflineSeriesAvailableChanged: requestPaint()
    onFamilyAccentChanged: requestPaint()
    onBypassedChanged: requestPaint()
    onTimelineFractionChanged: requestPaint()
    onTimelineScopeChanged: requestPaint()

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
        const history = root.offlineSeriesAvailable ? root.offlineHistory : []
        const valid = history.length > 0
        const rgb = root.bypassed ? "108, 188, 207" : "196, 119, 74"
        let axisMax = 6
        if (valid) {
            for (let i = 0; i < history.length; ++i)
                axisMax = Math.max(axisMax, history[i].peakReductionDb)
        }
        axisMax = Math.ceil(axisMax / 3) * 3

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
            const fullFrames = history[history.length - 1].frameEndExclusive
            const pointX = p => left + plotWidth
                * (0.5 * (p.frameBegin / fullFrames)
                    + 0.5 * (p.frameEndExclusive / fullFrames))
            const pointY = p => top + plotHeight * p.peakReductionDb / axisMax

            // GR data is an immutable sampled whole-master envelope.
            // The copper fill is clipped BOTH to the curve's true shape
            // and to the waveform-synchronized cursor. It is not a rectangle.
            if (root.playheadAvailable && root.timelineFraction > 0) {
                const cursorX = left + plotWidth * root.timelineFraction
                ctx.save()
                ctx.beginPath()
                ctx.rect(left, top, Math.max(0, cursorX - left), plotHeight)
                ctx.clip()
                ctx.beginPath()
                ctx.moveTo(left, top)
                ctx.lineTo(left, pointY(history[0]))
                for (let i = 0; i < history.length; ++i)
                    ctx.lineTo(pointX(history[i]), pointY(history[i]))
                ctx.lineTo(right, pointY(history[history.length - 1]))
                ctx.lineTo(right, top)
                ctx.closePath()
                const fill = ctx.createLinearGradient(
                    left, 0, Math.max(left + 1, cursorX), 0)
                fill.addColorStop(0, "rgba(196, 119, 74, 0.035)")
                fill.addColorStop(0.7, "rgba(196, 119, 74, 0.23)")
                fill.addColorStop(1, "rgba(244, 166, 109, 0.68)")
                ctx.fillStyle = fill
                ctx.fill()
                ctx.restore()
            }
            // The entire future GR remains visible beyond the playhead.
            ctx.setLineDash([])
            ctx.strokeStyle = "#C4774A"
            ctx.lineWidth = 1.5
            ctx.beginPath()
            ctx.moveTo(left, pointY(history[0]))
            for (let i = 0; i < history.length; ++i)
                ctx.lineTo(pointX(history[i]), pointY(history[i]))
            ctx.lineTo(right, pointY(history[history.length - 1]))
            ctx.stroke()

            if (root.playheadAvailable) {
                const cursorX = left + plotWidth * root.timelineFraction
                // Exact waveform grammar: 2px yellow line and 8px triangle.
                ctx.setLineDash([])
                ctx.fillStyle = "#F2B632"
                ctx.fillRect(cursorX - 1, 0, 2, bottom)
                ctx.beginPath()
                ctx.moveTo(cursorX - 4, 0)
                ctx.lineTo(cursorX + 4, 0)
                ctx.lineTo(cursorX, 8)
                ctx.closePath()
                ctx.fill()
            }
        }
        ctx.setLineDash([])
        ctx.fillStyle = "#7895A6"
        ctx.font = "8px 'Segoe UI'"
        ctx.fillText("dB GR · sampled", Math.max(0, right - 68), height - 2)
        ctx.restore()
    }
}
