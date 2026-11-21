import QtQuick

// M20 ISOLATED PREVIEW ONLY. NO registration in app Main.qml or UI module.
// All audio feature math and availability are supplied by the C++ presenter.
Item {
    id: root
    width: 980
    height: 598
    property var presentation: ({ready: false})
    readonly property bool modelReady: !!(presentation && presentation.ready === true)
    readonly property color ice: "#75CDE3"
    readonly property color gold: "#E2B56F"
    Rectangle {
        anchors.fill: parent
        radius: 9
        color: "#091925"
        border.color: "#294459"
    }
    Text {
        id: heading
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 20
        text: "BASIC TONAL + STEREO ANALYZER"
        color: "#E6F4FB"
        font.family: "Segoe UI"
        font.pixelSize: 18
        font.bold: true
    }
    Text {
        anchors.top: parent.top
        anchors.topMargin: 23
        anchors.right: parent.right
        anchors.rightMargin: 18
        text: "M20 · ISOLATED PREVIEW · NOT A RELEASE"
        color: root.gold
        font.pixelSize: 10
    }
    Text {
        id: support
        objectName: "m20SupportDisclosure"
        anchors.left: heading.left
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.top: heading.bottom
        anchors.topMargin: 12
        color: root.modelReady ? "#9DB7C6" : root.gold
        font.pixelSize: 12
        elide: Text.ElideRight
        text: root.modelReady ? (root.presentation.supportText || "")
                              : "No qualified M20 view supplied"
    }
    Rectangle {
        objectName: "m20NoDataPanel"
        visible: !root.modelReady
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: support.bottom
        anchors.margins: 20
        color: "#10273A"
        radius: 7
        Text {
            anchors.centerIn: parent
            text: "INSUFFICIENT / UNQUALIFIED CONTENT · NO FABRICATED ZERO"
            font.pixelSize: 15
            color: "#98B3C3"
        }
    }
    Item {
        id: content
        objectName: "m20DataPanel"
        visible: root.modelReady
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: support.bottom
        anchors.margins: 17
        anchors.topMargin: 14
        Row {
            anchors.fill: parent
            spacing: 14
            Rectangle {
                id: tonal
                objectName: "m20StaticTonalPanel"
                width: Math.floor((parent.width-parent.spacing)*0.62)
                height: parent.height
                color: "#10273A"
                radius: 7
                border.color: "#294459"
                Text {
                    id: staticHeading
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 14
                    text: "AVERAGE SPECTRUM · STATIC (NOT LIVE B3)"
                    color: root.ice
                    font.bold: true
                    font.pixelSize: 12
                }
                Canvas {
                    id: spectrum
                    objectName: "m20StaticSpectrumCanvas"
                    visible: root.modelReady && root.presentation.hasSpectrum === true
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 17
                    anchors.rightMargin: 17
                    anchors.top: staticHeading.bottom
                    anchors.topMargin: 15
                    height: Math.floor(tonal.height*0.34)
                    onPaint: {
                        const ctx=getContext("2d")
                        ctx.reset()
                        const pts=root.presentation.spectrumPoints || []
                        ctx.beginPath()
                        ctx.strokeStyle=root.ice
                        ctx.lineWidth=1.6
                        let active=false
                        for (let i=0; i<pts.length; ++i) {
                            const p=pts[i]
                            if (!p.valid) {active=false;continue}
                            const x=p.x*width
                            const y=(1-p.y)*height
                            if (!active) ctx.moveTo(x,y)
                            else ctx.lineTo(x,y)
                            active=true
                        }
                        ctx.stroke()
                    }
                }
                Text {
                    id: profileHeading
                    anchors.top: spectrum.bottom
                    anchors.topMargin: 25
                    anchors.left: staticHeading.left
                    text: "NORMALIZED 1/" + (root.modelReady
                           ? root.presentation.denominator : "12")
                          + " OCTAVE · STATIC"
                    color: root.gold
                    font.bold: true
                    font.pixelSize: 12
                }
                Canvas {
                    id: bands
                    objectName: "m20FractionalBandsCanvas"
                    visible: root.modelReady && root.presentation.hasProfile === true
                    anchors.left: spectrum.left
                    anchors.right: spectrum.right
                    anchors.top: profileHeading.bottom
                    anchors.topMargin: 12
                    height: Math.floor(tonal.height*0.27)
                    onPaint: {
                        const ctx=getContext("2d")
                        ctx.reset()
                        const list=root.presentation.profileBands || []
                        ctx.fillStyle=root.gold
                        for (let i=0; i<list.length; ++i) {
                            const p=list[i]
                            if (!p.valid) continue
                            const barWidth=Math.max(2,width/Math.max(1,list.length)*0.62)
                            ctx.fillRect(p.x*width,(1-p.y)*height,barWidth,p.y*height)
                        }
                    }
                }
                Text {
                    anchors.left: bands.left
                    anchors.right: bands.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 15
                    text: "Measured zero is not missing support. LOW_SUPPORT and "
                        + "comparison eligibility belong to the canonical owner."
                    wrapMode: Text.WordWrap
                    font.pixelSize: 10
                    color: "#839FB1"
                }
                Connections {
                    target: root
                    function onPresentationChanged() {
                        spectrum.requestPaint()
                        bands.requestPaint()
                    }
                }
            }
            Rectangle {
                id: stereo
                objectName: "m20StaticStereoPanel"
                width: parent.width-tonal.width-parent.spacing
                height: parent.height
                color: "#10273A"
                border.color: "#294459"
                radius: 7
                Text {
                    id: stereoHeading
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 15
                    text: "BROADBAND STEREO"
                    color: root.ice
                    font.bold: true
                    font.pixelSize: 12
                }
                Text {
                    id: widthTitle
                    anchors.top: stereoHeading.bottom
                    anchors.topMargin: 24
                    anchors.left: stereoHeading.left
                    text: "WIDTH RATIO"
                    color: "#9DB7C6"
                    font.pixelSize: 12
                }
                Text {
                    id: widthReadout
                    objectName: "m20WidthReadout"
                    anchors.top: widthTitle.bottom
                    anchors.topMargin: 7
                    anchors.left: widthTitle.left
                    text: root.modelReady ? root.presentation.widthText : "—"
                    color: root.modelReady && root.presentation.hasWidth
                           ? root.gold : "#899CA9"
                    font.family: "Consolas"
                    font.bold: true
                    font.pixelSize: 27
                }
                Text {
                    id: corrHeading
                    anchors.top: widthReadout.bottom
                    anchors.topMargin: 30
                    anchors.left: widthTitle.left
                    text: "LOCAL CORRELATION"
                    color: root.ice
                    font.bold: true
                    font.pixelSize: 12
                }
                Text {
                    id: percentileReadout
                    objectName: "m20CorrelationPercentiles"
                    anchors.top: corrHeading.bottom
                    anchors.topMargin: 12
                    anchors.left: corrHeading.left
                    font.family: "Consolas"
                    font.pixelSize: 12
                    color: "#DCEAF2"
                    text: root.modelReady
                          ? "P10 " + root.presentation.p10Text
                            + " · P50 " + root.presentation.p50Text
                            + " · P90 " + root.presentation.p90Text
                          : "P10 — · P50 — · P90 —"
                }
                Canvas {
                    id: correlation
                    objectName: "m20CorrelationRecentCanvas"
                    visible: root.modelReady
                             && root.presentation.hasCorrelation === true
                    anchors.left: percentileReadout.left
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.top: percentileReadout.bottom
                    anchors.topMargin: 17
                    height: Math.floor(stereo.height*0.30)
                    onPaint: {
                        const ctx=getContext("2d")
                        ctx.reset()
                        const pts=root.presentation.correlationHistory || []
                        ctx.strokeStyle=root.ice
                        ctx.lineWidth=1.7
                        ctx.beginPath()
                        let active=false
                        for (let i=0;i<pts.length;++i) {
                            const p=pts[i]
                            if (!p.valid) {active=false;continue}
                            const x=p.x*width
                            const y=(1-p.y)*height
                            if (!active) ctx.moveTo(x,y)
                            else ctx.lineTo(x,y)
                            active=true
                        }
                        ctx.stroke()
                    }
                }
                Text {
                    anchors.left: corrHeading.left
                    anchors.right: parent.right
                    anchors.rightMargin: 15
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 15
                    text: "Static 5-second tail of complete Grid100 windows. "
                         + "Missing windows remain gaps; no stereo repair."
                    color: "#839FB1"
                    wrapMode: Text.WordWrap
                    font.pixelSize: 10
                }
                Connections {
                    target: root
                    function onPresentationChanged() {
                        correlation.requestPaint()
                    }
                }
            }
        }
    }
}
