import QtQuick

// One approved AutoMatch amount knob: vertical drag, precise numeric backup
// in the native Product dialog, 270-degree LED arc and proportional glow.
// Pure presentation/input. The existing AutoMatchViewModel remains the only
// authority to validate and start a search; no DSP mutation occurs here.
Item {
    id: root
    objectName: "studioMatchKnob"
    property real value: 100.0
    property color accentColor: "#4E86C6"
    property bool editable: true
    property bool prominent: false
    property real stepSize: 0.1
    signal valueEdited(real nextValue)

    implicitWidth: prominent ? 38 : 30
    implicitHeight: implicitWidth
    activeFocusOnTab: editable
    readonly property real fraction: Math.max(0, Math.min(1, value / 100.0))

    function submit(raw) {
        if (!editable || !Number.isFinite(raw)) return
        const step = Math.max(0.01, stepSize)
        const clamped = Math.max(0, Math.min(100, raw))
        const next = Math.round(clamped / step) * step
        const rounded = Math.round(next * 100) / 100
        if (Math.abs(rounded - value) > 0.0001)
            valueEdited(rounded)
    }

    Keys.onPressed: function(event) {
        if (!root.editable) return
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Right) {
            root.submit(root.value + 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Left) {
            root.submit(root.value - 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Home) {
            root.submit(0)
            event.accepted = true
        } else if (event.key === Qt.Key_End) {
            root.submit(100)
            event.accepted = true
        }
    }

    Canvas {
        id: dial
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)
            const size = Math.min(width, height)
            const cx = width / 2
            const cy = height / 2
            const r = size * 0.392
            const start = Math.PI * 0.75
            const sweep = Math.PI * 1.5
            const amount = root.fraction
            ctx.lineCap = "round"

            // Continuous unlit base ring with lower 90-degree gap.
            ctx.beginPath()
            ctx.arc(cx, cy, r, start, start + sweep, false)
            ctx.lineWidth = Math.max(2.0, size * 0.085)
            ctx.strokeStyle = "#294153"
            ctx.stroke()

            // Arc sweep, core opacity and halo blur all scale linearly
            // from zero. No minimum-light or minimum-glow offset.
            // Read-only states retain a dim proportional arc without glow.
            if (amount > 0) {
                ctx.save()
                ctx.beginPath()
                ctx.arc(cx, cy, r, start, start + sweep * amount, false)
                ctx.strokeStyle = root.accentColor
                ctx.globalAlpha = root.editable ? amount : 0.23 * amount
                ctx.lineWidth = Math.max(2.0, size * 0.085)
                ctx.shadowColor = root.accentColor
                ctx.shadowBlur = root.editable ? 9.0 * amount : 0
                ctx.stroke()
                ctx.restore()
            }

            ctx.beginPath()
            ctx.arc(cx, cy, size * 0.285, 0, 2 * Math.PI)
            ctx.fillStyle = "#132638"
            ctx.fill()
            ctx.strokeStyle = "#355064"
            ctx.lineWidth = 1.0
            ctx.stroke()

            // Pointer tracks the same 270-degree arc, never a static
            // decorative mark. Preserve precise values from TextField.
            const angle = start + sweep * amount
            const ux = Math.cos(angle)
            const uy = Math.sin(angle)
            ctx.save()
            ctx.beginPath()
            ctx.moveTo(cx + ux * size * 0.115, cy + uy * size * 0.115)
            ctx.lineTo(cx + ux * size * 0.26, cy + uy * size * 0.26)
            ctx.strokeStyle = root.editable ? root.accentColor : "#647987"
            ctx.lineWidth = Math.max(1.6, size * 0.065)
            ctx.lineCap = "round"
            ctx.stroke()
            ctx.restore()
        }
    }

    onValueChanged: dial.requestPaint()
    onAccentColorChanged: dial.requestPaint()
    onEditableChanged: dial.requestPaint()
    onWidthChanged: dial.requestPaint()
    onHeightChanged: dial.requestPaint()

    MouseArea {
        anchors.fill: parent
        enabled: root.editable
        cursorShape: Qt.SizeVerCursor
        hoverEnabled: true
        property real pressY: 0
        property real pressValue: 0
        onPressed: function(mouse) {
            pressY = mouse.y
            pressValue = root.value
            root.forceActiveFocus()
        }
        onPositionChanged: function(mouse) {
            if (pressed)
                root.submit(pressValue + (pressY - mouse.y) * 0.8)
        }
        onWheel: function(wheel) {
            root.submit(root.value + (wheel.angleDelta.y >= 0 ? 1 : -1))
            wheel.accepted = true
        }
    }
}
