// RGS MasterLab — L1-M19/M20/M21 visual proposal, QT SCREENSHOT REVIEW.
// This component is intentionally NOT in Main.qml or the live UI module.
// Numerical results must come from typed C++ presenters, never QML DSP.
// The Qt screenshot harness injects explicitly fictional DEMO DATA.
// No Phase-6 qualification, real GOLD alignment or production publishing.
import QtQuick

Rectangle {
    id: root
    objectName: "analysisWorkbenchPrototype"
    width: 1440
    height: 900
    color: "#071117"

    property int activeTab: 0 // 0=M19, 1=M20, 2=M21
    // Design-only alternate presentation: one module per detached/dockable shell.
    // Does not instantiate a real Window, alter Product, or claim dock wiring.
    property bool standaloneModuleMode: false
    readonly property string selectedModuleTitle: activeTab === 0
        ? "LEVELS  /  M19" : (activeTab === 1
        ? "TONAL & STEREO  /  M20" : "METRICS / DELTA  /  M21")
    property var presentation: ({ready:false})
    property bool fixtureMode: true
    readonly property bool available: !!presentation && presentation.ready === true
    readonly property color ink: "#E6EEF0"
    readonly property color dim: "#92A5B2"
    readonly property color cyan: "#00C8FF"
    readonly property color copper: "#D19562"
    readonly property color amber: "#F2B632"
    readonly property color edge: "#2A3947"
    readonly property color panel: "#0F1820"
    readonly property color raised: "#142634"

    function valueOr(value, unavailable) {
        return root.available && value !== undefined && value !== null
            ? String(value) : (unavailable || "UNAVAILABLE")
    }

    Column {
        id: layout
        anchors.fill: parent
        anchors.margins: 18
        spacing: root.standaloneModuleMode ? 8 : 12

        Row {
            width: parent.width
            height: 72
            spacing: 14
            Rectangle {
                width: 5
                height: 53
                anchors.verticalCenter: parent.verticalCenter
                radius: 3
                color: root.cyan
            }
            Column {
                width: parent.width - 330
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text {
                    text: root.standaloneModuleMode ? root.selectedModuleTitle : "ANALYSIS"
                    font.family: "Segoe UI"
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                    color: root.ink
                }
                Text {
                    text: root.standaloneModuleMode
                        ? "RGS MasterLab  /  INDIVIDUAL PANEL DESIGN · DOCK/FLOAT NOT ACTIVE"
                        : "RGS MasterLab  /  READ-ONLY MEASUREMENT & REFERENCE COMPARISON"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    color: root.dim
                }
            }
            Rectangle {
                width: 170
                height: 32
                anchors.verticalCenter: parent.verticalCenter
                radius: 5
                color: root.fixtureMode ? "#3A2C1B" : "#15352F"
                border.color: root.fixtureMode ? root.amber : "#3C9D8B"
                Text {
                    anchors.centerIn: parent
                    text: root.fixtureMode ? "DEMO DATA · DESIGN" : "ACTUAL PRODUCT DATA"
                    font.family: "Segoe UI"
                    font.bold: true
                    font.pixelSize: 10
                    color: root.fixtureMode ? root.amber : "#7CE5C6"
                }
            }
        }

        Row {
            id: nav
            width: parent.width
            visible: !root.standaloneModuleMode
            height: root.standaloneModuleMode ? 0 : 48
            spacing: 8
            Repeater {
                model: [
                    {name:"LEVELS",detail:"M19"},
                    {name:"TONAL & STEREO",detail:"M20"},
                    {name:"METRICS / DELTA",detail:"M21"}
                ]
                delegate: Rectangle {
                    width: (nav.width - 16)/3
                    height: nav.height
                    radius: 6
                    color: root.activeTab===index ? "#173445" : root.panel
                    border.color: root.activeTab===index ? root.cyan : root.edge
                    border.width: root.activeTab===index ? 2 : 1
                    Row {
                        anchors.centerIn: parent
                        spacing: 12
                        Text {
                            text: modelData.name
                            color: root.activeTab===index ? root.ink : root.dim
                            font.family: "Segoe UI"
                            font.pixelSize: 13
                            font.bold: true
                        }
                        Text {
                            text: modelData.detail
                            color: root.activeTab===index ? root.cyan : "#617B8B"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.activeTab = index
                    }
                }
            }
        }

        Rectangle {
            id: scopeStrip
            width: parent.width
            height: 52
            color: root.panel
            border.color: root.edge
            radius: 6
            Row {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10
                Text {
                    text: "ANALYSIS SCOPE"
                    width: 142
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.dim
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.bold: true
                }
                Repeater {
                    model: [
                        {name:"PREPARED",color:"#76CDE9"},
                        {name:"GOLD",color:"#D19562"},
                        {name:"PROCESSED",color:"#7CDDC8"}
                    ]
                    delegate: Rectangle {
                        width: Math.max(112, (scopeStrip.width-430)/3)
                        height: 29
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 4
                        color: modelData.name === "GOLD" ? "#30261E" : "#152937"
                        border.color: modelData.color
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: modelData.name
                            font.family: "Segoe UI"
                            font.bold: true
                            font.pixelSize: 11
                            color: modelData.color
                        }
                    }
                }
                Text {
                    text: "FULL TRACK · 48 kHz · STEREO"
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.dim
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                }
            }
        }

        Item {
            id: workspace
            width: parent.width
            height: Math.max(330,layout.height - 72 - nav.height - 52 - 34 - 4*layout.spacing)

            // M19: Five typed numeric readouts, no invented TP authorization.
            Column {
                id: levels
                objectName: "m19LevelsPage"
                visible: root.activeTab===0
                anchors.fill: parent
                spacing: 12

                Row {
                    id: levelTiles
                    width: parent.width
                    height: Math.min(165,Math.max(125,parent.height*0.30))
                    spacing: 10
                    Repeater {
                        model: [
                            {label:"INTEGRATED LOUDNESS",key:"lufs",unit:"LUFS",hint:"EBU / ABSOLUTE & RELATIVE GATE"},
                            {label:"RMS LEVEL",key:"rms",unit:"dBFS",hint:"FULL SELECTED SCOPE"},
                            {label:"SAMPLE PEAK",key:"samplePeak",unit:"dBFS",hint:"RAW DISCRETE PEAK"},
                            {label:"TRUE PEAK",key:"truePeak",unit:"dBTP",hint:"M16 AUTHORITATIVE RESULT"},
                            {label:"CREST FACTOR",key:"crest",unit:"dB",hint:"PEAK − RMS"}
                        ]
                        delegate: Rectangle {
                            width: (levelTiles.width - 40)/5
                            height: levelTiles.height
                            radius: 7
                            color: root.raised
                            border.color: root.edge
                            Column {
                                anchors.fill: parent
                                anchors.margins: 13
                                spacing: 8
                                Text {
                                    text: modelData.label
                                    font.family: "Segoe UI"
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: root.dim
                                }
                                Row {
                                    spacing: 6
                                    Text {
                                        objectName: "m19MetricValue_" + modelData.key
                                        text: root.valueOr(
                                            root.presentation.levels
                                                ? root.presentation.levels[modelData.key] : null,
                                            "—")
                                        font.family: "Segoe UI"
                                        font.pixelSize: Math.max(21,Math.min(33,levelTiles.width/42))
                                        font.weight: Font.DemiBold
                                        color: modelData.key==="truePeak" ? root.amber : root.ink
                                    }
                                    Text {
                                        text: modelData.unit
                                        anchors.bottom: parent.bottom
                                        color: root.dim
                                        font.family: "Segoe UI"
                                        font.pixelSize: 11
                                    }
                                }
                                Text {
                                    width: parent.width
                                    text: modelData.hint
                                    color: modelData.key==="truePeak" ? root.amber : "#7995A6"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 9
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }

                Row {
                    width: parent.width
                    height: parent.height - levelTiles.height - parent.spacing
                    spacing: 12
                    Rectangle {
                        width: Math.floor((parent.width - 12)*0.65)
                        height: parent.height
                        radius: 7
                        color: root.panel
                        border.color: root.edge
                        Column {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 15
                            Text {
                                text: "LEVELS · REFERENCE RELATION"
                                font.family: "Segoe UI"
                                font.bold: true
                                font.pixelSize: 13
                                color: root.ink
                            }
                            Text {
                                text: "Read-only comparison; GOLD is measured independently."
                                font.family: "Segoe UI"
                                font.pixelSize: 11
                                color: root.dim
                            }
                            Repeater {
                                model: [
                                    {name:"PREPARED",value:0.52,color:"#76CDE9",reading:"−18.4 LUFS"},
                                    {name:"GOLD",value:0.76,color:"#D19562",reading:"−13.8 LUFS"},
                                    {name:"PROCESSED",value:0.73,color:"#7CDDC8",reading:"−14.2 LUFS"}
                                ]
                                delegate: Column {
                                    width: parent.width
                                    spacing: 5
                                    Row {
                                        width: parent.width
                                        Text {text:modelData.name;width:parent.width-125;color:root.dim;font.pixelSize:11;font.bold:true}
                                        Text {text:root.available?modelData.reading:"UNAVAILABLE";color:root.ink;font.pixelSize:12}
                                    }
                                    Rectangle {
                                        width: parent.width
                                        height: 12
                                        radius: 3
                                        color: "#203240"
                                        Rectangle {
                                            width: root.available?parent.width*modelData.value:0
                                            height: parent.height
                                            radius: 3
                                            color: modelData.color
                                        }
                                    }
                                }
                            }
                            Text {
                                text: "Reference values and bars are illustrative in this screenshot only."
                                font.pixelSize: 10
                                color: root.amber
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                    Rectangle {
                        width: parent.width - Math.floor((parent.width-12)*0.65) - 12
                        height: parent.height
                        radius: 7
                        color: root.panel
                        border.color: root.edge
                        Column {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 11
                            Text {text:"MEASUREMENT STATUS";color:root.ink;font.bold:true;font.pixelSize:13}
                            Text {text:"CURRENT · INTEGRATED";color:"#7CDDC8";font.bold:true;font.pixelSize:11}
                            Text {
                                text:"Measured LUFS, RMS, Sample Peak and Crest are available only with valid authenticated PCM."
                                color:root.dim;wrapMode:Text.WordWrap
                                width:parent.width;font.pixelSize:12
                            }
                            Rectangle {width:parent.width;height:1;color:root.edge}
                            Text {text:"TRUE PEAK · M16";color:root.amber;font.bold:true;font.pixelSize:11}
                            Text {
                                text:"Numerical TP is not automatically a release-qualified value. No substitute peak calculation."
                                color:root.dim;wrapMode:Text.WordWrap
                                width:parent.width;font.pixelSize:12
                            }
                        }
                    }
                }
            }

            // M20: the plot is VISUAL only. C++ owns FFT/PSD and all statuses.
            Column {
                id: tonal
                objectName: "m20TonalStereoPage"
                visible: root.activeTab===1
                anchors.fill: parent
                spacing: 12
                Row {
                    id: tonalUpper
                    width: parent.width
                    height: parent.height - 146
                    spacing: 12
                    Rectangle {
                        width: Math.floor((parent.width - 12)*0.68)
                        height: parent.height
                        radius: 7
                        color: root.panel
                        border.color: root.edge
                        Text {
                            id: spectrumTitle
                            text:"AVERAGE SPECTRUM  /  STATIC PSD"
                            anchors.top:parent.top;anchors.left:parent.left
                            anchors.margins:15
                            color:root.ink;font.bold:true;font.pixelSize:13
                        }
                        Text {
                            text:"Hz · LOG VIEW / SPECTRAL SHAPE"
                            anchors.top:parent.top;anchors.right:parent.right
                            anchors.margins:15
                            color:root.dim;font.pixelSize:10
                        }
                        Canvas {
                            objectName: "m20WorkbenchSpectrum"
                            id: spectrumCanvas
                            x:50;y:55
                            width:parent.width-76
                            height:parent.height-92
                            onPaint:{
                                const ctx=getContext("2d")
                                ctx.reset()
                                ctx.strokeStyle="#284459";ctx.lineWidth=1
                                for(let i=0;i<=4;i++){
                                    ctx.beginPath();ctx.moveTo(0,i*height/4)
                                    ctx.lineTo(width,i*height/4);ctx.stroke()
                                }
                                const logTicks=[0.0,0.233,0.566,0.899,1.0]
                                for(let i=0;i<logTicks.length;i++){
                                    const tickX=logTicks[i]*width
                                    ctx.beginPath();ctx.moveTo(tickX,0)
                                    ctx.lineTo(tickX,height);ctx.stroke()
                                }
                                const pts=root.available?root.presentation.spectrum:[]
                                ctx.strokeStyle="#00C8FF";ctx.lineWidth=2.3
                                ctx.beginPath()
                                for(let i=0;i<pts.length;i++){
                                    const p=pts[i]
                                    const x=p.x*width, y=(1-p.y)*height
                                    if(i===0)ctx.moveTo(x,y)
                                    else ctx.lineTo(x,y)
                                }
                                ctx.stroke()
                            }
                        }
                        Repeater {
                            model:[
                                {t:"20 Hz",x:0.0},
                                {t:"100 Hz",x:0.233},
                                {t:"1 kHz",x:0.566},
                                {t:"10 kHz",x:0.899},
                                {t:"20 kHz",x:1.0}
                            ]
                            delegate:Text {
                                width:60
                                x:Math.max(16,Math.min(parent.width-width-16,
                                    spectrumCanvas.x+modelData.x*spectrumCanvas.width-width/2))
                                y:spectrumCanvas.y+spectrumCanvas.height+8
                                text:modelData.t
                                horizontalAlignment:Text.AlignHCenter
                                color:root.dim;font.pixelSize:10
                            }
                        }
                    }
                    Rectangle {
                        width:parent.width-Math.floor((parent.width-12)*0.68)-12
                        height:parent.height
                        radius:7;color:root.panel;border.color:root.edge
                        Column{
                            anchors.fill:parent;anchors.margins:12;spacing:8
                            Text{text:"STEREO IMAGE";color:root.ink;font.bold:true;font.pixelSize:13}
                            Row{
                                spacing:10
                                Text{text:"WIDTH";width:110;color:root.dim;font.pixelSize:11}
                                Text{text:root.valueOr(root.presentation.stereo?root.presentation.stereo.width:null);color:root.cyan;font.pixelSize:24;font.bold:true}
                            }
                            Rectangle{width:parent.width;height:1;color:root.edge}
                            Text{text:"CORRELATION · WINDOWED";color:root.dim;font.pixelSize:11;font.bold:true}
                            Repeater{
                                model:[{k:"P10",c:"#D19562"},{k:"P50",c:"#00C8FF"},{k:"P90",c:"#7CDDC8"}]
                                delegate:Row{
                                    spacing:10
                                    Text{text:modelData.k;width:65;color:root.dim;font.pixelSize:12}
                                    Text{text:root.valueOr(root.presentation.stereo?root.presentation.stereo[modelData.k]:null);color:modelData.c;font.pixelSize:19;font.bold:true}
                                }
                            }
                            Rectangle{width:parent.width;height:1;color:root.edge}
                            Text{
                                width:parent.width
                                text:"Mono: NOT APPLICABLE · Low support: UNAVAILABLE"
                                color:root.amber;font.pixelSize:11
                                wrapMode:Text.WordWrap
                            }
                        }
                    }
                }
                Rectangle{
                    width:parent.width;height:134
                    radius:7;color:root.panel;border.color:root.edge
                    Row{
                        anchors.fill:parent;anchors.margins:14;spacing:15
                        Column{
                            width:180;spacing:7
                            Text{text:"TONAL PROFILE";color:root.ink;font.bold:true;font.pixelSize:13}
                            Text{text:"1/12 OCTAVE BANDS";color:root.dim;font.pixelSize:10}
                            Text{text:"SUPPORTED ONLY";color:root.amber;font.pixelSize:10}
                        }
                        Repeater {
                            model:root.available?(root.presentation.bands||[]):[]
                            delegate:Column{
                                width:Math.max(12,Math.floor((workspace.width-260)/root.presentation.bands.length)-14)
                                height:90
                                spacing:4
                                Rectangle{
                                    width:parent.width;height:62;color:"#102431"
                                    Rectangle{
                                        anchors.bottom:parent.bottom
                                        width:parent.width
                                        height:Math.round(62*modelData.level)
                                        color:modelData.supported ? root.cyan : "#485764"
                                    }
                                }
                                Text{
                                    width:parent.width
                                    horizontalAlignment:Text.AlignHCenter
                                    text:modelData.label
                                    color:modelData.supported?root.dim:root.amber
                                    font.pixelSize:9
                                }
                            }
                        }
                    }
                }
            }

            // M21: nine rows and two Gold-minus-Current columns.
            Column {
                id: comparison
                objectName: "m21ComparisonPage"
                visible:root.activeTab===2
                anchors.fill:parent
                spacing:11
                Rectangle{
                    width:parent.width
                    height:48
                    color:"#38291D";border.color:"#886946";radius:6
                    Row{
                        anchors.fill:parent;anchors.margins:11;spacing:10
                        Text{
                            text:"GOLD SCOPE IS INDEPENDENT"
                            color:root.amber;font.bold:true;font.pixelSize:12
                            anchors.verticalCenter:parent.verticalCenter
                        }
                        Text{
                            text:"No time alignment is assumed. Δ is GOLD − CURRENT; unavailable values remain unavailable."
                            color:"#E7C99D";font.pixelSize:11
                            anchors.verticalCenter:parent.verticalCenter
                        }
                    }
                }
                Rectangle{
                    width:parent.width
                    height:parent.height-59
                    color:root.panel
                    border.color:root.edge
                    radius:7
                    Column{
                        anchors.fill:parent
                        anchors.margins:10
                        spacing:2
                        Rectangle{
                            width:parent.width
                            height:42
                            color:"#1B3545"
                            Row{
                                anchors.fill:parent
                                Repeater{
                                    model:["METRIC","PREPARED","GOLD","PROCESSED","Δ PREPARED → GOLD","Δ PROCESSED → GOLD"]
                                    delegate:Text{
                                        width:parent.width/6
                                        height:parent.height
                                        text:modelData
                                        color:index===2?root.copper:root.ink
                                        font.family:"Segoe UI"
                                        font.bold:true;font.pixelSize:11
                                        horizontalAlignment:Text.AlignHCenter
                                        verticalAlignment:Text.AlignVCenter
                                        wrapMode:Text.WordWrap
                                    }
                                }
                            }
                        }
                        Repeater{
                            model:root.available?(root.presentation.metrics||[]):[]
                            delegate:Rectangle{
                                width:parent.width
                                height:Math.max(23,Math.min(48,Math.floor((parent.height-100)/9)))
                                color:index%2===0?"#14232F":"#0E1C26"
                                Row{
                                    anchors.fill:parent
                                    spacing:0
                                    Repeater{
                                        model:[modelData.name,modelData.prepared,modelData.gold,
                                            modelData.processed,modelData.preparedDelta,modelData.processedDelta]
                                        delegate:Text{
                                            width:parent.width/6
                                            height:parent.height
                                            text:modelData
                                            font.family:"Segoe UI"
                                            font.pixelSize:11
                                            font.bold:index===0
                                            color:String(modelData).indexOf("UNAVAILABLE")>=0?root.amber:
                                                index===2?root.copper:root.ink
                                            horizontalAlignment:index===0?Text.AlignLeft:Text.AlignHCenter
                                            leftPadding:index===0?12:0
                                            verticalAlignment:Text.AlignVCenter
                                            elide:Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                        Rectangle{width:parent.width;height:1;color:root.edge}
                        Text{
                            text:"Lifecycle: CURRENT / STALE / MISSING · Data updates only after authenticated Product owner validation."
                            color:root.dim;font.pixelSize:11
                            topPadding:8
                        }
                    }
                }
            }
        }

        Row{
            width:parent.width
            height:22
            spacing:0
            Text{
                width:parent.width/2
                text:root.standaloneModuleMode
                    ? "RGS MASTERLAB  ·  INDIVIDUAL PANEL PROTOTYPE  ·  NOT QUALIFIED"
                    : "RGS MASTERLAB  ·  ANALYSIS UI PROTOTYPE  ·  NOT QUALIFIED"
                color:root.dim;font.pixelSize:10
                elide:Text.ElideRight
            }
            Text{
                width:parent.width/2
                text:"DESIGN REVIEW  ·  NO PRODUCT WRITE / NO M18 MERGE"
                color:root.amber;font.pixelSize:10
                horizontalAlignment:Text.AlignRight
                elide:Text.ElideLeft
            }
        }
    }
}
