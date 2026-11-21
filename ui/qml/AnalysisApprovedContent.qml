import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Approved M19/M20/M21 visual review, PR #91 Qt Windows #13.
// Real Product owns one permanent instance per panel. No DEMO, fake DSP
// output, stale numeric residue, or data publication without Phase-6 owner.
Item {
    id: root
    objectName: "approvedAnalysisProductContent"
    // PO 2026-11-16: continuous, no-scroll top-left layout.
    // No vertical/horizontal mode switch; only the genuine M20
    // spectrum expands into spare vertical space.
    property int panelIndex: 0
    property string lifecycle: "MISSING"
    // These are observed, authentic M19–M21 Product measurements under
    // the PO's development GO. Official Phase-6 Gate G remains separate.
    property var observedReadouts: ({})
    readonly property var m19Values: observedReadouts && observedReadouts.m19
        ? observedReadouts.m19 : ({})
    readonly property var m20Values: observedReadouts && observedReadouts.m20
        ? observedReadouts.m20 : ({})
    readonly property var m21Rows: observedReadouts && observedReadouts.m21Rows
        ? observedReadouts.m21Rows : []
    readonly property var spectrumPoints: observedReadouts
        && observedReadouts.m20StaticPsd
        ? observedReadouts.m20StaticPsd : []
    // Read-only 1/12-octave energy of the existing M20 measured bands.
    // These are NOT the separately gated normalized Phase-6 fractions.
    readonly property var bandEnergyPoints: observedReadouts
        && observedReadouts.m20BandEnergy
        ? observedReadouts.m20BandEnergy : []
    readonly property bool hasBandEnergy: bandEnergyPoints.length > 1
    function hasMeasuredSpectrumEnergy() {
        if(!spectrumPoints || spectrumPoints.length < 2) return false
        for(let i=0;i<spectrumPoints.length;++i)
            if(spectrumPoints[i].hz >= 20
                && spectrumPoints[i].linearPower > 0) return true
        return false
    }
    readonly property bool hasSpectrum: hasMeasuredSpectrumEnergy()
    function hasNumber(value) {
        return typeof value === "number" && Number.isFinite(value)
    }
    function metricText(value, key) {
        if(!hasNumber(value)) return "—"
        return Number(value).toFixed(
            key === "width" || key === "P10"
            || key === "P50" || key === "P90" ? 3 : 2)
    }
    // Preserve the sign of a reference-minus-current difference.
    // An exact zero is neutral; positive deltas are explicitly marked
    // so that they cannot be mistaken for unsigned absolute values.
    function deltaText(value, key) {
        if(!hasNumber(value)) return "—"
        const shown=metricText(value,key)
        // Rounded signed zero is neutral in BOTH directions. A negative
        // sub-display-precision delta must never appear as "-0.00".
        if(Number(shown) === 0) return Number(0).toFixed(
            key === "width" || key === "P10"
            || key === "P50" || key === "P90" ? 3 : 2)
        return value > 0 ? "+" + shown : shown
    }
    // Display scales, not mastering quality scores or admissible errors.
    // Only correlation deltas have an exact finite physical maximum (2).
    function deltaDisplayLimit(key) {
        switch(key) {
        case "lufs": return 6.0
        case "rms": return 6.0
        case "samplePeak": return 6.0
        case "truePeak": return 6.0
        case "crest": return 6.0
        case "width": return 1.0
        case "P10": case "P50": case "P90": return 2.0
        default: return NaN
        }
    }
    function deltaDisplayUnit(key) {
        return key === "lufs" ? "LU"
            : key === "width" || key === "P10"
              || key === "P50" || key === "P90"
              ? "ratio" : "dB"
    }
    function metricExplanation(key) {
        switch(key) {
        case "lufs": return "Integrated K-weighted loudness, in LUFS; gated over each independently measured scope."
        case "rms": return "Full-scope root-mean-square signal level, in dBFS."
        case "samplePeak": return "Largest discrete PCM sample magnitude, in dBFS. Not interpolated True Peak."
        case "truePeak": return "M16 True Peak, in dBTP. No numeric substitute is permitted without its own supported measurement."
        case "crest": return "Discrete sample peak minus full-scope RMS, in dB."
        case "width": return "Stereo side/mid energy ratio on the retained gated scope; may exceed unity."
        case "P10": return "10th percentile of local 400 ms centred stereo correlations, at 100 ms intervals."
        case "P50": return "Median of local Grid100 stereo correlations, not a global full-track correlation."
        case "P90": return "90th percentile of local Grid100 stereo correlations."
        default: return "Read-only source metric."
        }
    }
    function availableInRole(role,from,to) {
        if(!m21Rows || m21Rows.length < to) return false
        for(let i=from;i<to;++i)
            if(hasNumber(m21Rows[i][role])) return true
        return false
    }
    function anyRowValue() {
        if(!m21Rows || !m21Rows.length) return false
        for(let i=0;i<m21Rows.length;++i) {
            const row=m21Rows[i]
            if(hasNumber(row.prepared)||hasNumber(row.reference)
                ||hasNumber(row.processed)
                ||hasNumber(row.deltaPrepared)
                ||hasNumber(row.deltaProcessed)) return true
        }
        return false
    }
    readonly property bool panelHasData: panelIndex === 0
        ? (hasNumber(m19Values.lufs) || hasNumber(m19Values.rms)
           || hasNumber(m19Values.samplePeak) || hasNumber(m19Values.crest))
        : panelIndex === 1
        ? (hasSpectrum || hasNumber(m20Values.width)
           || hasNumber(m20Values.P10) || hasNumber(m20Values.P50)
           || hasNumber(m20Values.P90))
        : anyRowValue()
    // M19: bounded vertical elasticity without reflow or font scaling.
    readonly property real m19SpareHeight: Math.max(0, root.height - 306)
    readonly property real m19TileHeight: 96
        + Math.min(56, root.m19SpareHeight * 0.33)
    readonly property real m19RelationHeight: 98
        + Math.min(64, root.m19SpareHeight * 0.34)
    // M21: each row grows gently 23..36 logical px, never further.
    // The whole comparison page centers after its final natural size.
    readonly property real m21RowHeight: Math.max(23,
        Math.min(36, (root.height - 66) / 9))
    // M21 six semantic groups, common to header and each metric row.
    // Five equal 8px gutters; narrower single-number columns make room
    // for two (number + deviation bar) groups at minimum width.
    // Fractions sum to 1: metric .20, 3 readings .13, 2 deltas .205.
    readonly property real m21ColumnGap: 8
    function m21ColumnWidth(column, tableWidth) {
        const usable=Math.max(0,tableWidth - 5*m21ColumnGap)
        if(column===0)return usable*0.20
        if(column>=1 &&column<=3)return usable*0.13
        return usable*0.205
    }
    readonly property color ink: "#E6EEF0"
    readonly property color dim: "#92A5B2"
    readonly property color cyan: "#00C8FF"
    readonly property color copper: "#D19562"
    readonly property color amber: "#F2B632"
    readonly property color edge: "#2C5A78"
    readonly property color panel: "#0E1A24"
    readonly property color raised: "#12243F"

    // Every displayed number comes from the existing authenticated inbox:
    // current owner, real source, live PCM and finite cell are mandatory.
    // Missing, stale and unmeasured values stay visibly unavailable.
    clip: true
    Column {
        id: stack
        objectName: "analysisProductContentStack"
        anchors.left: parent.left
        anchors.top: parent.top
        width: root.width
        spacing: 8

        // The M19 cards grow only to their bounded limit. Afterwards
        // the entire 3+2 group and relation panel center vertically.
        Item {
            id: m19Viewport
            objectName: "m19CenteredViewport"
            visible: root.panelIndex === 0
            width: parent.width
            height: root.height
            Column {
                id: m19
                objectName: "m19LevelsPage"
                width: parent.width
                y: Math.max(0, (m19Viewport.height - m19.height) / 2)
                spacing: 8
            Grid {
                id: levelGrid
                objectName: "m19MetricsGrid"
                width: parent.width
                columns: 3
                columnSpacing: 8
                rowSpacing: 8
                Repeater {
                    model: [
                        {name:"INTEGRATED LOUDNESS",key:"lufs",unit:"LUFS",
                         hint:"EBU · INTEGRATED",
                         tooltip:"Integrated K-weighted loudness over the selected analysis scope. EBU loudness gate; no playback gain is changed."},
                        {name:"RMS LEVEL",key:"rms",unit:"dBFS",
                         hint:"FULL SELECTED SCOPE",
                         tooltip:"Root-mean-square signal level of the selected source and scope in dBFS."},
                        {name:"SAMPLE PEAK",key:"samplePeak",unit:"dBFS",
                         hint:"DISCRETE PEAK",
                         tooltip:"Highest absolute discrete PCM sample, in dBFS. This is not an interpolated True Peak."},
                        {name:"TRUE PEAK",key:"truePeak",unit:"dBTP",
                         hint:"M16 GATE REQUIRED",
                         tooltip:"True Peak is a separate M16 measurement. This cell remains unavailable until its numerical evidence is supported; sample peak is not a substitute."},
                        {name:"CREST FACTOR",key:"crest",unit:"dB",
                         hint:"PEAK − RMS",
                         tooltip:"Sample-peak level minus full-scope RMS level, expressed in dB."}
                    ]
                    delegate: Rectangle {
                        width: (levelGrid.width
                            - (levelGrid.columns-1)*levelGrid.columnSpacing)
                            / levelGrid.columns
                        height: root.m19TileHeight
                        radius: 7
                        color: root.raised
                        border.color: root.edge
                        Item {
                            anchors.fill: parent
                            anchors.margins: 5
                            StudioReadOnlyMeasurement {
                                objectName: "m19MetricValue_" + modelData.key
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                // Preserve 36px numeric typography; trim
                                // only unused 102px intrinsic bottom space.
                                height: 86
                                heroReadout: true
                                labelText: modelData.name
                                unitText: modelData.unit
                                rawText: root.metricText(
                                    root.m19Values[modelData.key],modelData.key)
                                dataAvailable: root.hasNumber(
                                    root.m19Values[modelData.key])
                                accentColor: modelData.key === "truePeak"
                                    ? root.amber : root.cyan
                            }
                        }
                        ToolTip.text: modelData.tooltip
                        ToolTip.visible: m19CardDwell.ready
                        ToolTip.delay: 0
                        StudioTooltipDwell {
                            id: m19CardDwell
                            anchors.fill: parent
                        }
                    }
                }
            }
            Rectangle {
                objectName: "m19ReferenceRelationPanel"
                width: parent.width
                height: root.m19RelationHeight
                color: root.panel
                border.color: root.edge
                radius: 7
                ToolTip.text: "Integrated LUFS for each independent PREPARED, REFERENCE or PROCESSED scope. REFERENCE is its own WAV and sample rate; displayed values are not automatically time-aligned."
                ToolTip.visible: relationDwell.ready
                ToolTip.delay: 0
                StudioTooltipDwell {
                    id: relationDwell
                    anchors.fill: parent
                }
                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 2
                    Text {
                        text: "LEVELS  /  REFERENCE RELATION"
                        color: root.ink
                        font.family: "Segoe UI"
                        font.bold: true
                        font.pixelSize: 12
                    }
                    Repeater {
                        model: [
                            {name:"PREPARED",tone:"#76CDE9"},
                            {name:"PROCESSED",tone:"#7CDDC8"},
                            {name:"REFERENCE",tone:"#D19562"}
                        ]
                        delegate: Row {
                            width: parent.width
                            height: 18
                            spacing: 8
                            Rectangle {
                                width: 4
                                height: 17
                                radius: 2
                                color: modelData.tone
                            }
                            Text {
                                width: 100
                                text: modelData.name
                                color: root.dim
                                font.family: "Segoe UI"
                                font.pixelSize: 11
                            }
                            Text {
                                width: Math.max(20,parent.width-125)
                                text: root.m21Rows.length > 0
                                    && root.hasNumber(root.m21Rows[0][
                                        modelData.name.toLowerCase()])
                                    ? root.metricText(root.m21Rows[0][
                                        modelData.name.toLowerCase()],"lufs") + " LUFS"
                                    : "UNAVAILABLE"
                                color: root.m21Rows.length > 0
                                    && root.hasNumber(root.m21Rows[0][
                                        modelData.name.toLowerCase()])
                                    ? root.ink : root.amber
                                font.family: "Consolas"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
        }

        // M20 static PSD, P06 Width and local Grid100 correlations.
        // The curve uses actual measured linear-power bins; only the
        // vertical display scale is relative to the measured maximum.
        Column {
            id: m20
            objectName: "m20TonalStereoPage"
            visible: root.panelIndex === 1
            width: parent.width
            spacing: 8
            Rectangle {
                objectName: "m20SpectrumPanel"
                width: parent.width
                height: Math.max(140, root.height - 166)
                radius: 7
                color: root.panel
                border.color: root.edge
                Text {
                    text: "AVERAGE SPECTRUM  /  STATIC PSD"
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 11
                    color: root.ink
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    font.bold: true
                }
                Canvas {
                    id: spectrumCanvas
                    objectName: "m20WorkbenchSpectrum"
                    anchors.fill: parent
                    anchors.leftMargin: 37
                    anchors.rightMargin: 13
                    anchors.topMargin: 37
                    anchors.bottomMargin: 31
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    Connections {
                        target: root
                        function onSpectrumPointsChanged() {
                            spectrumCanvas.requestPaint()
                        }
                    }
                    onPaint: {
                        const ctx=getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle="#284459"
                        ctx.lineWidth=1
                        for(let i=0;i<=4;++i) {
                            ctx.beginPath()
                            ctx.moveTo(0,height*i/4)
                            ctx.lineTo(width,height*i/4)
                            ctx.stroke()
                        }
                        for(const x of [0.0,0.233,0.566,0.899,1.0]) {
                            ctx.beginPath()
                            ctx.moveTo(width*x,0)
                            ctx.lineTo(width*x,height)
                            ctx.stroke()
                        }
                        // Authentic mean-channel PSD. Convert power to dB
                        // ONLY for axis rendering, not Phase-6 output data.
                        const points=root.spectrumPoints
                        if(!root.hasSpectrum || !points.length)return
                        const hiHz=20000
                        const loHz=20
                        const logSpan=Math.log10(hiHz/loHz)
                        let maxDb=-Infinity
                        for(let i=0;i<points.length;++i) {
                            const p=points[i]
                            if(p.hz<loHz || p.hz>hiHz || p.linearPower<=0)
                                continue
                            const db=10*Math.log10(p.linearPower)
                            if(Number.isFinite(db))maxDb=Math.max(maxDb,db)
                        }
                        if(!Number.isFinite(maxDb))return
                        // 72 dB display-only vertical span, no fabricated
                        // normalized values or substitute P02 energy.
                        const floorDb=maxDb-72
                        ctx.save()
                        ctx.strokeStyle=root.cyan
                        ctx.lineWidth=1.6
                        ctx.beginPath()
                        let started=false
                        for(let i=0;i<points.length;++i) {
                            const p=points[i]
                            if(p.hz<loHz || p.hz>hiHz || p.linearPower<=0)
                                continue
                            const db=10*Math.log10(p.linearPower)
                            if(!Number.isFinite(db))continue
                            const x=Math.log10(p.hz/loHz)/logSpan*width
                            const y=height*(1-Math.max(0,Math.min(1,
                                (db-floorDb)/72)))
                            if(!started) {ctx.moveTo(x,y);started=true}
                            else ctx.lineTo(x,y)
                        }
                        if(started)ctx.stroke()
                        ctx.restore()
                    }
                }
                Text {
                    objectName: "m20MissingSpectrumNotice"
                    visible: !root.hasSpectrum
                    anchors.centerIn: spectrumCanvas
                    text: root.lifecycle === "STALE"
                        ? "SPECTRUM STALE\nREANALYSIS REQUIRED"
                        : "SPECTRUM UNAVAILABLE\nAUTHENTICATED PCM REQUIRED"
                    color: root.amber
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                }
                ToolTip.text: "Static mean power spectral density from the selected real PCM. Logarithmic frequency axis, display-only 72 dB vertical scale relative to its measured maximum. Not a live spectrum or normalized octave-band profile."
                ToolTip.visible: spectrumDwell.ready
                ToolTip.delay: 0
                StudioTooltipDwell {
                    id: spectrumDwell
                    anchors.fill: parent
                }
                // The canvas uses a fixed 20..20000 Hz logarithmic axis.
                // Labels must be placed on those SAME log-frequency grid
                // coordinates, not in five equally sized Row cells.
                Item {
                    anchors.left: spectrumCanvas.left
                    anchors.right: spectrumCanvas.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 7
                    height: 16
                    Repeater {
                        model: [
                            {label:"20 Hz",hz:20},
                            {label:"100 Hz",hz:100},
                            {label:"1 kHz",hz:1000},
                            {label:"10 kHz",hz:10000},
                            {label:"20 kHz",hz:20000}
                        ]
                        delegate: Text {
                            objectName: "m20FrequencyTick_" + index
                            width: 56
                            height: parent.height
                            x: Math.max(0,Math.min(parent.width-width,
                                parent.width*Math.log10(modelData.hz/20)
                                /Math.log10(20000/20)-width/2))
                            text: modelData.label
                            color: root.dim
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            horizontalAlignment: index===0 ? Text.AlignLeft
                                : index===4 ? Text.AlignRight
                                : Text.AlignHCenter
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width
                height: 90
                radius: 7
                color: root.panel
                border.color: root.edge
                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6
                    Text {
                        text: "STEREO IMAGE  /  CORRELATION"
                        color: root.ink
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Row {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: [{key:"width",title:"WIDTH",
                                     tip:"Stereo side-to-mid energy ratio over the real M19 retained loudness-gated union; can legitimately exceed 1."},
                                    {key:"P10",title:"P10",
                                     tip:"10th percentile of 400 ms centred local L/R correlations, evaluated every 100 ms."},
                                    {key:"P50",title:"P50",
                                     tip:"Median of authentic Grid100 local stereo correlations, not a full-programme global correlation."},
                                    {key:"P90",title:"P90",
                                     tip:"90th percentile of authentic Grid100 local correlations; undefined or mono data is unavailable."}]
                            delegate: StudioReadOnlyMeasurement {
                                objectName: modelData.key === "width"
                                    ? "m20StereoWidthReadout"
                                    : "m20StereoCorrelation_" + modelData.key
                                width: (parent.width-18)/4
                                height: 50
                                labelText: modelData.title
                                rawText: root.metricText(
                                    root.m20Values[modelData.key],modelData.key)
                                dataAvailable: root.hasNumber(
                                    root.m20Values[modelData.key])
                                ToolTip.text: modelData.tip
                                ToolTip.visible: stereoDwell.ready
                                ToolTip.delay: 0
                                StudioTooltipDwell {
                                    id: stereoDwell
                                    anchors.fill: parent
                                }
                            }
                        }
                    }
                }
            }
            Rectangle {
                objectName: "m20TonalProfilePanel"
                width: parent.width
                height: 60
                radius: 7
                color: root.panel
                border.color: root.edge
                ToolTip.text: "Genuine measured 1/12-octave linear band energies from the same M20 PCM and scope as the static spectrum above. Relative 48 dB display scale; no independent FFT, normalized fractions or reference-matching score."
                ToolTip.visible: profileDwell.ready
                ToolTip.delay: 0
                StudioTooltipDwell {
                    id: profileDwell
                    anchors.fill: parent
                }
                Text {
                    text: "TONAL PROFILE  /  1/12-OCTAVE BAND ENERGY"
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 10
                    color: root.ink
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    font.bold: true
                }
                // The real M20 engine already computed the band energies.
                // Qt only draws their relative heights; no signal-analysis
                // algorithm or unsupported normalized shape is introduced.
                Canvas {
                    id: bandEnergyCanvas
                    objectName: "m20MeasuredBandEnergy"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 27
                    anchors.bottomMargin: 7
                    visible: root.hasBandEnergy
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    Connections {
                        target: root
                        function onBandEnergyPointsChanged() {
                            bandEnergyCanvas.requestPaint()
                        }
                    }
                    onPaint: {
                        const ctx=getContext("2d")
                        ctx.reset()
                        const points=root.bandEnergyPoints
                        if(!root.hasBandEnergy || width<=0 || height<=0)return
                        let maxDb=-Infinity
                        for(let i=0;i<points.length;++i) {
                            const p=points[i]
                            if(p.hz<20 || p.hz>20000 ||
                               p.linearEnergy<=0)continue
                            const db=10*Math.log10(p.linearEnergy)
                            if(Number.isFinite(db))maxDb=Math.max(maxDb,db)
                        }
                        if(!Number.isFinite(maxDb))return
                        const span=48
                        const frequencySpan=Math.log10(20000/20)
                        ctx.save()
                        ctx.strokeStyle=root.cyan
                        ctx.lineCap="round"
                        ctx.lineWidth=Math.max(1.2,
                            Math.min(5.0,width/points.length*0.48))
                        for(let i=0;i<points.length;++i) {
                            const p=points[i]
                            if(p.hz<20 || p.hz>20000 ||
                               p.linearEnergy<=0)continue
                            const db=10*Math.log10(p.linearEnergy)
                            if(!Number.isFinite(db))continue
                            const x=Math.log10(p.hz/20)/frequencySpan*width
                            const normalized=Math.max(0,Math.min(1,
                                (db-(maxDb-span))/span))
                            ctx.beginPath()
                            ctx.moveTo(x,height)
                            ctx.lineTo(x,height*(1-normalized))
                            ctx.stroke()
                        }
                        ctx.restore()
                    }
                }
                Text {
                    objectName: "m20MissingProfileNotice"
                    visible: !root.hasBandEnergy
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 9
                    text: root.lifecycle === "STALE"
                        ? "BAND ENERGY STALE · REANALYZE"
                        : "NO MEASURED BAND ENERGY"
                    color: root.dim
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                }
            }
        }

        // M21 approved nine-metric table: the six display columns are
        // metric / PREPARED / PROCESSED / REFERENCE / two deltas.
        // M21 occupies its normal height at minimum. Rows grow continuously
        // to 36 px; any additional space goes ABOVE and BELOW the complete
        // table, keeping its vertical center fixed. Text remains unscaled.
        Item {
            id: m21Viewport
            objectName: "m21CenteredViewport"
            visible: root.panelIndex === 2
            width: parent.width
            height: root.height
            Column {
                id: m21
                objectName: "m21ComparisonPage"
                width: parent.width
                y: Math.max(0, (m21Viewport.height - m21.height) / 2)
                spacing: 6
                Rectangle {
                    width: parent.width
                    height: 32
                    color: "#1B3545"
                    border.color: root.edge
                    radius: 5
                    Row {
                        anchors.fill: parent
                        spacing: root.m21ColumnGap
                        Repeater {
                            model: ["METRIC","PREPARED","PROCESSED","REFERENCE",
                                    "Δ PREPARED → REFERENCE",
                                    "Δ PROCESSED → REFERENCE"]
                            delegate: Text {
                                objectName: "m21Header_" + index
                                width: root.m21ColumnWidth(index,parent.width)
                                height: parent.height
                                text: modelData
                                color: index === 3 ? root.copper : root.ink
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                                font.bold: true
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
                Text {
                    objectName: "m21MissingComparisonNotice"
                    width: parent.width
                    height: 22
                    text: root.anyRowValue()
                        ? "CURRENT MEASUREMENTS · MISSING CELLS SHOWN AS —"
                        : root.lifecycle === "STALE"
                        ? "METRICS STALE · REANALYZE ALL SELECTED SOURCES"
                        : "METRICS UNAVAILABLE · SELECT AND ANALYZE SOURCES"
                    color: root.amber
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                // Group rows without a 6px ColumnLayout gap between each
                // metric; all nine readings remain visible at 1184x688.
                Column {
                    id: m21MetricRows
                    objectName: "m21FullNineMetricRows"
                    width: m21.width
                    spacing: 0
                    Repeater {
                        model: ["Integrated LUFS","RMS Level","Sample Peak",
                                "True Peak · M16","Crest Factor","Stereo Width",
                                "Correlation P10","Correlation P50","Correlation P90"]
                        delegate: Rectangle {
                            id: m21MetricRow
                            objectName: "m21MetricRow_" + index
                            readonly property int metricIndex: index
                            readonly property string metricName: modelData
                            readonly property var observed: root.m21Rows.length > metricIndex
                                ? root.m21Rows[metricIndex] : ({})
                            width: m21.width
                            height: root.m21RowHeight
                            color: index%2===0 ? "#14232F" : "#0E1C26"
                            Row {
                                anchors.fill: parent
                                spacing: root.m21ColumnGap
                                Repeater {
                                    model: [m21MetricRow.metricName,
                                        root.metricText(m21MetricRow.observed.prepared,
                                                        m21MetricRow.observed.key),
                                        root.metricText(m21MetricRow.observed.processed,
                                                        m21MetricRow.observed.key),
                                        root.metricText(m21MetricRow.observed.reference,
                                                        m21MetricRow.observed.key),
                                        root.deltaText(m21MetricRow.observed.deltaPrepared,
                                                        m21MetricRow.observed.key),
                                        root.deltaText(m21MetricRow.observed.deltaProcessed,
                                                        m21MetricRow.observed.key)]
                                    delegate: Item {
                                        id: m21ValueCell
                                        objectName: "m21ValueCell_"
                                            + m21MetricRow.metricIndex + "_" + index
                                        width: root.m21ColumnWidth(index,parent.width)
                                        height: parent.height
                                        readonly property bool isDelta: index===4 || index===5
                                        readonly property var deltaValue: index===4
                                            ? m21MetricRow.observed.deltaPrepared
                                            : index===5
                                            ? m21MetricRow.observed.deltaProcessed
                                            : undefined
                                        readonly property real displayLimit:
                                            root.deltaDisplayLimit(m21MetricRow.observed.key)
                                        readonly property bool hasDelta: isDelta
                                            && root.hasNumber(deltaValue)
                                            && Number.isFinite(displayLimit)
                                        readonly property real barWidth: Math.min(
                                            44,Math.max(24,width*0.28))
                                        readonly property real numericWidth: Math.min(
                                            64,Math.max(48,width*0.41))
                                        readonly property real deltaGroupGap: 8
                                        // The MIDPOINT between the numeric-centre
                                        // and bar-centre is exactly the CELL centre.
                                        // Both positions are shared by all 9 rows.
                                        readonly property real deltaGroupLeft:
                                            (width - numericWidth - deltaGroupGap
                                             - barWidth)/2
                                            - (numericWidth - barWidth)/4
                                        Text {
                                            objectName: "m21NumericCell_"
                                                + m21MetricRow.metricIndex + "_" + index
                                            x: m21ValueCell.hasDelta
                                                ? m21ValueCell.deltaGroupLeft : 0
                                            y: 0
                                            height: parent.height
                                            width: m21ValueCell.hasDelta
                                                ? m21ValueCell.numericWidth
                                                : parent.width
                                            text: modelData
                                            font.family: index===0 ? "Segoe UI" : "Consolas"
                                            font.pixelSize: 10
                                            font.bold: index===0
                                            color: index===3 ? root.copper
                                                : index===0 ? root.ink : root.dim
                                            horizontalAlignment: index===0
                                                ? Text.AlignLeft : Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: index===0 ? 7 : 0
                                            elide: Text.ElideRight
                                        }
                                        // Only actual finite Product-owned delta values
                                        // may render a bar. No substitutes or scores.
                                        Canvas {
                                            id: divergenceBar
                                            objectName: "m21DeltaBar_"
                                                + m21MetricRow.metricIndex + "_" + index
                                            visible: m21ValueCell.hasDelta
                                            x: m21ValueCell.deltaGroupLeft
                                                + m21ValueCell.numericWidth
                                                + m21ValueCell.deltaGroupGap
                                            y: (parent.height - height)/2
                                            width: m21ValueCell.barWidth
                                            height: 8
                                            readonly property var deltaValue:
                                                m21ValueCell.deltaValue
                                            readonly property real displayLimit:
                                                m21ValueCell.displayLimit
                                            onWidthChanged: requestPaint()
                                            onDeltaValueChanged: requestPaint()
                                            onDisplayLimitChanged: requestPaint()
                                            onVisibleChanged: if (visible) requestPaint()
                                            onPaint: {
                                                const ctx=getContext("2d")
                                                ctx.reset()
                                                if(!root.hasNumber(deltaValue)
                                                    || !Number.isFinite(displayLimit)
                                                    || displayLimit<=0) return
                                                const mid=width/2
                                                const half=Math.max(0,mid-2)
                                                const ratio=Math.min(1,
                                                    Math.abs(deltaValue)/displayLimit)
                                                const span=half*ratio
                                                const y=2
                                                ctx.fillStyle="#36556B"
                                                ctx.fillRect(1,y,width-2,4)
                                                if(span>0) {
                                                    const gradient=deltaValue<0
                                                        ? ctx.createLinearGradient(
                                                            mid-half,0,mid,0)
                                                        : ctx.createLinearGradient(
                                                            mid,0,mid+half,0)
                                                    if(deltaValue<0) {
                                                        gradient.addColorStop(0,"#D96959")
                                                        gradient.addColorStop(0.5,"#E7BA50")
                                                        gradient.addColorStop(1,"#41C69B")
                                                    } else {
                                                        gradient.addColorStop(0,"#41C69B")
                                                        gradient.addColorStop(0.5,"#E7BA50")
                                                        gradient.addColorStop(1,"#D96959")
                                                    }
                                                    ctx.fillStyle=gradient
                                                    ctx.fillRect(deltaValue<0
                                                        ? mid-span : mid,y,span,4)
                                                }
                                                ctx.fillStyle="#A8D5CB"
                                                ctx.fillRect(mid-0.5,0,1,8)
                                            }
                                            ToolTip.text: "Δ = REFERENCE − CURRENT: "
                                                + modelData + " "
                                                + root.deltaDisplayUnit(
                                                    m21MetricRow.observed.key)
                                                + ". Display limit ±"
                                                + m21ValueCell.displayLimit + " "
                                                + root.deltaDisplayUnit(
                                                    m21MetricRow.observed.key)
                                                + (Math.abs(m21ValueCell.deltaValue)
                                                    >m21ValueCell.displayLimit
                                                    ? " · SCALE SATURATED" : "")
                                                + ". Color encodes deviation magnitude, NOT audio quality."
                                            ToolTip.visible: deltaDwell.ready
                                            ToolTip.delay: 0
                                            StudioTooltipDwell {
                                                id: deltaDwell
                                                anchors.fill: parent
                                            }
                                        }
                                    }
                                }
                            }
                            ToolTip.text: m21MetricRow.metricName + " · "
                                + root.metricExplanation(m21MetricRow.observed.key)
                                + " PREPARED, independent REFERENCE and PROCESSED "
                                + "are separate real measurements. Delta columns "
                                + "are REFERENCE minus PREPARED or PROCESSED, "
                                + "calculated from unrounded operands. "
                                + "Signed zero-centered bars use per-metric "
                                + "visual scales, not audio-quality scores. "
                                + "Saturation never changes the numeric delta. "
                                + "Missing, stale or incompatible results show —."
                            ToolTip.visible: m21RowDwell.ready
                            ToolTip.delay: 0
                            StudioTooltipDwell {
                                id: m21RowDwell
                                anchors.fill: parent
                            }
                        }
                    }
                }
            }
        }

    }
}
