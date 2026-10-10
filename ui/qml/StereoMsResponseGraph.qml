import QtQuick
import QtQuick.Controls

// M15 active surface: ALL audio/Width/crossover arithmetic belongs to DSP C++.
// This QML component only projects authoritative frequency-response points
// into pixels and stages direct pointer edits until release.
Rectangle {
    id: root
    objectName: "stereoMsResponseGraph"
    property var viewModel: null

    readonly property color seaGreen: "#4A9A88"
    readonly property color widthHue: "#2ED3FF"
    readonly property color cutoffHue: "#2FD98F"
    readonly property color lowHue: "#FFD84A"
    readonly property bool ready: viewModel && viewModel.widthResponseStatus === "READY"
    readonly property var responsePoints: ready ? viewModel.widthResponsePoints : []
    readonly property real minFrequency: 20
    readonly property real maxFrequency: responsePoints.length > 1
        ? responsePoints[responsePoints.length - 1].frequencyHz : 20000
    readonly property real axisMaximum: {
        let peak = 200
        for (let i = 0; i < responsePoints.length; ++i) {
            peak = Math.max(peak, responsePoints[i].widthPercent * 1.12)
        }
        return Math.ceil(peak / 50) * 50
    }

    readonly property real plotX: 47
    readonly property real plotY: 29
    readonly property real plotW: Math.max(1, width - plotX - 16)
    readonly property real plotH: Math.max(1, height - plotY - 32)

    color: "#081824"
    border.color: "#1A3E55"
    border.width: 1
    radius: 6
    clip: true

    gradient: Gradient {
        GradientStop { position: 0.0; color: "#0D2A3A" }
        GradientStop { position: 1.0; color: "#06141D" }
    }

    function frequencyX(hz) {
        const frequency = Math.max(minFrequency, Math.min(maxFrequency, hz))
        return plotX + plotW * Math.log(frequency / minFrequency)
            / Math.log(maxFrequency / minFrequency)
    }

    function pointerFrequency(px) {
        const position = Math.max(0, Math.min(1, (px - plotX) / plotW))
        return minFrequency * Math.pow(maxFrequency / minFrequency, position)
    }

    function responseY(widthPercent) {
        return plotY + plotH * (1 - Math.max(0, Math.min(axisMaximum, widthPercent)) / axisMaximum)
    }

    // Read nearest backend-evaluated point only. No interpolated crossover
    // law is reconstructed in QML.
    function valueNearFrequency(hz) {
        if (!ready || responsePoints.length === 0) return 0
        let result = responsePoints[0]
        let closest = Math.abs(Math.log(result.frequencyHz / hz))
        for (let i = 1; i < responsePoints.length; ++i) {
            const current = responsePoints[i]
            const distance = Math.abs(Math.log(current.frequencyHz / hz))
            if (distance < closest) {
                result = current
                closest = distance
            }
        }
        return result.widthPercent
    }

    Text {
        x: 14
        y: 6
        text: "STEREO WIDTH RESPONSE"
        color: "#A1B5C9"
        font.family: "Segoe UI"
        font.pixelSize: 11
        font.weight: Font.Bold
    }

    Repeater {
        model: [20, 100, 1000, 10000]
        delegate: Item {
            required property real modelData
            x: root.frequencyX(modelData)
            y: root.plotY
            height: root.plotH
            width: 1
            visible: modelData <= root.maxFrequency
            Rectangle {
                anchors.fill: parent
                color: "#33425A65"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height + 6
                text: modelData >= 1000 ? (modelData / 1000) + "k" : modelData
                color: "#A1B5C9"
                font.family: "Consolas"
                font.pixelSize: 10
            }
        }
    }

    Repeater {
        model: 5
        delegate: Item {
            required property int index
            readonly property real stepWidth: index * root.axisMaximum / 4
            x: root.plotX
            y: root.responseY(stepWidth)
            width: root.plotW
            height: 1
            Rectangle {
                anchors.fill: parent
                color: index === 0 ? "#665E7E82" : "#2E2A3A49"
            }
            Text {
                x: -42
                y: -7
                text: Math.round(parent.stepWidth) + "%"
                color: "#A1B5C9"
                font.family: "Consolas"
                font.pixelSize: 10
            }
        }
    }

    Canvas {
        id: responseCanvas
        objectName: "stereoMsResponseCurve"
        anchors.fill: parent
        visible: root.ready
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const pts = root.responsePoints
            if (!root.ready || pts.length < 2) return
            ctx.beginPath()
            for (let i = 0; i < pts.length; ++i) {
                const x = root.frequencyX(pts[i].frequencyHz)
                const y = root.responseY(pts[i].widthPercent)
                if (i === 0) ctx.moveTo(x, y)
                else ctx.lineTo(x, y)
            }
            ctx.strokeStyle = "#64D2B4"
            ctx.lineWidth = 2
            ctx.stroke()
        }
        Connections {
            target: root.viewModel
            function onChanged() { responseCanvas.requestPaint() }
        }
        Connections {
            target: root
            function onWidthChanged() { responseCanvas.requestPaint() }
            function onHeightChanged() { responseCanvas.requestPaint() }
            function onReadyChanged() { responseCanvas.requestPaint() }
        }
    }

    // Semantic handles: filled intervention hue, white outline; their
    // matching Studio sliders use white knobs with the SAME hue border.
    Rectangle {
        id: widthHandle
        objectName: "stereoMsWidthHandle"
        visible: root.ready
        width: 17
        height: 17
        radius: width / 2
        color: root.widthHue
        border.color: "#F5F8FC"
        border.width: 2
        x: root.frequencyX(root.maxFrequency * 0.6) - width / 2
        y: root.responseY(root.valueNearFrequency(root.maxFrequency * 0.6)) - height / 2
        DragHandler {
            id: widthDrag
            target: null
            property real startValue: 100
            onActiveChanged: {
                if (active && root.viewModel) startValue = root.viewModel.draftWidthPercent
                else if (!active && root.viewModel) root.viewModel.commitDraft()
            }
            onTranslationChanged: {
                if (active && root.viewModel) {
                    const next = Math.max(0, startValue - translation.y
                        / root.plotH * root.axisMaximum)
                    root.viewModel.setDraftWidthPercent(next)
                }
            }
        }
        ToolTip.text: "WIDTH — vertical drag"
        ToolTip.visible: widthDrag.active
    }

    Rectangle {
        id: cutoffHandle
        objectName: "stereoMsCutoffHandle"
        visible: root.ready && root.viewModel.monoBassControlsEffective
        width: 17
        height: 17
        radius: width / 2
        color: root.cutoffHue
        border.color: "#F5F8FC"
        border.width: 2
        x: root.frequencyX(root.viewModel ? root.viewModel.monoBassCutoffHz : 120) - width / 2
        y: root.responseY(root.valueNearFrequency(
            root.viewModel ? root.viewModel.monoBassCutoffHz : 120)) - height / 2
        DragHandler {
            id: cutoffDrag
            target: null
            property real startFrequency: 120
            onActiveChanged: {
                if (active && root.viewModel) startFrequency = root.viewModel.monoBassCutoffHz
                else if (!active && root.viewModel) root.viewModel.commitDraft()
            }
            onTranslationChanged: {
                if (active && root.viewModel) {
                    const desiredX = root.frequencyX(startFrequency) + translation.x
                    const next = Math.max(40, Math.min(300, root.pointerFrequency(desiredX)))
                    root.viewModel.setDraftMonoBassCutoffHz(next)
                }
            }
        }
        ToolTip.text: "MONO BASS CUTOFF — horizontal drag"
        ToolTip.visible: cutoffDrag.active
    }

    Rectangle {
        id: lowHandle
        objectName: "stereoMsLowWidthHandle"
        visible: root.ready && root.viewModel.monoBassControlsEffective
        width: 17
        height: 17
        radius: width / 2
        color: root.lowHue
        border.color: "#F5F8FC"
        border.width: 2
        x: root.frequencyX(32) - width / 2
        y: root.responseY(root.valueNearFrequency(32)) - height / 2
        DragHandler {
            id: lowDrag
            target: null
            property real startValue: 100
            onActiveChanged: {
                if (active && root.viewModel) startValue = root.viewModel.lowBandWidthPercent
                else if (!active && root.viewModel) root.viewModel.commitDraft()
            }
            onTranslationChanged: {
                if (active && root.viewModel) {
                    const next = Math.max(0, Math.min(100,
                        startValue - translation.y / root.plotH * 100))
                    root.viewModel.setDraftLowBandWidthPercent(next)
                }
            }
        }
        ToolTip.text: "LOW WIDTH — vertical drag"
        ToolTip.visible: lowDrag.active
    }

    Rectangle {
        anchors.centerIn: parent
        width: stateLabel.implicitWidth + 30
        height: 42
        radius: 6
        color: "#12243F"
        border.color: "#32526A"
        visible: !root.ready
        Text {
            id: stateLabel
            anchors.centerIn: parent
            text: {
                if (!root.viewModel) return "NO DSP VIEW MODEL"
                switch (root.viewModel.widthResponseStatus) {
                case "MODULE_UNAVAILABLE": return "MODULE UNAVAILABLE"
                case "MONO_INPUT": return "MONO · SPATIAL NON-EFFECTIVE"
                case "BYPASSED": return "BYPASSED · NO ACTIVE RESPONSE"
                case "SOURCE_UNAVAILABLE": return "SOURCE FORMAT UNAVAILABLE"
                default: return "RESPONSE UNAVAILABLE"
                }
            }
            color: "#A1B5C9"
            font.family: "Segoe UI"
            font.pixelSize: 12
        }
    }
}
