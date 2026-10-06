import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "compressorEditor"

    property var viewModel: null

    readonly property color textPrimary: "#F5F8FC"
    readonly property color textSecondary: "#A1B5C9"
    readonly property color textMuted: "#586773"
    readonly property color accent: "#00C8FF"
    readonly property color copperAccent: "#C4774A"
    readonly property color error: "#F27683"
    readonly property color borderDark: "#1E354A"
    readonly property color panelBg: "#0B1824"
    readonly property color wellBg: "#060D14"
    // Authored from the 1184x688 minimum composition upward. Keep the
    // 3-column topology stable; reclaim width from padding, gaps and controls.
    readonly property int compactNumericFieldWidth: 92

    readonly property string validationField: root.viewModel ? root.viewModel.validationField : ""
    readonly property string validationErrorText: root.viewModel ? root.viewModel.validationMessage : ""
    readonly property bool hasValidationError: validationErrorText.length > 0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Header Action Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "DYNAMICS COMPRESSOR"
                color: root.textSecondary
                font.family: "Segoe UI"
                font.pixelSize: 11
                font.weight: Font.Bold
            }

            Item { Layout.fillWidth: true }

            // Reset to Default Button
            StudioButton {
                id: resetButton
                objectName: "resetCompressorButton"
                text: "Reset to Default"
                minimumControlWidth: 140
                enabled: root.viewModel !== null
                onClicked: if (root.viewModel) root.viewModel.resetToDefault()
                Accessible.name: "Reset Compressor to default parameters"
                ToolTip.text: "Reset compressor to canonical M13 defaults"
                ToolTip.visible: hovered
            }
        }

        // Main Editor Body Split
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // Left Side: Static Transfer Curve Display Well
            Rectangle {
                id: curveWell
                objectName: "compressorCurveWell"
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                radius: 6
                color: root.wellBg
                border.color: root.borderDark
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "STATIC TRANSFER CURVE"
                            color: root.copperAccent
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "In: -120..+6 dBFS"
                            color: root.textMuted
                            font.family: "Segoe UI"
                            font.pixelSize: 9
                        }
                    }

                    // Canvas Graph with Interactive Handles
                    Canvas {
                        id: curveCanvas
                        objectName: "compressorCurveCanvas"
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        property var pointsList: root.viewModel ? root.viewModel.transferCurvePoints : []
                        property var handlesList: root.viewModel ? root.viewModel.transferCurveHandles : []

                        property real viewMinDbfs: -60.0
                        property real viewMaxDbfs: 6.0

                        onPointsListChanged: requestPaint()
                        onHandlesListChanged: requestPaint()
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)

                            var minDbfs = -60.0
                            var maxDbfs = 6.0

                            // Dynamic Viewport Auto-fit for extreme parameter states
                            if (handlesList && handlesList.length > 0) {
                                for (var h = 0; h < handlesList.length; ++h) {
                                    if (handlesList[h].inputDbfs < minDbfs) minDbfs = handlesList[h].inputDbfs
                                    if (handlesList[h].outputDbfs < minDbfs) minDbfs = handlesList[h].outputDbfs
                                    if (handlesList[h].outputDbfs > maxDbfs) maxDbfs = handlesList[h].outputDbfs
                                }
                            }
                            minDbfs = Math.floor(minDbfs / 12.0) * 12.0
                            maxDbfs = Math.ceil(maxDbfs / 6.0) * 6.0

                            viewMinDbfs = minDbfs
                            viewMaxDbfs = maxDbfs

                            const dbRange = maxDbfs - minDbfs

                            function mapX(db) {
                                return (db - minDbfs) / dbRange * width
                            }
                            function mapY(db) {
                                return height - ((db - minDbfs) / dbRange * height)
                            }

                            // Grid Lines
                            ctx.lineWidth = 1
                            ctx.strokeStyle = "#122536"
                            const gridSteps = [-108, -96, -84, -72, -60, -48, -36, -24, -12, 0]
                            for (var i = 0; i < gridSteps.length; ++i) {
                                if (gridSteps[i] >= minDbfs && gridSteps[i] <= maxDbfs) {
                                    var gx = mapX(gridSteps[i])
                                    var gy = mapY(gridSteps[i])

                                    ctx.beginPath()
                                    ctx.moveTo(gx, 0)
                                    ctx.lineTo(gx, height)
                                    ctx.stroke()

                                    ctx.beginPath()
                                    ctx.moveTo(0, gy)
                                    ctx.lineTo(width, gy)
                                    ctx.stroke()
                                }
                            }

                            // 1:1 45-degree Reference Line
                            ctx.strokeStyle = "#1E3A52"
                            ctx.setLineDash([3, 3])
                            ctx.beginPath()
                            ctx.moveTo(mapX(minDbfs), mapY(minDbfs))
                            ctx.lineTo(mapX(maxDbfs), mapY(maxDbfs))
                            ctx.stroke()
                            ctx.setLineDash([])

                            // Dynamics Copper Transfer Curve
                            if (pointsList && pointsList.length > 0) {
                                ctx.strokeStyle = "#C4774A"
                                ctx.lineWidth = 2
                                ctx.beginPath()

                                var started = false
                                for (var p = 0; p < pointsList.length; ++p) {
                                    var pt = pointsList[p]
                                    if (pt.inputDbfs >= minDbfs) {
                                        var px = mapX(pt.inputDbfs)
                                        var py = mapY(pt.outputDbfs)

                                        if (!started) {
                                            ctx.moveTo(px, py)
                                            started = true
                                        } else {
                                            ctx.lineTo(px, py)
                                        }
                                    }
                                }
                                ctx.stroke()
                            }

                            // C++ Transfer Curve Handles
                            if (handlesList && handlesList.length > 0) {
                                for (var handleIdx = 0; handleIdx < handlesList.length; ++handleIdx) {
                                    var handle = handlesList[handleIdx]
                                    var hx = mapX(handle.inputDbfs)
                                    var hy = mapY(handle.outputDbfs)

                                    ctx.fillStyle = handle.color
                                    ctx.strokeStyle = "#FFFFFF"
                                    ctx.lineWidth = 1.5

                                    ctx.beginPath()
                                    ctx.arc(hx, hy, 5, 0, 2 * Math.PI)
                                    ctx.fill()
                                    ctx.stroke()
                                }
                            }
                        }

                        MouseArea {
                            id: curveMouseArea
                            anchors.fill: parent
                            hoverEnabled: true

                            property string activeHandleId: ""

                            function mapXToDbfs(px) {
                                const minDbfs = curveCanvas.viewMinDbfs
                                const maxDbfs = curveCanvas.viewMaxDbfs
                                return minDbfs + (px / curveCanvas.width) * (maxDbfs - minDbfs)
                            }

                            function mapYToDbfs(py) {
                                const minDbfs = curveCanvas.viewMinDbfs
                                const maxDbfs = curveCanvas.viewMaxDbfs
                                return minDbfs + ((curveCanvas.height - py) / curveCanvas.height) * (maxDbfs - minDbfs)
                            }

                            onPressed: (mouse) => {
                                activeHandleId = ""
                                if (!root.viewModel) return

                                var handles = root.viewModel.transferCurveHandles
                                if (!handles || handles.length === 0) return

                                const minDbfs = curveCanvas.viewMinDbfs
                                const maxDbfs = curveCanvas.viewMaxDbfs
                                const dbRange = maxDbfs - minDbfs

                                var bestId = ""
                                var bestDist = 20.0

                                for (var i = 0; i < handles.length; ++i) {
                                    var h = handles[i]
                                    var hx = (h.inputDbfs - minDbfs) / dbRange * curveCanvas.width
                                    var hy = curveCanvas.height - ((h.outputDbfs - minDbfs) / dbRange * curveCanvas.height)
                                    var dist = Math.hypot(mouse.x - hx, mouse.y - hy)
                                    if (dist < bestDist) {
                                        bestDist = dist
                                        bestId = h.id
                                    }
                                }

                                activeHandleId = bestId
                            }

                            onPositionChanged: (mouse) => {
                                if (pressed && activeHandleId !== "" && root.viewModel) {
                                    var inDbfs = mapXToDbfs(mouse.x)
                                    var outDbfs = mapYToDbfs(mouse.y)

                                    // Apply legal drag clamps
                                    if (activeHandleId === "threshold") {
                                        inDbfs = Math.max(-120.0, Math.min(0.0, inDbfs))
                                    } else if (activeHandleId === "knee") {
                                        // Knee is derived from distance to threshold in C++
                                    } else if (activeHandleId === "makeup") {
                                        outDbfs = Math.max(-24.0, Math.min(24.0, outDbfs))
                                    }

                                    root.viewModel.setCurveHandleDraft(activeHandleId, inDbfs, outDbfs)
                                }
                            }

                            onReleased: {
                                if (activeHandleId !== "" && root.viewModel) {
                                    root.viewModel.commitDraft()
                                    activeHandleId = ""
                                }
                            }
                        }
                    }

                    // Divider
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#1A2C3D"
                    }

                    // Compact Live Gain Reduction Area
                    ColumnLayout {
                        id: liveGrWell
                        objectName: "compressorLiveGrWell"
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                objectName: "compressorLiveGrStateText"
                                text: root.viewModel ? root.viewModel.liveGrState : "NOT AUDITIONED"
                                color: root.viewModel && root.viewModel.liveGrState.startsWith("ACTIVE") ? root.copperAccent : root.textMuted
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                                font.weight: Font.Bold
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                objectName: "compressorLiveGrValueText"
                                text: root.viewModel ? root.viewModel.liveGrDbText : "-- dB"
                                color: root.copperAccent
                                font.family: "Consolas"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                visible: root.viewModel ? (root.viewModel.liveGrState.startsWith("ACTIVE")) : false
                            }

                            Text {
                                objectName: "compressorLiveGrValueTextR"
                                text: root.viewModel && root.viewModel.isDualMonoTelemetry ? (" / R: " + root.viewModel.liveGrDbTextR) : ""
                                color: root.copperAccent
                                font.family: "Consolas"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                visible: root.viewModel ? (root.viewModel.isDualMonoTelemetry && root.viewModel.liveGrState.startsWith("ACTIVE")) : false
                            }
                        }

                        // Live GR History Trace Canvas
                        Canvas {
                            id: liveGrCanvas
                            objectName: "compressorLiveGrCanvas"
                            Layout.fillWidth: true
                            implicitHeight: 50

                            property var historyL: root.viewModel ? root.viewModel.liveGrHistoryL : []
                            property var historyR: root.viewModel ? root.viewModel.liveGrHistoryR : []

                            onHistoryLChanged: requestPaint()
                            onHistoryRChanged: requestPaint()
                            onWidthChanged: requestPaint()
                            onHeightChanged: requestPaint()

                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)

                                if (!historyL || historyL.length === 0) return

                                var maxDb = 24.0
                                for (var i = 0; i < historyL.length; ++i) {
                                    if (historyL[i].peakDb > maxDb) maxDb = historyL[i].peakDb
                                }
                                if (historyR && historyR.length > 0) {
                                    for (var j = 0; j < historyR.length; ++j) {
                                        if (historyR[j].peakDb > maxDb) maxDb = historyR[j].peakDb
                                    }
                                }
                                maxDb = Math.ceil(maxDb / 6.0) * 6.0

                                function mapY(db) {
                                    return (db / maxDb) * height
                                }

                                const numPoints = historyL.length
                                const dx = width / Math.max(1, numPoints - 1)

                                // Draw L mean line trace
                                ctx.strokeStyle = "#C4774A"
                                ctx.lineWidth = 1.5
                                ctx.beginPath()
                                for (var p = 0; p < numPoints; ++p) {
                                    var x = p * dx
                                    var yMean = mapY(historyL[p].meanDb)
                                    if (p === 0) ctx.moveTo(x, yMean)
                                    else ctx.lineTo(x, yMean)
                                }
                                ctx.stroke()

                                // Draw L peak whiskers
                                ctx.fillStyle = "#E6EEF0"
                                for (var k = 0; k < numPoints; ++k) {
                                    if (historyL[k].peakDb > historyL[k].meanDb + 0.1) {
                                        var px = k * dx
                                        var pyPeak = mapY(historyL[k].peakDb)
                                        var pyMean = mapY(historyL[k].meanDb)
                                        ctx.beginPath()
                                        ctx.moveTo(px, pyMean)
                                        ctx.lineTo(px, pyPeak)
                                        ctx.stroke()

                                        ctx.beginPath()
                                        ctx.arc(px, pyPeak, 1.5, 0, 2 * Math.PI)
                                        ctx.fill()
                                    }
                                }

                                // Draw R mean line trace if dual mono
                                if (historyR && historyR.length === numPoints) {
                                    ctx.strokeStyle = "#2FD98F"
                                    ctx.lineWidth = 1.5
                                    ctx.beginPath()
                                    for (var pr = 0; pr < numPoints; ++pr) {
                                        var xr = pr * dx
                                        var yrMean = mapY(historyR[pr].meanDb)
                                        if (pr === 0) ctx.moveTo(xr, yrMean)
                                        else ctx.lineTo(xr, yrMean)
                                    }
                                    ctx.stroke()
                                }
                            }
                        }
                    }
                }
            }

            // Right Side: Control Panels
            Rectangle {
                id: controlsPanel
                objectName: "compressorControlsPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 6
                color: root.panelBg
                border.color: root.hasValidationError ? root.error : root.borderDark
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 8

                        // Detector & Stereo Link Selector Strip
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            // Detector Mode Group
                            ColumnLayout {
                                spacing: 2
                                Text {
                                    text: "DETECTOR MODE"
                                    color: root.textSecondary
                                    font.family: "Segoe UI"
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                }
                                RowLayout {
                                    spacing: 2
                                    StudioButton {
                                        objectName: "detectorRmsButton"
                                        emphasizeSelectedText: false
                                        text: "RMS"
                                        selected: root.viewModel ? (root.viewModel.detectorMode === "RMS") : true
                                        minimumControlWidth: 56
                                        contentPadding: 6
                                        onClicked: if (root.viewModel) root.viewModel.setDetectorMode("RMS")
                                    }
                                    StudioButton {
                                        objectName: "detectorPeakButton"
                                        emphasizeSelectedText: false
                                        text: "PEAK"
                                        selected: root.viewModel ? (root.viewModel.detectorMode === "PEAK") : false
                                        minimumControlWidth: 56
                                        contentPadding: 6
                                        onClicked: if (root.viewModel) root.viewModel.setDetectorMode("PEAK")
                                    }
                                }
                            }

                            // Stereo Link Group (Applicability bound)
                            ColumnLayout {
                                opacity: root.viewModel ? (root.viewModel.channelLinkEffective ? 1.0 : 0.4) : 1.0
                                enabled: root.viewModel ? root.viewModel.channelLinkEffective : true
                                spacing: 2

                                RowLayout {
                                    spacing: 4
                                    Text {
                                        text: "STEREO LINK"
                                        color: root.textSecondary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                    }
                                    Text {
                                        text: "(MONO - Non-Effective)"
                                        color: root.textMuted
                                        font.family: "Segoe UI"
                                        font.pixelSize: 9
                                        visible: root.viewModel ? !root.viewModel.channelLinkEffective : false
                                    }
                                }

                                RowLayout {
                                    spacing: 2
                                    StudioButton {
                                        objectName: "linkMaxButton"
                                        emphasizeSelectedText: false
                                        text: "LINKED MAX"
                                        selected: root.viewModel ? (root.viewModel.channelLink === "LINKED_MAX") : true
                                        minimumControlWidth: 72
                                        contentPadding: 6
                                        onClicked: if (root.viewModel) root.viewModel.setChannelLink("LINKED_MAX")
                                    }
                                    StudioButton {
                                        objectName: "linkMeanButton"
                                        emphasizeSelectedText: false
                                        text: "LINKED MEAN"
                                        selected: root.viewModel ? (root.viewModel.channelLink === "LINKED_MEAN") : false
                                        minimumControlWidth: 72
                                        contentPadding: 6
                                        onClicked: if (root.viewModel) root.viewModel.setChannelLink("LINKED_MEAN")
                                    }
                                    StudioButton {
                                        objectName: "linkDualMonoButton"
                                        emphasizeSelectedText: false
                                        text: "DUAL MONO"
                                        selected: root.viewModel ? (root.viewModel.channelLink === "DUAL_MONO") : false
                                        minimumControlWidth: 72
                                        contentPadding: 6
                                        onClicked: if (root.viewModel) root.viewModel.setChannelLink("DUAL_MONO")
                                    }
                                }
                            }
                        }

                        // Divider
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#1A2C3D"
                        }

                        // Grid of Numeric Controls & Sliders
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 3
                            columnSpacing: 8
                            rowSpacing: 6

                            // 1. Threshold
                            ColumnLayout {
                                spacing: 2

                                StudioNumericField {
                                    id: thresholdField
                                    objectName: "compressorThresholdField"
                                    compactFieldWidth: root.compactNumericFieldWidth
                                    fieldName: "thresholdDbfs"
                                    labelText: "THRESHOLD"
                                    unitText: "dBFS"
                                    rawText: root.viewModel ? root.viewModel.thresholdText : "-24.0"
                                    viewModel: root.viewModel
                                    interactionHint: "Drag cyan curve point left/right or use the slider.\nEnter to apply • Esc to cancel."
                                    compact: true
                                }

                                StudioParameterSlider {
                                    objectName: "compressorThresholdSlider"
                                    from: -120.0
                                    to: 0.0
                                    stepSize: 0.1
                                    value: root.viewModel ? root.viewModel.draftThresholdDbfs : -24.0
                                    fieldName: "thresholdDbfs"
                                    viewModel: root.viewModel
                                    compact: true
                                    accentColor: "#2ED3FF"
                                    accessibleName: "Threshold continuous adjustment slider"
                                }
                            }

                            // 2. Ratio
                            ColumnLayout {
                                spacing: 2

                                StudioNumericField {
                                    id: ratioField
                                    objectName: "compressorRatioField"
                                    compactFieldWidth: root.compactNumericFieldWidth
                                    fieldName: "ratio"
                                    labelText: "RATIO"
                                    unitText: ": 1"
                                    rawText: root.viewModel ? root.viewModel.ratioText : "2.00"
                                    viewModel: root.viewModel
                                    interactionHint: "Drag emerald curve point up/down or use the slider.\nEnter to apply • Esc to cancel."
                                    compact: true
                                }

                                StudioParameterSlider {
                                    objectName: "compressorRatioSlider"
                                    from: 1.0
                                    to: 20.0
                                    stepSize: 0.05
                                    value: root.viewModel ? root.viewModel.draftRatio : 2.0
                                    fieldName: "ratio"
                                    viewModel: root.viewModel
                                    compact: true
                                    accentColor: "#2FD98F"
                                    accessibleName: "Ratio continuous adjustment slider"
                                }
                            }

                            // 3. Knee
                            ColumnLayout {
                                spacing: 2

                                StudioNumericField {
                                    id: kneeField
                                    objectName: "compressorKneeField"
                                    compactFieldWidth: root.compactNumericFieldWidth
                                    fieldName: "kneeDb"
                                    labelText: "KNEE"
                                    unitText: "dB"
                                    rawText: root.viewModel ? root.viewModel.kneeText : "6.0"
                                    viewModel: root.viewModel
                                    interactionHint: "Drag yellow curve point left/right or use the slider.\nEnter to apply • Esc to cancel."
                                    compact: true
                                }

                                StudioParameterSlider {
                                    objectName: "compressorKneeSlider"
                                    from: 0.0
                                    to: 24.0
                                    stepSize: 0.1
                                    value: root.viewModel ? root.viewModel.draftKneeDb : 6.0
                                    fieldName: "kneeDb"
                                    viewModel: root.viewModel
                                    compact: true
                                    accentColor: "#FFD84A"
                                    accessibleName: "Knee continuous adjustment slider"
                                }
                            }

                            // 4. Attack
                            ColumnLayout {
                                spacing: 2

                                StudioNumericField {
                                    id: attackField
                                    objectName: "compressorAttackField"
                                    compactFieldWidth: root.compactNumericFieldWidth
                                    fieldName: "attackMs"
                                    labelText: "ATTACK"
                                    unitText: "ms"
                                    rawText: root.viewModel ? root.viewModel.attackText : "30.0"
                                    viewModel: root.viewModel
                                    interactionHint: "Drag the slider to adjust.\nEnter to apply • Esc to cancel."
                                    compact: true
                                }

                                StudioParameterSlider {
                                    objectName: "compressorAttackSlider"
                                    from: 0.1
                                    to: 500.0
                                    stepSize: 0.1
                                    logarithmic: true
                                    value: root.viewModel ? root.viewModel.draftAttackMs : 30.0
                                    fieldName: "attackMs"
                                    viewModel: root.viewModel
                                    compact: true
                                    accessibleName: "Attack continuous adjustment slider"
                                }
                            }

                            // 5. Release
                            ColumnLayout {
                                spacing: 2

                                StudioNumericField {
                                    id: releaseField
                                    objectName: "compressorReleaseField"
                                    compactFieldWidth: root.compactNumericFieldWidth
                                    fieldName: "releaseMs"
                                    labelText: "RELEASE"
                                    unitText: "ms"
                                    rawText: root.viewModel ? root.viewModel.releaseText : "200.0"
                                    viewModel: root.viewModel
                                    interactionHint: "Drag the slider to adjust.\nEnter to apply • Esc to cancel."
                                    compact: true
                                }

                                StudioParameterSlider {
                                    objectName: "compressorReleaseSlider"
                                    from: 1.0
                                    to: 5000.0
                                    stepSize: 1.0
                                    logarithmic: true
                                    value: root.viewModel ? root.viewModel.draftReleaseMs : 200.0
                                    fieldName: "releaseMs"
                                    viewModel: root.viewModel
                                    compact: true
                                    accessibleName: "Release continuous adjustment slider"
                                }
                            }

                            // 6. RMS Time Constant (Applicability bound)
                            ColumnLayout {
                                spacing: 2
                                opacity: root.viewModel ? (root.viewModel.rmsTimeEffective ? 1.0 : 0.4) : 1.0

                                StudioNumericField {
                                    id: rmsTimeField
                                    objectName: "compressorRmsTimeField"
                                    compactFieldWidth: root.compactNumericFieldWidth
                                    fieldName: "rmsTimeConstantMs"
                                    labelText: root.viewModel && root.viewModel.rmsTimeEffective ? "RMS TIME" : "RMS TIME (PEAK)"
                                    unitText: "ms"
                                    rawText: root.viewModel ? root.viewModel.rmsTimeConstantText : "50.0"
                                    viewModel: root.viewModel
                                    interactionHint: "Drag the slider to adjust.\nEnter to apply • Esc to cancel."
                                    compact: true
                                    enabled: root.viewModel ? root.viewModel.rmsTimeEffective : true
                                }

                                StudioParameterSlider {
                                    objectName: "compressorRmsTimeSlider"
                                    from: 1.0
                                    to: 500.0
                                    stepSize: 0.1
                                    logarithmic: true
                                    value: root.viewModel ? root.viewModel.draftRmsTimeConstantMs : 50.0
                                    fieldName: "rmsTimeConstantMs"
                                    viewModel: root.viewModel
                                    compact: true
                                    enabled: root.viewModel ? root.viewModel.rmsTimeEffective : true
                                    accessibleName: "RMS Time Constant continuous adjustment slider"
                                }
                            }

                            // 7. Lookahead
                            ColumnLayout {
                                spacing: 2

                                StudioNumericField {
                                    id: lookAheadField
                                    objectName: "compressorLookAheadField"
                                    compactFieldWidth: root.compactNumericFieldWidth
                                    fieldName: "lookAheadMs"
                                    labelText: "LOOKAHEAD"
                                    unitText: "ms"
                                    rawText: root.viewModel ? root.viewModel.lookAheadText : "5.0"
                                    viewModel: root.viewModel
                                    interactionHint: "Drag the slider to adjust.\nEnter to apply • Esc to cancel."
                                    compact: true
                                }

                                StudioParameterSlider {
                                    objectName: "compressorLookAheadSlider"
                                    from: 0.0
                                    to: 20.0
                                    stepSize: 0.1
                                    value: root.viewModel ? root.viewModel.draftLookAheadMs : 5.0
                                    fieldName: "lookAheadMs"
                                    viewModel: root.viewModel
                                    compact: true
                                    accessibleName: "Lookahead continuous adjustment slider"
                                }
                            }

                            // 8. Mix
                            ColumnLayout {
                                spacing: 2

                                StudioNumericField {
                                    id: mixField
                                    objectName: "compressorMixField"
                                    compactFieldWidth: root.compactNumericFieldWidth
                                    fieldName: "mixPercent"
                                    labelText: "MIX"
                                    unitText: "%"
                                    rawText: root.viewModel ? root.viewModel.mixPercentText : "100.0"
                                    viewModel: root.viewModel
                                    interactionHint: "Drag the slider to adjust.\nEnter to apply • Esc to cancel."
                                    compact: true
                                }

                                StudioParameterSlider {
                                    objectName: "compressorMixSlider"
                                    from: 0.0
                                    to: 100.0
                                    stepSize: 0.5
                                    value: root.viewModel ? root.viewModel.draftMixPercent : 100.0
                                    fieldName: "mixPercent"
                                    viewModel: root.viewModel
                                    compact: true
                                    accessibleName: "Mix continuous adjustment slider"
                                }
                            }

                            // 9. Make-up Gain
                            ColumnLayout {
                                spacing: 2

                                StudioNumericField {
                                    id: makeupField
                                    objectName: "compressorMakeupField"
                                    compactFieldWidth: root.compactNumericFieldWidth
                                    fieldName: "makeupGainDb"
                                    labelText: "MAKE-UP"
                                    unitText: "dB"
                                    rawText: root.viewModel ? root.viewModel.makeupGainText : "0.0"
                                    viewModel: root.viewModel
                                    interactionHint: "Drag coral curve point up/down or use the slider.\nEnter to apply • Esc to cancel."
                                    compact: true
                                }

                                StudioParameterSlider {
                                    objectName: "compressorMakeupSlider"
                                    from: -24.0
                                    to: 24.0
                                    stepSize: 0.1
                                    value: root.viewModel ? root.viewModel.draftMakeupGainDb : 0.0
                                    fieldName: "makeupGainDb"
                                    viewModel: root.viewModel
                                    compact: true
                                    accentColor: "#FF6B6B"
                                    fillFromOrigin: true
                                    positionFillOrigin: 0.0
                                    accessibleName: "Make-up Gain continuous adjustment slider"
                                }
                            }
                        }

                        // Validation Error Display
                        Text {
                            id: validationErrorLabel
                            objectName: "compressorValidationError"
                            text: root.validationErrorText
                            color: root.error
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            visible: root.hasValidationError
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }
    }
}
