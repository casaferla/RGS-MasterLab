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

        if (kind === "zoom-in" || kind === "zoom-out") {
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
