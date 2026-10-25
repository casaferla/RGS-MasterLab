import QtQuick

Canvas {
    id: icon

    property string kind: ""
    property color strokeColor: "#DDE6F3"
    property color fillColor: strokeColor
    property real strokeWidth: 1.8

    implicitWidth: 18
    implicitHeight: 18
    antialiasing: true

    onKindChanged: requestPaint()
    onStrokeColorChanged: requestPaint()
    onFillColorChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)
        const sx = width / 18.0
        const sy = height / 18.0
        ctx.scale(sx, sy)
        ctx.strokeStyle = strokeColor
        ctx.fillStyle = fillColor
        ctx.lineWidth = strokeWidth
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        function line(x1, y1, x2, y2) {
            ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke()
        }
        function rect(x, y, w, h) {
            ctx.beginPath(); ctx.rect(x, y, w, h); ctx.stroke()
        }
        function dashRect(x, y, w, h, dash, gap) {
            for (let px = x; px < x + w; px += dash + gap) {
                line(px, y, Math.min(px + dash, x + w), y)
                line(px, y + h, Math.min(px + dash, x + w), y + h)
            }
            for (let py = y; py < y + h; py += dash + gap) {
                line(x, py, x, Math.min(py + dash, y + h))
                line(x + w, py, x + w, Math.min(py + dash, y + h))
            }
        }

        // PO-approved source silhouettes, 2026-10-25, 18x18 optical grid.
        // Keep the family hue, opacity and fixed 16px row slot in
        // DspChainSelector; these are semantic glyphs, never LED dots.
        if (kind === "module-gain") {
            // Three HORIZONTAL slider tracks; never the old single fader.
            line(2.5, 3.6, 10.2, 3.6)
            line(13.0, 3.6, 15.5, 3.6)
            line(11.6, 1.9, 11.6, 5.2)
            line(2.5, 9.0, 4.6, 9.0)
            line(7.4, 9.0, 15.5, 9.0)
            line(6.0, 7.3, 6.0, 10.7)
            line(2.5, 14.5, 10.8, 14.5)
            line(13.6, 14.5, 15.5, 14.5)
            line(12.2, 12.8, 12.2, 16.2)
        } else if (kind === "module-eq") {
            // Three VERTICAL mixing faders, staggered per source artwork.
            line(3.5, 2.0, 3.5, 9.2)
            line(3.5, 12.2, 3.5, 16.0)
            line(1.9, 10.7, 5.1, 10.7)
            line(9.0, 2.0, 9.0, 4.6)
            line(9.0, 7.6, 9.0, 16.0)
            line(7.4, 6.1, 10.6, 6.1)
            line(14.5, 2.0, 14.5, 11.0)
            line(14.5, 14.0, 14.5, 16.0)
            line(12.9, 12.5, 16.1, 12.5)
        } else if (kind === "module-compressor") {
            // Rounded audio bars; the old transfer curve is NOT approved.
            ctx.save()
            ctx.lineWidth = 2.5
            line(2.8, 7.1, 2.8, 11.1)
            line(5.9, 4.5, 5.9, 13.5)
            line(9.0, 2.1, 9.0, 15.9)
            line(12.1, 5.0, 12.1, 13.0)
            line(15.2, 7.2, 15.2, 10.8)
            ctx.restore()
        } else if (kind === "module-stereo") {
            // The approved branching M/S arrow, not overlapping rings.
            line(9.0, 16.1, 9.0, 9.3)
            line(9.0, 9.3, 3.4, 3.7)
            line(9.0, 9.3, 14.6, 3.7)
            line(3.4, 3.7, 3.4, 7.1)
            line(3.4, 3.7, 6.8, 3.7)
            line(14.6, 3.7, 11.2, 3.7)
            line(14.6, 3.7, 14.6, 7.1)
        } else if (kind === "module-peak-meter") {
            // A metering HISTOGRAM: three solid graduated columns.
            ctx.save()
            ctx.lineWidth = 3.2
            line(3.6, 10.4, 3.6, 15.1)
            line(9.0, 4.3, 9.0, 15.1)
            line(14.4, 8.0, 14.4, 15.1)
            ctx.restore()
        } else if (kind === "module-limiter") {
            // Verified-terminal shield/check mark, not a ceiling curve.
            ctx.beginPath()
            ctx.moveTo(9.0, 1.9)
            ctx.lineTo(15.1, 4.7)
            ctx.lineTo(15.1, 9.4)
            ctx.quadraticCurveTo(15.1, 13.3, 9.0, 16.3)
            ctx.quadraticCurveTo(2.9, 13.3, 2.9, 9.4)
            ctx.lineTo(2.9, 4.7)
            ctx.closePath()
            ctx.stroke()
            line(6.1, 9.3, 8.2, 11.4)
            line(8.2, 11.4, 12.2, 7.3)
        } else if (kind === "module-headphones") {
            // Rounded headset band and two independent ear cups.
            ctx.beginPath()
            ctx.arc(9.0, 9.0, 6.0, Math.PI, Math.PI * 2)
            ctx.stroke()
            line(3.0, 9.0, 3.0, 14.4)
            line(15.0, 9.0, 15.0, 14.4)
            ctx.beginPath()
            ctx.moveTo(3.1, 11.1)
            ctx.quadraticCurveTo(6.0, 10.4, 6.0, 13.4)
            ctx.quadraticCurveTo(6.0, 16.4, 3.1, 15.0)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(14.9, 11.1)
            ctx.quadraticCurveTo(12.0, 10.4, 12.0, 13.4)
            ctx.quadraticCurveTo(12.0, 16.4, 14.9, 15.0)
            ctx.closePath()
            ctx.stroke()
        } else if (kind === "zoom-in" || kind === "zoom-out") {
            ctx.beginPath(); ctx.arc(7.2, 7.2, 4.7, 0, Math.PI * 2); ctx.stroke()
            line(10.7, 10.7, 15.8, 15.8)
            line(4.7, 7.2, 9.7, 7.2)
            if (kind === "zoom-in") line(7.2, 4.7, 7.2, 9.7)
        } else if (kind === "fit-source") {
            line(2.5, 3.0, 2.5, 15.0); line(15.5, 3.0, 15.5, 15.0)
            line(5.0, 9.0, 13.0, 9.0)
            line(5.0, 9.0, 7.5, 6.5); line(5.0, 9.0, 7.5, 11.5)
            line(13.0, 9.0, 10.5, 6.5); line(13.0, 9.0, 10.5, 11.5)
        } else if (kind === "fit-region") {
            line(2.0, 3.0, 2.0, 15.0); line(16.0, 3.0, 16.0, 15.0)
            dashRect(5.0, 5.0, 8.0, 8.0, 2.0, 2.0)
            line(4.0, 9.0, 7.0, 9.0); line(4.0, 9.0, 6.0, 7.0); line(4.0, 9.0, 6.0, 11.0)
            line(14.0, 9.0, 11.0, 9.0); line(14.0, 9.0, 12.0, 7.0); line(14.0, 9.0, 12.0, 11.0)
        } else if (kind === "clear-region") {
            dashRect(3.0, 4.0, 12.0, 10.0, 3.0, 3.0)
        } else if (kind === "source") {
            ctx.beginPath(); ctx.moveTo(4.0, 2.0); ctx.lineTo(11.5, 2.0); ctx.lineTo(15.0, 5.5); ctx.lineTo(15.0, 16.0); ctx.lineTo(4.0, 16.0); ctx.closePath(); ctx.stroke()
            line(11.5, 2.0, 11.5, 5.5); line(11.5, 5.5, 15.0, 5.5)
            ctx.beginPath(); ctx.moveTo(6.0, 11.0); ctx.lineTo(7.5, 9.0); ctx.lineTo(9.0, 13.0); ctx.lineTo(10.5, 8.0); ctx.lineTo(12.5, 11.0); ctx.stroke()
        } else if (kind === "play") {
            ctx.beginPath(); ctx.moveTo(6.0, 3.5); ctx.lineTo(15.0, 9.0); ctx.lineTo(6.0, 14.5); ctx.closePath(); ctx.fill()
        } else if (kind === "pause") {
            ctx.fillRect(5.0, 3.5, 3.0, 11.0); ctx.fillRect(10.5, 3.5, 3.0, 11.0)
        } else if (kind === "stop") {
            ctx.fillRect(4.5, 4.5, 9.0, 9.0)
        } else if (kind === "minimize") {
            line(3.0, 11.0, 15.0, 11.0)
        } else if (kind === "maximize") {
            rect(4.0, 4.0, 10.0, 10.0)
        } else if (kind === "restore") {
            rect(5.5, 3.5, 9.0, 9.0); rect(3.5, 5.5, 9.0, 9.0)
        } else if (kind === "close") {
            line(4.0, 4.0, 14.0, 14.0); line(14.0, 4.0, 4.0, 14.0)
        } else if (kind === "folder-open") {
            ctx.beginPath(); ctx.moveTo(2.5, 6.0); ctx.lineTo(7.0, 6.0); ctx.lineTo(8.5, 4.0); ctx.lineTo(15.5, 4.0); ctx.lineTo(15.5, 14.5); ctx.lineTo(2.5, 14.5); ctx.closePath(); ctx.stroke()
            line(2.5, 8.0, 15.5, 8.0)
        }
    }
}
