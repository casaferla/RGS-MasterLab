import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "stereoMsEditor"
    property var viewModel: null
    property bool showLocalValidation: true

    readonly property color seaGreen: "#4A9A88"
    readonly property color widthHue: "#2ED3FF"
    readonly property color cutoffHue: "#2FD98F"
    readonly property color lowHue: "#FFD84A"
    // Secondary, muted M/S colors: only a focused or pressed gain slider
    // lights its corresponding border/halo. Main response controls retain
    // their direct cyan / emerald / yellow handle semantics.
    readonly property color midGainHue: "#4A9A88"
    readonly property color sideGainHue: "#659CAF"
    // Same numeric rectangle width and matching slider span for ALL five
    // editable Stereo/M-S scalar controls, regardless of row or unit.
    readonly property int numericFieldWidth: 86
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
                id: validationFeedbackText
                objectName: "stereoMsValidationFeedbackText"
                text: root.viewModel ? root.viewModel.validationMessage : ""
                visible: root.showLocalValidation && text.length > 0
                color: "#F27683"
                font.family: "Segoe UI"
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.maximumWidth: 250

                // Preserve the compact header, but never hide the full
                // representability interval behind Text.ElideRight.
                HoverHandler { id: validationFeedbackHover }
                ToolTip.text: validationFeedbackText.text
                ToolTip.visible: (validationFeedbackHover.hovered && validationFeedbackText.text.length > 0) && stationaryTooltipDwell1.ready
                ToolTip.delay: 0
                StudioTooltipDwell {
                    id: stationaryTooltipDwell1
                    anchors.fill: parent
                }
                Accessible.name: validationFeedbackText.text
            }
            StudioButton {
                objectName: "stereoMsResetButton"
                text: "Reset to Default"
                minimumControlWidth: 140
                enabled: root.viewModel && root.viewModel.available
                accentColor: root.seaGreen
                ToolTip.text: "Restore all Stereo/M-S parameters to defaults. This does not change the module bypass state."
                ToolTip.visible: (hovered) && stationaryTooltipDwell2.ready
                ToolTip.delay: 0
                StudioTooltipDwell {
                    id: stationaryTooltipDwell2
                    anchors.fill: parent
                }
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
                HoverHandler { id: stereoDynamicHover }
                ToolTip.text: "Actual post-M/S output density (M vertical, S horizontal), measured correlation and low-frequency Side energy. This is observation only; dragging the cloud does not edit audio."
                ToolTip.visible: (stereoDynamicHover.hovered) && stationaryTooltipDwell3.ready
                ToolTip.delay: 0
                StudioTooltipDwell {
                    id: stationaryTooltipDwell3
                    anchors.fill: parent
                }
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
                    // Symmetric clearance between the M/S letters and the
                    // two orthogonal axes. Diagonals retain their full reach.
                    readonly property real orthogonalInsetPx: 22

                    // B4c3a: only previously completed, actually heard
                    // Grid20 buckets from the accepted post-M15 stage.
                    // Grid33 is the frozen DSP quantizer: x=Side, y=Mid.
                    // The identical pixel scale on X/Y preserves geometry;
                    // all-zero PCM is intentionally non-luminous.
                    Canvas {
                        id: cloud
                        objectName: "stereoMsGoniometerCloud"
                        anchors.fill: parent
                        readonly property string renderStyle: "SOFT_RADIAL_DENSITY"
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
                        // Accumulate the *same* completed, sink-heard Grid33
                        // buckets once on a telemetry change. The previous
                        // occupiedCells binding and Canvas painter each scanned
                        // every bucket/cell separately on the UI thread.
                        // This is presentation caching, not a DSP conversion.
                        readonly property var densitySnapshot: {
                            const totals = []
                            for (let i = 0; i < 1089; ++i) totals.push(0)
                            if (!displayHistory || !heardBuckets
                                    || heardBuckets.length === 0)
                                return { counts: totals, occupied: 0 }
                            for (let j = 0; j < heardBuckets.length; ++j) {
                                const bucket = heardBuckets[j]
                                if (!bucket || !bucket.occupancy
                                        || bucket.occupancy.length !== 1089)
                                    continue
                                const zeros = Number(bucket.zeroVectorCount)
                                for (let i = 0; i < 1089; ++i) {
                                    let n = Number(bucket.occupancy[i])
                                    if (i === 544)
                                        n = Math.max(0, n - zeros)
                                    totals[i] += Math.max(0, n)
                                }
                            }
                            let occupied = 0
                            for (let i = 0; i < 1089; ++i)
                                if (totals[i] > 0) ++occupied
                            return { counts: totals, occupied: occupied }
                        }
                        readonly property int occupiedCells: densitySnapshot.occupied
                        visible: displayHistory && occupiedCells > 0
                        opacity: telemetryPaused ? 0.42 : 1.0
                        renderTarget: Canvas.Image
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            if (!cloud.visible || !cloud.densitySnapshot) return
                            // Each dot is a measured Grid33 cell with a soft
                            // Sea Green radial falloff (material density,
                            // not a new interpolated audio point).
                            // Fixed, monotone count-to-opacity law: no AGC,
                            // synthetic particles or persistence animation.
                            const side = Math.min(width, height) / 33
                            const haloRadius = side * 0.68
                            const counts = cloud.densitySnapshot.counts
                            for (let i = 0; i < 1089; ++i) {
                                const count = counts[i]
                                if (count <= 0) continue
                                const binX = i % 33
                                const binY = Math.floor(i / 33)
                                const x = (binX + 0.5) * side
                                const y = (32 - binY + 0.5) * side
                                const density = Math.log(1 + count)
                                const coreAlpha = Math.min(0.94,
                                    0.22 + 0.105 * density)
                                const shoulderAlpha = Math.min(0.60,
                                    0.13 + 0.078 * density)
                                const haloAlpha = Math.min(0.22,
                                    0.024 + 0.034 * density)
                                const pigment = ctx.createRadialGradient(
                                    x, y, 0, x, y, haloRadius)
                                pigment.addColorStop(0,
                                    "rgba(137,217,193," + coreAlpha + ")")
                                pigment.addColorStop(0.24,
                                    "rgba(74,154,136," + shoulderAlpha + ")")
                                pigment.addColorStop(0.63,
                                    "rgba(74,154,136," + haloAlpha + ")")
                                pigment.addColorStop(1,
                                    "rgba(74,154,136,0)")
                                ctx.fillStyle = pigment
                                ctx.fillRect(x - haloRadius, y - haloRadius,
                                    2 * haloRadius, 2 * haloRadius)
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
                        objectName: "stereoMsMidAxis"
                        anchors.centerIn: parent
                        width: 1
                        height: Math.max(0, parent.height - 2 * axes.orthogonalInsetPx)
                        color: "#5F4A9A88"
                    }
                    Rectangle {
                        objectName: "stereoMsSideAxis"
                        anchors.centerIn: parent
                        width: Math.max(0, parent.width - 2 * axes.orthogonalInsetPx)
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
                        objectName: "stereoMsMidAxisLabel"
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "M"
                        color: "#A1B5C9"
                        font.family: "Consolas"
                        font.pixelSize: 10
                    }
                    Text {
                        objectName: "stereoMsSideAxisLabel"
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
                    HoverHandler { id: stereoTelemetryHover }
                    ToolTip.text: "Telemetric state reflects real published and audible post-M/S data. Unavailable, paused or bypassed readings must not be presented as live measurements."
                    ToolTip.visible: (stereoTelemetryHover.hovered) && stationaryTooltipDwell4.ready
                    ToolTip.delay: 0
                    StudioTooltipDwell {
                        id: stationaryTooltipDwell4
                        anchors.fill: parent
                    }
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
                        HoverHandler { id: stereoCorrHover }
                        ToolTip.text: "Observed L/R correlation for verified post-M/S output. Undefined or missing measurements display --, not an invented zero."
                        ToolTip.visible: (stereoCorrHover.hovered) && stationaryTooltipDwell5.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: stationaryTooltipDwell5
                            anchors.fill: parent
                        }
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
                        HoverHandler { id: stereoSideLowHover }
                        ToolTip.text: "Observed low-frequency Side energy, before / after the Low Width operation. Only verified output-stage values are shown."
                        ToolTip.visible: (stereoSideLowHover.hovered) && stationaryTooltipDwell6.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: stationaryTooltipDwell6
                            anchors.fill: parent
                        }
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
                        enabled: root.viewModel && root.viewModel.spatialControlsEffective
                        opacity: enabled ? 1 : 0.48
                        fieldObjectName: "stereoMsWidthField"
                        sliderObjectName: "stereoMsWidthSlider"
                        compactFieldWidth: root.numericFieldWidth
                        labelText: "WIDTH"
                        unitText: "%"
                        fieldName: "widthPercent"
                        rawText: root.viewModel ? root.viewModel.draftWidthPercent.toFixed(1) : "100.0"
                        viewModel: root.viewModel
                        interactionHint: "WIDTH: 0–200%. Drag the cyan handle vertically, use this slider, or type an exact number; Enter applies, Esc restores."
                        sliderInteractionHint: "WIDTH 0–200%: change the broadband Mid/Side balance. Release commits one DSP change; no mouse-wheel edits."
                        deferResponseWhileDragging: true
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
                        enabled: root.viewModel && root.viewModel.spatialControlsEffective
                            && root.viewModel.monoBassControlsEffective
                        opacity: enabled ? 1 : 0.48
                        fieldObjectName: "stereoMsCutoffField"
                        sliderObjectName: "stereoMsCutoffSlider"
                        compactFieldWidth: root.numericFieldWidth
                        labelText: "MONO BASS CUTOFF"
                        unitText: "Hz"
                        fieldName: "monoBassCutoffHz"
                        interactionHint: "CUTOFF: 40–300 Hz subject to source rate. Drag the green handle horizontally or type the exact frequency; Enter applies, Esc restores."
                        sliderInteractionHint: "MONO BASS CUTOFF: select the crossover frequency, only when LR12/LR24 is effective."
                        deferResponseWhileDragging: true
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
                        enabled: root.viewModel && root.viewModel.spatialControlsEffective
                            && root.viewModel.monoBassControlsEffective
                        opacity: enabled ? 1 : 0.48
                        fieldObjectName: "stereoMsLowWidthField"
                        sliderObjectName: "stereoMsLowWidthSlider"
                        compactFieldWidth: root.numericFieldWidth
                        labelText: "LOW WIDTH"
                        unitText: "%"
                        fieldName: "lowBandWidthPercent"
                        interactionHint: "LOW WIDTH: 0–100%. Drag the yellow handle vertically or enter a value; only effective while Mono Bass is active."
                        sliderInteractionHint: "LOW WIDTH 0–100%: control low-frequency Side width without changing the independent Mid/Side gains."
                        deferResponseWhileDragging: true
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

                    // M15's final two editable numeric fields get the same
                    // third input mode as all other numeric DSP controls.
                    // No graph handles exist for these independent gains.
                    StudioNumericSliderPair {
                        objectName: "stereoMsMidGainControl"
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        enabled: root.viewModel && root.viewModel.spatialControlsEffective
                        opacity: enabled ? 1 : 0.48
                        fieldObjectName: "stereoMsMidGainField"
                        sliderObjectName: "stereoMsMidGainSlider"
                        compactFieldWidth: root.numericFieldWidth
                        labelText: "MID GAIN"
                        unitText: "dB"
                        fieldName: "midGainDb"
                        rawText: root.viewModel ? root.viewModel.midGainDb.toFixed(1) : "0.0"
                        viewModel: root.viewModel
                        interactionHint: "MID GAIN: -12 to +12 dB; edit numerically, Enter to apply, Esc to restore."
                        sliderInteractionHint: "MID GAIN: move the gain from -12 to +12 dB, measured from the zero dB center; release to commit."
                        deferResponseWhileDragging: true
                        from: -12
                        to: 12
                        stepSize: 0.1
                        value: root.viewModel ? root.viewModel.midGainDb : 0
                        fillFromOrigin: true
                        positionFillOrigin: 0
                        positionFillColor: Qt.alpha(root.midGainHue, 0.38)
                        accentColor: root.midGainHue
                        focusAccentOnly: true
                        sliderAccessibleName: "Mid gain continuous adjustment"
                    }
                    StudioNumericSliderPair {
                        objectName: "stereoMsSideGainControl"
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        enabled: root.viewModel && root.viewModel.spatialControlsEffective
                        opacity: enabled ? 1 : 0.48
                        fieldObjectName: "stereoMsSideGainField"
                        sliderObjectName: "stereoMsSideGainSlider"
                        compactFieldWidth: root.numericFieldWidth
                        labelText: "SIDE GAIN"
                        unitText: "dB"
                        fieldName: "sideGainDb"
                        rawText: root.viewModel ? root.viewModel.sideGainDb.toFixed(1) : "0.0"
                        viewModel: root.viewModel
                        interactionHint: "SIDE GAIN: -24 to +12 dB; edit numerically, Enter to apply, Esc to restore."
                        sliderInteractionHint: "SIDE GAIN: move the gain from -24 to +12 dB, with zero at two-thirds along the track; release to commit."
                        deferResponseWhileDragging: true
                        from: -24
                        to: 12
                        stepSize: 0.1
                        value: root.viewModel ? root.viewModel.sideGainDb : 0
                        fillFromOrigin: true
                        positionFillOrigin: 0
                        positionFillColor: Qt.alpha(root.sideGainHue, 0.38)
                        accentColor: root.sideGainHue
                        focusAccentOnly: true
                        sliderAccessibleName: "Side gain continuous adjustment"
                    }

                    ColumnLayout {
                        id: monoBassModeControls
                        objectName: "stereoMsMonoBassModeControls"
                        Layout.alignment: Qt.AlignTop
                        spacing: 2
                        Text {
                            id: monoBassModeLabel
                            objectName: "stereoMsMonoBassModeLabel"
                            text: "MONO BASS"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            height: 14
                            Layout.preferredHeight: 14
                        }
                        RowLayout {
                            id: monoBassModeButtons
                            objectName: "stereoMsMonoBassModeButtons"
                            spacing: 3
                            Repeater {
                                model: ["OFF", "LR12", "LR24"]
                                delegate: StudioSegmentButton {
                                    required property string modelData
                                    text: modelData
                                    minimumControlWidth: 58
                                    selected: root.viewModel && root.viewModel.monoBassMode === modelData
                                    accentColor: root.seaGreen
                                    selectedFamilyFill: true
                                    ToolTip.text: modelData === "OFF"
                                        ? "Disable Mono Bass crossover; broadband M/S controls remain available."
                                        : modelData === "LR12"
                                            ? "Use the 12 dB/oct Linkwitz-Riley Mono Bass crossover."
                                            : "Use the 24 dB/oct Linkwitz-Riley Mono Bass crossover."
                                    ToolTip.visible: (hovered) && stationaryTooltipDwell7.ready
                                    ToolTip.delay: 0
                                    StudioTooltipDwell {
                                        id: stationaryTooltipDwell7
                                        anchors.fill: parent
                                    }
                                    enabled: root.viewModel && root.viewModel.spatialControlsEffective
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
                        accentColor: root.seaGreen
                        useFamilyAccent: true
                        ToolTip.text: "Mute the Side component while preserving the stored Mid/Side gain parameters. The resulting stereo collapses toward Mid only."
                        ToolTip.visible: (hovered) && stationaryTooltipDwell8.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: stationaryTooltipDwell8
                            anchors.fill: parent
                        }
                        checked: root.viewModel ? root.viewModel.sideMuted : false
                        enabled: root.viewModel && root.viewModel.spatialControlsEffective
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
