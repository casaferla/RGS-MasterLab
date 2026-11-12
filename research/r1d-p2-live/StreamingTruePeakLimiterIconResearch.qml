import QtQuick

// RESEARCH-ONLY icon concept approved by PO 12 Nov 2026.
// Do NOT register this component or change StudioIcon.qml until Product I2 GO.
// In production StudioIcon should own the final drawing and its original
// dim/glow, tooltip, keyboard and accessibility contract.
Canvas {
    id: icon
    property color strokeColor: "#C4774A" // Dynamics Copper, per frozen UI family
    property bool glowEnabled: false
    implicitWidth: 18
    implicitHeight: 18
    antialiasing: true
    onStrokeColorChanged: requestPaint()
    onGlowEnabledChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)
        ctx.scale(width / 18.0, height / 18.0)
        ctx.strokeStyle = icon.strokeColor
        ctx.lineWidth = 1.65
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        ctx.shadowBlur = icon.glowEnabled ? 6.5 : 0
        ctx.shadowColor = icon.glowEnabled ? icon.strokeColor : "transparent"
        ctx.beginPath()
        // Horizontal TRUE PEAK ceiling: deliberately NOT M17's shield/check.
        ctx.moveTo(2, 4.8)
        ctx.lineTo(16, 4.8)
        ctx.stroke()
        ctx.beginPath()
        // Under-ceiling stylised controlled waveform, distinct from M16 bars.
        ctx.moveTo(2, 13)
        ctx.lineTo(4.1, 10.5)
        ctx.lineTo(6.2, 13)
        ctx.lineTo(8.7, 7.4)
        ctx.quadraticCurveTo(9.4, 5.9, 10.2, 7.4)
        ctx.lineTo(12.4, 12.7)
        ctx.lineTo(14, 8.1)
        ctx.quadraticCurveTo(14.8, 6.9, 16, 8.2)
        ctx.stroke()
    }
}
