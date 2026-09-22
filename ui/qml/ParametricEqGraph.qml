import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

Rectangle {
    id: root
    property var viewModel: null

    color: "#050C11"
    border.color: "#2A3947"
    border.width: 1
    radius: 4

    clip: true

    readonly property double minFreq: 20.0
    readonly property double maxFreq: 20000.0
    readonly property double minGain: -18.0
    readonly property double maxGain: 18.0

    function freqToX(freq) {
        if (freq <= minFreq) return 0
        if (freq >= maxFreq) return width
        const logMin = Math.log10(minFreq)
        const logMax = Math.log10(maxFreq)
        return width * (Math.log10(freq) - logMin) / (logMax - logMin)
    }

    function xToFreq(x) {
        if (x <= 0) return minFreq
        if (x >= width) return maxFreq
        const logMin = Math.log10(minFreq)
        const logMax = Math.log10(maxFreq)
        return Math.pow(10, logMin + (x / width) * (logMax - logMin))
    }

    function gainToY(gain) {
        if (gain <= minGain) return height
        if (gain >= maxGain) return 0
        return height * (1.0 - (gain - minGain) / (maxGain - minGain))
    }

    function yToGain(y) {
        if (y <= 0) return maxGain
        if (y >= height) return minGain
        return maxGain - (y / height) * (maxGain - minGain)
    }

    // Grid lines (vertical frequency landmarks)
    Repeater {
        model: [100, 1000, 10000]
        delegate: Item {
            required property real modelData
            readonly property real lineX: root.freqToX(modelData)
            anchors.fill: parent

            Rectangle {
                x: parent.lineX
                y: 0
                width: 1
                height: parent.height
                color: "#182836"
            }

            Text {
                x: parent.lineX + 4
                y: parent.height - 18
                text: parent.modelData >= 1000 ? (parent.modelData / 1000) + "k" : parent.modelData
                color: "#384956"
                font.family: "Segoe UI"
                font.pixelSize: 9
            }
        }
    }

    // Grid lines (horizontal gain landmarks)
    Repeater {
        model: [-12, -6, 0, 6, 12]
        delegate: Item {
            required property real modelData
            readonly property real lineY: root.gainToY(modelData)
            anchors.fill: parent

            Rectangle {
                x: 0
                y: parent.lineY
                width: parent.width
                height: 1
                color: parent.modelData === 0 ? "#2D4354" : "#13212C"
            }

            Text {
                x: 6
                y: parent.lineY - 12
                text: (parent.modelData > 0 ? "+" : "") + parent.modelData + " dB"
                color: parent.modelData === 0 ? "#4D6374" : "#283846"
                font.family: "Segoe UI"
                font.pixelSize: 9
            }
        }
    }

    // Mixed Routing Indicator
    Rectangle {
        visible: root.viewModel !== null && root.viewModel !== undefined && root.viewModel.mixedRouting
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        width: mixedText.width + 16
        height: 22
        color: "#2B1A3A"
        border.color: "#8B42C0"
        border.width: 1
        radius: 3

        Text {
            id: mixedText
            anchors.centerIn: parent
            text: "MIXED ROUTING ACTIVE"
            color: "#D088FF"
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.weight: Font.DemiBold
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

            ctx.beginPath()
            ctx.lineWidth = 2
            ctx.strokeStyle = "#8B42C0"

            for (let i = 0; i < points.length; ++i) {
                const pt = points[i]
                const px = root.freqToX(pt.frequency)
                const py = root.gainToY(pt.magnitudeDb)
                if (i === 0) {
                    ctx.moveTo(px, py)
                } else {
                    ctx.lineTo(px, py)
                }
            }
            ctx.stroke()

            // Fill area under curve
            const lastPt = points[points.length - 1]
            const zeroY = root.gainToY(0)
            ctx.lineTo(root.freqToX(lastPt.frequency), zeroY)
            ctx.lineTo(root.freqToX(points[0].frequency), zeroY)
            ctx.closePath()
            ctx.fillStyle = "#1A8B42C0"
            ctx.fill()
        }
    }

    // Band Handles
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
            width: isSelected ? 24 : 18
            height: isSelected ? 24 : 18

            Rectangle {
                id: handleCircle
                anchors.fill: parent
                radius: width / 2
                color: {
                    if (!isEnabled) return "#1A2630"
                    if (isSelected) return "#8B42C0"
                    return "#223545"
                }
                border.color: {
                    if (!isEnabled) return "#3A4B58"
                    if (isSelected) return "#E6EEF0"
                    return "#00C8FF"
                }
                border.width: isSelected ? 2 : 1

                Text {
                    anchors.centerIn: parent
                    text: index + 1
                    color: !isEnabled ? "#485866" : "#E6EEF0"
                    font.family: "Segoe UI"
                    font.pixelSize: isSelected ? 11 : 9
                    font.weight: Font.Bold
                }
            }

            MouseArea {
                id: handleMouse
                anchors.fill: parent
                drag.target: isSelected ? parent : null
                drag.axis: modelData.gainApplicable ? Drag.XAndYAxis : Drag.XAxis
                drag.minimumX: -parent.width / 2
                drag.maximumX: root.width - parent.width / 2
                drag.minimumY: -parent.height / 2
                drag.maximumY: root.height - parent.height / 2

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
