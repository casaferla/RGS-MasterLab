import QtQuick 2.15

// ISOLATED M19-M21 VISUAL REVIEW. Do not register in Main.qml.
// Only typed C++ presenter/fixture may supply numbers; never infer
// availability, substitute 0 for missing, or compute audio data in QML.
Item {
    id: root
    width: 1360
    height: 820
    property int selectedPage: 0
    property var presentation: ({ ready: false })
    readonly property bool ready: !!(presentation && presentation.ready === true)
    readonly property color bg: "#071117"
    readonly property color panel: "#0F1D28"
    readonly property color raised: "#142634"
    readonly property color outline: "#2A3947"
    readonly property color cyan: "#00C8FF"
    readonly property color copper: "#D7A16B"
    readonly property color pale: "#E6EEF0"
    readonly property color muted: "#91A5B3"
    readonly property color yellow: "#F2B632"
    readonly property color rose: "#F27683"
    property int pagePadding: 22
    function shown(v) { return v === undefined || v === null || String(v)===""
                                   ? "—" : String(v) }
    function safeList(v) { return v === undefined || v === null ? [] : v }
    function statusColor(s) {
        return String(s)==="CURRENT" ? cyan
            : String(s)==="MISSING" || String(s)==="UNAVAILABLE" ? muted
            : yellow
    }

    Rectangle { anchors.fill: parent; color: root.bg }
    Rectangle {
        x: 12; y: 12; width: parent.width-24; height: 84
        color: root.panel; radius: 9; border.color: root.outline
        Text {
            x: 19; y: 15
            text: "ANALYSIS WORKSPACE"
            color: root.pale; font.family: "Segoe UI"
            font.pixelSize: 23; font.bold: true
        }
        Text {
            x: 20; y: 54
            text: "Reference-aware, offline analysis  ·  " +
                  (root.ready ? root.shown(root.presentation.scopeLabel) : "NO VALID SCOPE")
            color: root.muted; font.pixelSize: 12
            font.family: "Segoe UI"
        }
        Rectangle {
            width: 268; height: 29
            x: parent.width-width-19; y: 18
            radius: 5; color: "#392818"; border.color: root.copper
            Text {
                anchors.centerIn: parent; text: "PO UI REVIEW  ·  DEMO DATA"
                color: root.copper; font.pixelSize: 11
                font.bold: true; font.family: "Segoe UI"
            }
        }
        Text {
            x: parent.width-width-19; y: 55
            text: "Not connected to Product · no Gate/FQ claim"
            color: root.muted; font.pixelSize: 10
            font.family: "Segoe UI"
        }
    }

    Row {
        id: tabs
        objectName: "analysisTabs"
        x: 12; y: 108; spacing: 8
        Repeater {
            model: ["M19  ·  LEVELS", "M20  ·  TONAL + STEREO",
                    "M21  ·  METRICS / DELTA"]
            delegate: Rectangle {
                width: index===1 ? 251 : 221
                height: 43
                radius: 7
                color: root.selectedPage===index ? root.raised : root.panel
                border.color: root.selectedPage===index ? root.cyan : root.outline
                border.width: root.selectedPage===index ? 2 : 1
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: root.selectedPage===index ? root.pale : root.muted
                    font.family: "Segoe UI"; font.pixelSize: 13
                    font.bold: root.selectedPage===index
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.selectedPage=index
                }
            }
        }
    }

    Rectangle {
        id: statusBar
        x: 12; y: 161; width: root.width-24; height: 55
        radius: 7; color: root.panel; border.color: root.outline
        Row {
            x: 15; y: 9; spacing: 28
            Repeater {
                model: [
                    {name:"PREPARED",state:root.ready ? root.shown(root.presentation.preparedState) : "MISSING"},
                    {name:"GOLD",state:root.ready ? root.shown(root.presentation.goldState) : "MISSING"},
                    {name:"PROCESSED",state:root.ready ? root.shown(root.presentation.processedState) : "MISSING"}
                ]
                delegate: Item {
                    width: 204; height: 35
                    Rectangle {
                        width: 4; height: 28; x: 0; y: 3
                        color: root.statusColor(modelData.state); radius: 2
                    }
                    Text {
                        x: 13; y: 1; text: modelData.name
                        font.family: "Segoe UI"; font.pixelSize: 10
                        font.bold: true; color: root.muted
                    }
                    Text {
                        x: 13; y: 17
                        text: modelData.state
                        font.family: "Segoe UI"; font.pixelSize: 12
                        color: root.statusColor(modelData.state)
                    }
                }
            }
        }
        Text {
            anchors.right: parent.right; anchors.rightMargin: 15
            anchors.verticalCenter: parent.verticalCenter
            color: root.muted; font.pixelSize: 12; font.family: "Segoe UI"
            text: root.ready ? root.shown(root.presentation.rateLabel) : "No data"
        }
    }

    Rectangle {
        id: content
        x: 12; y: 226; width: root.width-24; height: root.height-281
        radius: 8; color: root.panel; border.color: root.outline

        Item {
            objectName: "m19LevelsPage"
            anchors.fill: parent
            visible: root.selectedPage===0
            Text {
                x: 20; y: 17; text: "BASIC LEVEL ANALYZER"
                color: root.pale; font.family: "Segoe UI"
                font.pixelSize: 19; font.bold: true
            }
            Text {
                x: 20; y: 52; text: "Five authoritative measurements · absolute full-file or explicit analysis region"
                color: root.muted; font.family: "Segoe UI"; font.pixelSize: 12
            }
            Row {
                x: 17; y: 91; spacing: 11
                Repeater {
                    model: root.ready ? root.safeList(root.presentation.levels) : []
                    delegate: Rectangle {
                        objectName: "levelMetricCard"
                        width: Math.floor((content.width-45-44)/5)
                        height: 184
                        color: root.raised; radius: 8; border.color: root.outline
                        Rectangle {
                            x: 11; y: 12; width: 29; height: 3
                            color: modelData.status==="CURRENT" ? root.cyan : root.yellow
                        }
                        Text {
                            x: 12; y: 27; width: parent.width-24
                            text: root.shown(modelData.name)
                            color: root.muted; font.family: "Segoe UI"
                            font.pixelSize: 12; font.bold: true
                        }
                        Text {
                            x: 11; y: 69; width: parent.width-22
                            text: root.shown(modelData.value)
                            color: modelData.status==="CURRENT" ? root.pale : root.yellow
                            font.family: "Consolas"; font.pixelSize: modelData.status==="CURRENT" ? 31 : 21
                            font.bold: true; elide: Text.ElideRight
                        }
                        Text {
                            x: 12; y: 140; width: parent.width-24
                            text: root.shown(modelData.status)
                            color: root.statusColor(modelData.status)
                            font.family: "Segoe UI"; font.pixelSize: 11
                        }
                    }
                }
            }
            Rectangle {
                x: 17; y: 302; width: parent.width-34; height: 174
                color: root.raised; radius: 8; border.color: root.outline
                Text {
                    x: 16; y: 15; text: "MEASUREMENT PROVENANCE"
                    font.family: "Segoe UI"; font.bold: true; font.pixelSize: 13
                    color: root.cyan
                }
                Text {
                    x: 16; y: 49; width: parent.width-32
                    font.family: "Segoe UI"; font.pixelSize: 13
                    lineHeight: 1.5; wrapMode: Text.Wrap
                    color: root.pale
                    text: "Integrated LUFS · M19 K-weighting and retained gate.\n" +
                          "RMS / Sample Peak / Crest · exact selected PCM scope.\n" +
                          "True Peak · measured exclusively by M16 (full owning resource).\n" +
                          "Missing, stale, silence and unsupported are never displayed as zero."
                }
            }
        }

        Item {
            objectName: "m20TonalStereoPage"
            anchors.fill: parent
            visible: root.selectedPage===1
            Text {
                x: 19; y: 15; text: "TONAL + STEREO ANALYZER"
                color: root.pale; font.bold: true; font.pixelSize: 19
                font.family: "Segoe UI"
            }
            Text {
                x: 19; y: 48
                text: "Static, scope-bound spectrum · externally qualified tonal support · no live FFT duplication"
                color: root.muted; font.pixelSize: 12; font.family: "Segoe UI"
            }
            Rectangle {
                id: tonal
                x: 16; y: 81; width: Math.floor((parent.width-48)*0.625)
                height: parent.height-96; color: root.raised
                radius: 8; border.color: root.outline
                Text {
                    x: 15; y: 14
                    text: "AVERAGE SPECTRUM   ·   1/12-OCTAVE POWER"
                    font.family: "Segoe UI"; font.pixelSize: 12
                    font.bold: true; color: root.cyan
                }
                Canvas {
                    id: spectrum
                    objectName: "reviewM20Spectrum"
                    x: 20; y: 58; width: parent.width-40; height: 194
                    onPaint: {
                        const ctx=getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle="#304653"
                        ctx.lineWidth=1
                        for(let k=0;k<5;k++){
                            const yy=(k+1)*height/6
                            ctx.beginPath();ctx.moveTo(0,yy)
                            ctx.lineTo(width,yy);ctx.stroke()
                        }
                        const pts=root.ready ? root.safeList(root.presentation.spectrumPoints) : []
                        ctx.strokeStyle="#00C8FF";ctx.lineWidth=2
                        ctx.beginPath()
                        let started=false
                        for(let i=0;i<pts.length;i++){
                            const p=pts[i]
                            if(!p.valid){started=false;continue}
                            const x=Math.max(0,Math.min(1,p.x))*width
                            const y=(1-Math.max(0,Math.min(1,p.y)))*height
                            if(!started){ctx.moveTo(x,y);started=true}
                            else ctx.lineTo(x,y)
                        }
                        ctx.stroke()
                    }
                }
                Text {
                    x: 17; y: 267
                    color: root.copper; font.family: "Segoe UI"
                    font.pixelSize: 12; font.bold: true
                    text: "NORMALIZED TONAL BANDS · SUPPORT REQUIRED"
                }
                Canvas {
                    id: bands
                    objectName: "reviewM20Bands"
                    x: 20; y: 305; width: parent.width-40; height: 145
                    onPaint: {
                        const ctx=getContext("2d");ctx.reset()
                        const b=root.ready ? root.safeList(root.presentation.bands) : []
                        for(let i=0;i<b.length;i++){
                            const bar=b[i]
                            const w=Math.max(3,width/Math.max(1,b.length)*0.68)
                            const xx=bar.x*width
                            const yy=(1-bar.y)*height
                            ctx.fillStyle=bar.eligible===true ? "#D7A16B" : "#516472"
                            ctx.fillRect(xx,yy,w,bar.y*height)
                        }
                    }
                }
            }
            Rectangle {
                x: tonal.x+tonal.width+12; y: 81
                width: content.width-tonal.width-44; height: content.height-96
                color: root.raised; radius: 8; border.color: root.outline
                Text {
                    x: 17; y: 14; text: "BROADBAND STEREO"
                    font.family: "Segoe UI"; font.pixelSize: 12
                    font.bold: true; color: root.cyan
                }
                Text {
                    x: 18; y: 58; text: "WIDTH RATIO"
                    color: root.muted; font.family: "Segoe UI"; font.pixelSize: 12
                }
                Text {
                    x: 18; y: 82
                    text: root.ready ? root.shown(root.presentation.widthText) : "—"
                    color: root.copper; font.family: "Consolas"
                    font.bold: true; font.pixelSize: 36
                }
                Text {
                    x: 18; y: 167; text: "LOCAL CORRELATION · PERCENTILES"
                    color: root.muted; font.family: "Segoe UI"; font.pixelSize: 11
                }
                Text {
                    x: 18; y: 190
                    text: root.ready ? root.shown(root.presentation.correlationText) : "P10 —  ·  P50 —  ·  P90 —"
                    color: root.pale; font.pixelSize: 15
                    font.family: "Consolas"
                }
                Canvas {
                    id: correlation
                    objectName: "reviewM20Correlation"
                    x: 18; y: 250; width: parent.width-36; height: 190
                    onPaint: {
                        const ctx=getContext("2d");ctx.reset()
                        ctx.strokeStyle="#49606D";ctx.lineWidth=1
                        ctx.beginPath();ctx.moveTo(0,height/2)
                        ctx.lineTo(width,height/2);ctx.stroke()
                        ctx.strokeStyle="#00C8FF";ctx.lineWidth=2
                        const pts=root.ready ? root.safeList(root.presentation.correlationPoints):[]
                        ctx.beginPath()
                        let started=false
                        for(let i=0;i<pts.length;i++){
                            const p=pts[i];if(!p.valid){started=false;continue}
                            if(!started){ctx.moveTo(p.x*width,(1-p.y)*height);started=true}
                            else ctx.lineTo(p.x*width,(1-p.y)*height)
                        }
                        ctx.stroke()
                    }
                }
            }
        }

        Item {
            objectName: "m21MetricsPage"
            anchors.fill: parent
            visible: root.selectedPage===2
            Text {
                x: 18; y: 14; text: "METRICS / DELTA  ·  GOLD - CURRENT"
                color: root.pale; font.family: "Segoe UI"
                font.pixelSize: 19; font.bold: true
            }
            Text {
                x: 18; y: 48; width: parent.width-36
                text: "GOLD has an independent analysis scope. Differences do not imply time alignment."
                color: root.copper; font.family: "Segoe UI"; font.pixelSize: 12
            }
            Rectangle {
                id: tableHeader
                x: 16; y: 78; width: parent.width-32; height: 47
                radius: 5; color: "#244052"
                Row {
                    anchors.fill: parent
                    Repeater {
                        model: ["METRIC","PREPARED","GOLD","PROCESSED",
                                "Δ PREPARED→GOLD","Δ PROCESSED→GOLD"]
                        delegate: Text {
                            width: index===0 ? 233 : (tableHeader.width-233)/5
                            height: parent.height
                            text: modelData
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: root.pale; font.family: "Segoe UI"
                            font.pixelSize: 12; font.bold: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
            Column {
                x: tableHeader.x; y: 131
                width: tableHeader.width; spacing: 3
                Repeater {
                    model: root.ready ? root.safeList(root.presentation.rows) : []
                    delegate: Rectangle {
                        height: 38; width: parent.width
                        radius: 3; color: index%2===0 ? "#192E3D" : "#142634"
                        Row {
                            anchors.fill: parent
                            Repeater {
                                model: [modelData.name,modelData.prepared,modelData.gold,
                                        modelData.processed,modelData.preparedDelta,
                                        modelData.processedDelta]
                                delegate: Text {
                                    width: index===0 ? 233 : (tableHeader.width-233)/5
                                    height: parent.height
                                    text: root.shown(modelData)
                                    color: (String(modelData).indexOf("MISSING")>=0
                                            ||String(modelData).indexOf("STALE")>=0)
                                           ? root.yellow : root.pale
                                    font.family: index===0 ? "Segoe UI" : "Consolas"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
            Text {
                x: 18; anchors.bottom: parent.bottom; anchors.bottomMargin: 12
                width: parent.width-36
                text: "Unrounded comparison · unavailable is not 0 · no inferred tonal shape dB · no stale result reuse"
                color: root.muted; font.pixelSize: 11; font.family: "Segoe UI"
            }
        }

        Rectangle {
            objectName: "analysisMissingOverlay"
            anchors.fill: parent; radius: 8
            color: "#0F1D28"; visible: !root.ready
            Text {
                anchors.centerIn: parent
                text: "NO AUTHENTICATED ANALYSIS RESULTS\nMISSING  ·  UNAVAILABLE  ·  STALE  are never 0.0"
                horizontalAlignment: Text.AlignHCenter
                color: root.yellow; font.family: "Segoe UI"
                font.pixelSize: 17; lineHeight: 1.6
            }
        }
        Connections {
            target: root
            function onPresentationChanged() {
                spectrum.requestPaint()
                bands.requestPaint()
                correlation.requestPaint()
            }
            function onSelectedPageChanged() {
                spectrum.requestPaint()
                bands.requestPaint()
                correlation.requestPaint()
            }
        }
    }

    Text {
        x: 15; anchors.bottom: parent.bottom; anchors.bottomMargin: 12
        color: root.muted; font.pixelSize: 11; font.family: "Segoe UI"
        text: "RGS MasterLab  ·  Analysis UX review  ·  isolated QtQuick render, not a release"
    }
}
