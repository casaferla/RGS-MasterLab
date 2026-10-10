import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "stereoMsEditor"
    property var viewModel: null

    readonly property color seaGreen: "#4A9A88"
    readonly property color widthHue: "#2ED3FF"
    readonly property color cutoffHue: "#2FD98F"
    readonly property color lowHue: "#FFD84A"
    readonly property color sliderFill: Qt.rgba(74 / 255, 154 / 255, 136 / 255, 0.38)

    // EQ-like top expansive region, but a separate near-square, strictly
    // observational M/S output well: its coordinates must never be stretched.
    ColumnLayout {
        anchors.fill: parent
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 27
            spacing: 8
            Text {
                text: "SPATIAL CONTROL"
                color: "#A1B5C9"
                font.family: "Segoe UI"
                font.pixelSize: 11
                font.weight: Font.Bold
            }
            Rectangle { width: 2; height: 14; color: root.seaGreen }
            Text {
                text: "Width · Mono Bass"
                color: "#D1E2E8"
                font.family: "Segoe UI"
                font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
            Text {
                text: root.viewModel ? root.viewModel.validationMessage : ""
                visible: text.length > 0
                color: "#F27683"
                font.family: "Segoe UI"
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.maximumWidth: 250
            }
            StudioButton {
                objectName: "stereoMsResetButton"
                text: "Reset to Default"
                minimumControlWidth: 140
                enabled: root.viewModel && root.viewModel.available
                onClicked: if (root.viewModel) root.viewModel.resetToDefault()
                Accessible.name: "Reset Stereo M/S parameters"
            }
        }

        RowLayout {
            id: visualWells
            objectName: "stereoMsVisualWells"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 150
            spacing: 9

            StereoMsResponseGraph {
                id: responseGraph
                objectName: "stereoMsActiveSurface"
                viewModel: root.viewModel
                Layout.minimumWidth: 260
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 580
            }

            Rectangle {
                id: dynamicWell
                objectName: "stereoMsDynamicWell"
                Layout.minimumWidth: 210
                Layout.preferredWidth: Math.min(330, Math.max(210, visualWells.height - 8))
                Layout.maximumWidth: 350
                Layout.fillHeight: true
                radius: 6
                color: "#081824"
                border.width: 1
                border.color: "#1A3E55"

                // The observational square lives in the middle band; text
                // above and telemetry/status below keep fixed edge positions.
                readonly property real axesBandTop: goniometerTitle.y
                    + goniometerTitle.height + 8
                readonly property real axesBandBottom: telemetryStatus.y - 8

                Text {
                    id: goniometerTitle
                    objectName: "stereoMsGoniometerTitle"
                    x: 12
                    y: 8
                    text: "OUTPUT M/S GONIOMETER"
                    color: "#A1B5C9"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }

                Item {
                    id: axes
                    objectName: "stereoMsStaticAxes"
                    width: Math.max(0, Math.min(parent.width - 30,
                        dynamicWell.axesBandBottom - dynamicWell.axesBandTop))
                    height: width
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: dynamicWell.axesBandTop + Math.max(0,
                        (dynamicWell.axesBandBottom - dynamicWell.axesBandTop - height) / 2)

                    // B4c3a: only previously completed, actually heard
                    // Grid20 buckets from the accepted post-M15 stage.
                    // Grid33 is the frozen DSP quantizer: x=Side, y=Mid.
                    // The identical pixel scale on X/Y preserves geometry;
                    // all-zero PCM is intentionally non-luminous.
                    Canvas {
                        id: cloud
                        objectName: "stereoMsGoniometerCloud"
                        anchors.fill: parent
                        readonly property var heardBuckets: root.viewModel
                            ? root.viewModel.telemetryDensityBuckets : []
                        readonly property bool telemetryLive: root.viewModel
                            && root.viewModel.telemetryActive
                        readonly property bool telemetryPaused: root.viewModel
                            && root.viewModel.telemetryStatus === "PAUSED"
                        // PAUSED retains the last completed heard bucket set,
                        // but it is NOT live. STOP/BYPASS/TRANSITION/other
                        // audition targets cannot display this frozen cloud.
                        readonly property bool displayHistory: telemetryLive
                            || telemetryPaused
                        readonly property int occupiedCells: {
                            if (!displayHistory || !heardBuckets || heardBuckets.length === 0)
                                return 0
                            let occupied = 0
                            for (let i = 0; i < 1089; ++i) {
                                let count = 0
                                for (let j = 0; j < heardBuckets.length; ++j) {
                                    const bucket = heardBuckets[j]
                                    if (!bucket || !bucket.occupancy
                                            || bucket.occupancy.length !== 1089)
                                        continue
                                    let contribution = Number(bucket.occupancy[i])
                                    if (i === 544)
                                        contribution = Math.max(0, contribution
                                            - Number(bucket.zeroVectorCount))
                                    count += Math.max(0, contribution)
                                }
                                if (count > 0) ++occupied
                            }
                            return occupied
                        }
                        visible: displayHistory && occupiedCells > 0
                        opacity: telemetryPaused ? 0.42 : 1.0
                        renderTarget: Canvas.Image
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            if (!cloud.visible || !cloud.heardBuckets) return
                            // Fixed count-to-intensity curve. There is NO
                            // frame-local AGC, cosmetic random particles,
                            // synthetic stereo analysis or persistence timer.
                            const side = Math.min(width, height) / 33
                            const dot = Math.max(1.2, Math.min(3.5, side * 0.77))
                            for (let i = 0; i < 1089; ++i) {
                                let count = 0
                                for (let j = 0; j < cloud.heardBuckets.length; ++j) {
                                    const b = cloud.heardBuckets[j]
                                    if (!b || !b.occupancy || b.occupancy.length !== 1089)
                                        continue
                                    let n = Number(b.occupancy[i])
                                    if (i === 544)
                                        n = Math.max(0, n - Number(b.zeroVectorCount))
                                    count += Math.max(0, n)
                                }
                                if (count <= 0) continue
                                const binX = i % 33
                                const binY = Math.floor(i / 33)
                                const x = (binX + 0.5) * side
                                const y = (32 - binY + 0.5) * side
                                const alpha = Math.min(0.84,
                                    0.14 + 0.115 * Math.log(1 + count))
                                ctx.fillStyle = Qt.rgba(74 / 255, 154 / 255,
                                    136 / 255, alpha)
                                ctx.fillRect(x - dot / 2, y - dot / 2, dot, dot)
                            }
                        }
                        Connections {
                            target: root.viewModel
                            function onTelemetryChanged() { cloud.requestPaint() }
                        }
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        onVisibleChanged: requestPaint()
                        Component.onCompleted: requestPaint()
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 1
                        height: parent.height
                        color: "#5F4A9A88"
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width
                        height: 1
                        color: "#5F4A9A88"
                    }
                    Rectangle {
                        width: Math.hypot(parent.width, parent.height)
                        height: 1
                        color: "#324A9A88"
                        anchors.centerIn: parent
                        rotation: 45
                    }
                    Rectangle {
                        width: Math.hypot(parent.width, parent.height)
                        height: 1
                        color: "#324A9A88"
                        anchors.centerIn: parent
                        rotation: -45
                    }
                    Text {
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "M"
                        color: "#A1B5C9"
                        font.family: "Consolas"
                        font.pixelSize: 10
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "S"
                        color: "#A1B5C9"
                        font.family: "Consolas"
                        font.pixelSize: 10
                    }
                    // The cloud is observational only; no drag handlers.
                    // PAUSED holds the last real cloud dimmed, not moving.
                    // Missing audio, STOPPED, bypass, non-Processed audition
                    // and transition never synthesize density on the surface.
                }

                Text {
                    id: telemetryStatus
                    objectName: "stereoMsTelemetryStatus"
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: telemetryRow.top
                    anchors.bottomMargin: 9
                    text: root.viewModel
                        ? (root.viewModel.telemetryStatus === "ACTIVE"
                            ? (cloud.occupiedCells > 0
                                ? "ACTIVE · POST M/S OUTPUT" : "ACTIVE · NO DENSITY")
                            : root.viewModel.telemetryStatus)
                        : "REAL-TIME TELEMETRY UNAVAILABLE"
                    color: "#9EB6B9"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                }

                RowLayout {
                    id: telemetryRow
                    objectName: "stereoMsTelemetryPlaceholder"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 10
                    spacing: 10
                    Text {
                        objectName: "stereoMsCorrelationReadout"
                        readonly property var metric: root.viewModel
                            ? root.viewModel.telemetryCorrelation : ({})
                        text: metric.valid === true && metric.value !== undefined
                            ? "CORR  " + Number(metric.value).toFixed(2)
                            : "CORR  --"
                        color: "#A1B5C9"
                        font.family: "Consolas"
                        font.pixelSize: 10
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        objectName: "stereoMsSideLowReadout"
                        readonly property var metric: root.viewModel
                            ? root.viewModel.telemetrySideLow : ({})
                        text: metric.before !== undefined && metric.after !== undefined
                            ? "SIDE LOW  " + Number(metric.before).toPrecision(3)
                                + " / " + Number(metric.after).toPrecision(3)
                            : "SIDE LOW  -- / --"
                        color: "#A1B5C9"
                        font.family: "Consolas"
                        font.pixelSize: 10
                    }
                }
            }
        }

        Rectangle {
            objectName: "stereoMsControlsRegion"
            Layout.fillWidth: true
            Layout.preferredHeight: 166
            Layout.minimumHeight: 166
            radius: 6
            color: "#12243F"
            border.color: "#2C5A78"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 9
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    // Canonical field+slider pair owns exact numeric-box alignment;
                    // unit suffix and surplus column width never extend the track.
                    StudioNumericSliderPair {
                        objectName: "stereoMsWidthControl"
                        Layout.fillWidth: true
                        fieldObjectName: "stereoMsWidthField"
                        sliderObjectName: "stereoMsWidthSlider"
                        compactFieldWidth: 86
                        labelText: "WIDTH"
                        unitText: "%"
                        fieldName: "widthPercent"
                        rawText: root.viewModel ? root.viewModel.draftWidthPercent.toFixed(1) : "100.0"
                        viewModel: root.viewModel
                        interactionHint: "Drag the cyan handle vertically, or use slider. Enter to apply."
                        from: 0
                        to: 200
                        stepSize: 0.5
                        value: root.viewModel ? root.viewModel.draftWidthPercent : 100
                        positionFillColor: root.sliderFill
                        hasSemanticAccent: true
                        semanticAccent: root.widthHue
                        sliderAccessibleName: "Broadband stereo width"
                    }

                    StudioNumericSliderPair {
                        objectName: "stereoMsCutoffControl"
                        Layout.fillWidth: true
                        fieldObjectName: "stereoMsCutoffField"
                        sliderObjectName: "stereoMsCutoffSlider"
                        compactFieldWidth: 86
                        labelText: "MONO BASS CUTOFF"
                        unitText: "Hz"
                        fieldName: "monoBassCutoffHz"
                        rawText: root.viewModel ? root.viewModel.monoBassCutoffHz.toFixed(1) : "120.0"
                        viewModel: root.viewModel
                        from: 40
                        to: 300
                        stepSize: 1
                        value: root.viewModel ? root.viewModel.monoBassCutoffHz : 120
                        positionFillColor: root.sliderFill
                        hasSemanticAccent: true
                        semanticAccent: root.cutoffHue
                        sliderAccessibleName: "Mono Bass cutoff frequency"
                    }

                    StudioNumericSliderPair {
                        objectName: "stereoMsLowWidthControl"
                        Layout.fillWidth: true
                        fieldObjectName: "stereoMsLowWidthField"
                        sliderObjectName: "stereoMsLowWidthSlider"
                        compactFieldWidth: 86
                        labelText: "LOW WIDTH"
                        unitText: "%"
                        fieldName: "lowBandWidthPercent"
                        rawText: root.viewModel ? root.viewModel.lowBandWidthPercent.toFixed(1) : "100.0"
                        viewModel: root.viewModel
                        from: 0
                        to: 100
                        stepSize: 1
                        value: root.viewModel ? root.viewModel.lowBandWidthPercent : 100
                        positionFillColor: root.sliderFill
                        hasSemanticAccent: true
                        semanticAccent: root.lowHue
                        sliderAccessibleName: "Low-frequency Side width"
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#304E64"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    StudioNumericField {
                        objectName: "stereoMsMidGainField"
                        compact: true
                        compactFieldWidth: 78
                        labelText: "MID GAIN"
                        unitText: "dB"
                        fieldName: "midGainDb"
                        rawText: root.viewModel ? root.viewModel.midGainDb.toFixed(1) : "0.0"
                        viewModel: root.viewModel
                    }
                    StudioNumericField {
                        objectName: "stereoMsSideGainField"
                        compact: true
                        compactFieldWidth: 78
                        labelText: "SIDE GAIN"
                        unitText: "dB"
                        fieldName: "sideGainDb"
                        rawText: root.viewModel ? root.viewModel.sideGainDb.toFixed(1) : "0.0"
                        viewModel: root.viewModel
                    }

                    ColumnLayout {
                        spacing: 3
                        Text {
                            text: "MONO BASS"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        RowLayout {
                            spacing: 3
                            Repeater {
                                model: ["OFF", "LR12", "LR24"]
                                delegate: StudioSegmentButton {
                                    required property string modelData
                                    text: modelData
                                    minimumControlWidth: 58
                                    selected: root.viewModel && root.viewModel.monoBassMode === modelData
                                    accentColor: root.seaGreen
                                    enabled: root.viewModel && root.viewModel.available
                                    onClicked: if (root.viewModel
                                            && root.viewModel.setDraftMonoBassMode(modelData))
                                        root.viewModel.commitDraft()
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    StudioToggle {
                        objectName: "stereoMsSideMutedToggle"
                        text: "Side Muted"
                        checked: root.viewModel ? root.viewModel.sideMuted : false
                        enabled: root.viewModel && root.viewModel.available
                        onClicked: {
                            if (root.viewModel && root.viewModel.setDraftSideMuted(checked))
                                root.viewModel.commitDraft()
                        }
                    }
                }
            }
        }
    }
}
