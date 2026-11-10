import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Approved M19/M20/M21 visual review, PR #91 Qt Windows #13.
// Real Product owns one permanent instance per panel. No DEMO, fake DSP
// output, stale numeric residue, or data publication without Phase-6 owner.
Flickable {
    id: root
    objectName: "approvedAnalysisProductContent"
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
    readonly property bool hasSpectrum: spectrumPoints.length > 1
    function hasNumber(value) {
        return typeof value === "number" && Number.isFinite(value)
    }
    function metricText(value, key) {
        if(!hasNumber(value)) return "—"
        return Number(value).toFixed(
            key === "width" || key === "P10"
            || key === "P50" || key === "P90" ? 3 : 2)
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
    boundsBehavior: Flickable.StopAtBounds
    contentWidth: width
    contentHeight: stack.implicitHeight
    ScrollBar.vertical: ScrollBar {
        policy: root.contentHeight > root.height
            ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
    }
    Column {
        id: stack
        width: Math.max(0,root.width-5)
        spacing: 8

        // M19 approved InputGain-style heroes. Reflow to 3 columns at the
        // genuine ~730px DSP host. Never shrink 36px to fake 1440 geometry.
        Column {
            id: m19
            objectName: "m19LevelsPage"
            visible: root.panelIndex === 0
            width: parent.width
            spacing: 8
            Grid {
                id: levelGrid
                width: parent.width
                columns: width >= 1040 ? 5 : (width >= 460 ? 3 : 2)
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
                        height: 148
                        radius: 7
                        color: root.raised
                        border.color: root.edge
                        Column {
                            anchors.fill: parent
                            anchors.margins: 9
                            spacing: 6
                            StudioReadOnlyMeasurement {
                                objectName: "m19MetricValue_" + modelData.key
                                width: parent.width
                                height: implicitHeight
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
                            Text {
                                width: parent.width
                                text: modelData.hint
                                color: modelData.key === "truePeak"
                                    ? root.amber : root.dim
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
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
                width: parent.width
                height: 135
                color: root.panel
                border.color: root.edge
                radius: 7
                Column {
                    anchors.fill: parent
                    anchors.margins: 11
                    spacing: 6
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
                            {name:"REFERENCE",tone:"#D19562"},
                            {name:"PROCESSED",tone:"#7CDDC8"}
                        ]
                        delegate: Row {
                            width: parent.width
                            height: 23
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
            Text {
                width: parent.width
                text: "TRUE PEAK requires the independent M16 authority. " +
                      "No reference bars or values may be invented from sample peak."
                color: root.dim
                font.family: "Segoe UI"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }

        // M20 approved static spectral PSD, 1/12-octave profile, and
        // read-only width/P10/P50/P90. The graph draws AXES only.
        Column {
            id: m20
            objectName: "m20TonalStereoPage"
            visible: root.panelIndex === 1
            width: parent.width
            spacing: 8
            Rectangle {
                width: parent.width
                height: 211
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
                        // NO synthesized spectrum. Only a qualified C++
                        // publication may eventually populate a curve.
                    }
                }
                Text {
                    objectName: "m20MissingSpectrumNotice"
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
                Row {
                    anchors.left: spectrumCanvas.left
                    anchors.right: spectrumCanvas.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 7
                    Repeater {
                        model: ["20 Hz","100 Hz","1 kHz","10 kHz","20 kHz"]
                        delegate: Text {
                            width: parent.width/5
                            text: modelData
                            color: root.dim
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width
                height: 112
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
                            model: [{key:"width",title:"WIDTH"},
                                    {key:"P10",title:"P10"},
                                    {key:"P50",title:"P50"},
                                    {key:"P90",title:"P90"}]
                            delegate: StudioReadOnlyMeasurement {
                                objectName: modelData.key === "width"
                                    ? "m20StereoWidthReadout"
                                    : "m20StereoCorrelation_" + modelData.key
                                width: (parent.width-18)/4
                                height: 50
                                labelText: modelData.title
                                rawText: ""
                                dataAvailable: false
                            }
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width
                height: 78
                radius: 7
                color: root.panel
                border.color: root.edge
                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6
                    Text {
                        text: "TONAL PROFILE  /  1/12-OCTAVE BANDS"
                        color: root.ink
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Text {
                        objectName: "m20MissingProfileNotice"
                        text: root.lifecycle === "STALE"
                            ? "TONAL PROFILE STALE"
                            : "TONAL PROFILE UNAVAILABLE · SUPPORTED BANDS ONLY"
                        color: root.amber
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                    }
                }
            }
        }

        // M21 approved nine-metric table: the six display columns are
        // metric / PREPARED / REFERENCE / PROCESSED / two deltas.
        Column {
            id: m21
            objectName: "m21ComparisonPage"
            visible: root.panelIndex === 2
            width: parent.width
            spacing: 6
            Rectangle {
                width: parent.width
                height: 42
                radius: 6
                color: "#38291D"
                border.color: "#886946"
                Text {
                    anchors.fill: parent
                    anchors.margins: 8
                    text: "INDEPENDENT REFERENCE SCOPE  ·  Δ = REFERENCE − CURRENT"
                    color: "#E7C99D"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                width: parent.width
                height: 48
                color: "#1B3545"
                border.color: root.edge
                radius: 5
                Row {
                    anchors.fill: parent
                    Repeater {
                        model: ["METRIC","PREPARED","REFERENCE","PROCESSED",
                                "Δ PREP → REF","Δ PROC → REF"]
                        delegate: Text {
                            width: parent.width/6
                            height: parent.height
                            text: modelData
                            color: index === 2 ? root.copper : root.ink
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
                height: 37
                text: root.lifecycle === "STALE"
                    ? "METRICS STALE · REANALYZE ALL SELECTED SOURCES"
                    : "METRICS UNAVAILABLE · NO AUTHENTICATED PRODUCT DATA"
                color: root.amber
                font.family: "Segoe UI"
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            Repeater {
                model: ["Integrated LUFS","RMS Level","Sample Peak",
                        "True Peak · M16","Crest Factor","Stereo Width",
                        "Correlation P10","Correlation P50","Correlation P90"]
                delegate: Rectangle {
                    objectName: "m21MetricRow_" + index
                    width: m21.width
                    height: 28
                    color: index%2===0 ? "#14232F" : "#0E1C26"
                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: [modelData,"—","—","—","—","—"]
                            delegate: Text {
                                width: parent.width/6
                                height: parent.height
                                text: modelData
                                font.family: index===0 ? "Segoe UI" : "Consolas"
                                font.pixelSize: 10
                                font.bold: index===0
                                color: index===2 ? root.copper
                                    : index===0 ? root.ink : root.dim
                                horizontalAlignment: index===0
                                    ? Text.AlignLeft : Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: index===0 ? 7 : 0
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
            Text {
                width: parent.width
                text: "CURRENT / STALE / MISSING · Every column needs its own " +
                      "validated generation, realization, scope and owner."
                color: root.dim
                font.family: "Segoe UI"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            width: parent.width
            height: 53
            color: root.panel
            border.color: root.edge
            radius: 6
            Column {
                anchors.centerIn: parent
                spacing: 5
                Text {
                    objectName: "analysisUnavailableTitle"
                    text: root.lifecycle === "STALE"
                        ? "STALE · REANALYZE"
                        : "VERIFIED ANALYSIS UNAVAILABLE"
                    color: root.amber
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.bold: true
                }
                Text {
                    objectName: "analysisUnavailableExplanation"
                    text: "Awaiting canonical Product/Phase-6 numerical publication"
                    color: root.dim
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                }
            }
        }
    }
}
