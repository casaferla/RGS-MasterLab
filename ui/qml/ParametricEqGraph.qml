import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property var viewModel: null

    color: "#081824"
    border.color: "#1A3E55"
    border.width: 1
    radius: 6

    gradient: Gradient {
        GradientStop { position: 0.0; color: "#0D2A3A" }
        GradientStop { position: 1.0; color: "#06141D" }
    }

    readonly property double minFreq: 20.0
    readonly property double maxFreq: {
        if (root.viewModel !== null && root.viewModel !== undefined && root.viewModel.selectedBandResponsePoints) {
            const points = root.viewModel.selectedBandResponsePoints
            if (points.length > 0) {
                const lastPt = points[points.length - 1]
                if (lastPt && lastPt.frequency > minFreq) {
                    return lastPt.frequency
                }
            }
        }
        return 20000.0
    }
    readonly property double minGain: -18.0
    readonly property double maxGain: 18.0

    // Board04 plotRect specifications
    readonly property real plotX: 52
    readonly property real plotY: 10
    readonly property real plotW: width - plotX - 12
    readonly property real plotH: height - plotY - 22

    function freqToX(freq) {
        if (freq <= minFreq) return plotX
        if (freq >= maxFreq) return plotX + plotW
        const logMin = Math.log10(minFreq)
        const logMax = Math.log10(maxFreq)
        return plotX + plotW * (Math.log10(freq) - logMin) / (logMax - logMin)
    }

    function xToFreq(x) {
        if (x <= plotX) return minFreq
        if (x >= plotX + plotW) return maxFreq
        const logMin = Math.log10(minFreq)
        const logMax = Math.log10(maxFreq)
        return Math.pow(10, logMin + ((x - plotX) / plotW) * (logMax - logMin))
    }

    function gainToY(gain) {
        if (gain <= minGain) return plotY + plotH
        if (gain >= maxGain) return plotY
        return plotY + plotH * (1.0 - (gain - minGain) / (maxGain - minGain))
    }

    function yToGain(y) {
        if (y <= plotY) return maxGain
        if (y >= plotY + plotH) return minGain
        return maxGain - ((y - plotY) / plotH) * (maxGain - minGain)
    }

    // Top inset highlight
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        height: 1
        color: "#1A1AA0C6"
    }

    // Unclipped Frequency Labels Gutter (below plotRect)
    Repeater {
        model: [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]
        delegate: Item {
            required property real modelData
            readonly property real lineX: root.freqToX(modelData)
            readonly property bool isMajor: modelData === 100 || modelData === 1000 || modelData === 10000
            readonly property bool isEndpoint: modelData === 20000

            anchors.fill: parent

            Text {
                x: parent.lineX - implicitWidth / 2
                y: root.plotY + root.plotH + 4
                text: {
                    if (parent.isEndpoint) {
                        const actualMax = root.maxFreq
                        if (actualMax >= 1000) {
                            const kVal = actualMax / 1000.0
                            return (kVal === Math.floor(kVal) ? kVal : kVal.toFixed(3)) + "k"
                        }
                        return actualMax
                    }
                    if (parent.modelData >= 1000) {
                        const kVal = parent.modelData / 1000.0
                        return (kVal === Math.floor(kVal) ? kVal : kVal.toFixed(3)) + "k"
                    }
                    return parent.modelData
                }
                color: "#A1B5C9"
                font.family: "Consolas"
                font.pixelSize: 10
                visible: parent.isMajor || parent.isEndpoint || root.plotW >= 700
            }
        }
    }

    // Unclipped Gain Labels Gutter (left of plotRect)
    Repeater {
        model: [-18, -12, -6, 0, 6, 12, 18]
        delegate: Item {
            required property real modelData
            readonly property real lineY: root.gainToY(modelData)
            readonly property bool isZero: modelData === 0
            anchors.fill: parent

            Text {
                x: 6
                y: parent.lineY - 6
                text: (parent.modelData > 0 ? "+" : "") + parent.modelData + " dB"
                color: parent.isZero ? "#F5F8FC" : "#A1B5C9"
                font.family: "Consolas"
                font.pixelSize: 10
            }
        }
    }

    // Internal Clipped Plot Area (Grid, Curve, Fill)
    Item {
        id: plotArea
        x: root.plotX
        y: root.plotY
        width: root.plotW
        height: root.plotH
        clip: true

        // Grid lines (vertical frequency landmarks)
        Repeater {
            model: [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]
            delegate: Rectangle {
                required property real modelData
                readonly property real lineX: root.freqToX(modelData) - root.plotX
                readonly property bool isMajor: modelData === 100 || modelData === 1000 || modelData === 10000

                x: lineX
                y: 0
                width: 1
                height: root.plotH
                color: isMajor ? "#66405466" : "#2E2A3A49"
            }
        }

        // Grid lines (horizontal gain landmarks)
        Repeater {
            model: [-18, -12, -6, 0, 6, 12, 18]
            delegate: Rectangle {
                required property real modelData
                readonly property real lineY: root.gainToY(modelData) - root.plotY
                readonly property bool isZero: modelData === 0

                x: 0
                y: lineY
                width: root.plotW
                height: 1
                color: isZero ? "#996B7C8F" : "#2E2A3A49"
            }
        }

        // Response Curve Canvas
        Canvas {
            id: curveCanvas
            anchors.fill: parent

            Connections {
                target: root.viewModel
                function onChanged() { curveCanvas.requestPaint() }
            }

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                if (!root.viewModel || !root.viewModel.selectedBandResponsePoints) return

                const points = root.viewModel.selectedBandResponsePoints
                if (points.length < 2) return

                const zeroY = root.gainToY(0) - root.plotY

                // Fill under curve split at zero
                for (let i = 0; i < points.length - 1; ++i) {
                    const pt1 = points[i]
                    const pt2 = points[i + 1]
                    const px1 = root.freqToX(pt1.frequency) - root.plotX
                    const py1 = root.gainToY(pt1.magnitudeDb) - root.plotY
                    const px2 = root.freqToX(pt2.frequency) - root.plotX
                    const py2 = root.gainToY(pt2.magnitudeDb) - root.plotY

                    ctx.beginPath()
                    ctx.moveTo(px1, py1)
                    ctx.lineTo(px2, py2)
                    ctx.lineTo(px2, zeroY)
                    ctx.lineTo(px1, zeroY)
                    ctx.closePath()
                    ctx.fillStyle = "#242ED3FF"
                    ctx.fill()
                }

                // Halo Under-stroke
                ctx.beginPath()
                ctx.lineWidth = 6
                ctx.strokeStyle = "#592ED3FF"
                for (let i = 0; i < points.length; ++i) {
                    const pt = points[i]
                    const px = root.freqToX(pt.frequency) - root.plotX
                    const py = root.gainToY(pt.magnitudeDb) - root.plotY
                    if (i === 0) ctx.moveTo(px, py)
                    else ctx.lineTo(px, py)
                }
                ctx.stroke()

                // Main Stroke
                ctx.beginPath()
                ctx.lineWidth = 2
                ctx.strokeStyle = "#FF2ED3FF"
                for (let i = 0; i < points.length; ++i) {
                    const pt = points[i]
                    const px = root.freqToX(pt.frequency) - root.plotX
                    const py = root.gainToY(pt.magnitudeDb) - root.plotY
                    if (i === 0) ctx.moveTo(px, py)
                    else ctx.lineTo(px, py)
                }
                ctx.stroke()
            }
        }
    }

    // Band Handles (Unclipped Siblings Above plotArea)
    readonly property var bandColors: [
        "#FF2ED3FF", // Band 1
        "#FF2FD98F", // Band 2
        "#FFC8D94A", // Band 3
        "#FFFF7A9A", // Band 4
        "#FFB56CFF", // Band 5
        "#FF6FC8FF"  // Band 6
    ]

    Repeater {
        model: root.viewModel ? root.viewModel.bandSummaries : []
        delegate: Item {
            required property var modelData
            required property int index

            readonly property bool isSelected: root.viewModel !== null && root.viewModel.selectedIndex === index
            readonly property bool isEnabled: modelData.enabled
            readonly property real handleX: root.freqToX(modelData.frequency)
            readonly property real handleY: root.gainToY(modelData.gainApplicable ? modelData.gain : 0)

            x: handleX - handleCircle.width / 2
            y: handleY - handleCircle.height / 2
            width: isSelected ? 24 : 20
            height: isSelected ? 24 : 20

            // Hover ring
            Rectangle {
                visible: handleMouse.containsMouse
                anchors.centerIn: parent
                width: 28
                height: 28
                radius: 14
                color: "transparent"
                border.color: "#1400C8FF"
                border.width: 1
            }

            Rectangle {
                id: handleCircle
                anchors.fill: parent
                radius: width / 2
                color: !isEnabled ? "#992A3A49" : "#FF0E2233"
                border.color: {
                    if (!isEnabled) return "#99566676"
                    if (isSelected) return "#FFFFFFFF"
                    return root.bandColors[index % root.bandColors.length]
                }
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: index + 1
                    color: !isEnabled ? "#99A6B2BF" : (isSelected ? "#FFFFFFFF" : "#E6FFFFFF")
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    font.weight: Font.Bold
                }
            }

            MouseArea {
                id: handleMouse
                anchors.fill: parent
                hoverEnabled: true
                drag.target: isSelected ? parent : null
                drag.axis: modelData.gainApplicable ? Drag.XAndYAxis : Drag.XAxis
                drag.minimumX: root.plotX - parent.width / 2
                drag.maximumX: root.plotX + root.plotW - parent.width / 2
                drag.minimumY: root.plotY - parent.height / 2
                drag.maximumY: root.plotY + root.plotH - parent.height / 2

                onPressed: {
                    if (!isSelected) {
                        root.viewModel.selectBand(index)
                    }
                }

                onPositionChanged: {
                    if (pressed && isSelected) {
                        const newCenterX = parent.x + parent.width / 2
                        const newCenterY = parent.y + parent.height / 2
                        const newFreq = root.xToFreq(newCenterX)
                        const newGain = root.yToGain(newCenterY)
                        root.viewModel.graphDrag(newFreq, newGain)
                    }
                }

                onReleased: {
                    if (isSelected) {
                        root.viewModel.graphRelease()
                    }
                }
            }
        }
    }
}
