import QtQuick
// Authentic per-band C01 applied dynamic gain, 5 seconds, no QML detector.
// Every plotted sample is from the accepted C01 render sidecar and its
// authoritative audible frame, not a fabricated live animation.
Item {
    id: root
    objectName: "dynamicEqActivityHistoryGraph"
    property var activityViewModel: null
    readonly property bool graphAvailable: !!activityViewModel
        && activityViewModel.available === true
        && activityViewModel.bandCount > 0
        && activityViewModel.buckets.length > 0
    readonly property var bandHues: [
        "#2ED3FF","#2FD98F","#FFD84A","#FF6B6B","#4F7CFF","#F5F8FC"]
    readonly property real secondsVisible: 5.0
    Text {
        x: 12; y: 6
        text: "APPLIED DYNAMIC GAIN  ·  LAST 5 SECONDS"
        color: "#B5C8D4"
        font.family: "Segoe UI";font.bold: true;font.pixelSize: 11
    }
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 14
        y: 6
        text: root.activityViewModel
            ? root.activityViewModel.stateText : "TELEMETRY UNAVAILABLE"
        color: root.graphAvailable ? "#45BFAE" : "#8093A2"
        font.family: "Segoe UI";font.pixelSize: 10
    }
    Text {
        visible: !root.graphAvailable
        anchors.centerIn: parent
        text: "Activity unavailable — no accepted audible C01 sidecar"
        color: "#8299AA"
        font.family: "Segoe UI";font.pixelSize: 12
    }
    Canvas {
        id: plot
        anchors.fill: parent
        anchors.topMargin: 30
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections {
            target: root.activityViewModel
            function onChanged() {plot.requestPaint()}
        }
        onPaint: {
            const ctx=getContext("2d")
            ctx.reset()
            if(!root.graphAvailable) return
            const n=root.activityViewModel.bandCount
            const buckets=root.activityViewModel.buckets
            const audible=root.activityViewModel.audibleFrame
            const fs=root.activityViewModel.sampleRateHz
            const left=68, right=Math.max(70,width-15)
            const duration=Math.max(1,5*fs)
            const begin=audible-duration
            const at=frame => left+(frame-begin)*(right-left)/duration
            const laneH=height/n
            for(let b=0;b<n;++b) {
                const y0=(b+0.5)*laneH
                const heightScale=laneH*0.38/18.0
                const valueY=gain => y0-gain*heightScale
                ctx.strokeStyle="rgba(122,154,177,0.45)"
                ctx.setLineDash([3,4]);ctx.lineWidth=1
                ctx.beginPath();ctx.moveTo(left,y0);ctx.lineTo(right,y0)
                ctx.stroke();ctx.setLineDash([])
                const summaries=root.activityViewModel.bandSummaries || []
                const id=buckets[0].bands[b].bandId
                const bandIndex=summaries.findIndex(entry => entry.bandId === id)
                if(bandIndex<0) continue
                const hue=root.bandHues[bandIndex]
                ctx.fillStyle=hue
                ctx.font="11px Consolas"
                ctx.fillText("B"+(bandIndex+1),12,y0+4)
                ctx.fillStyle="rgba(114,153,174,0.75)"
                ctx.font="9px Consolas"
                ctx.fillText("0 dB",34,y0+3)
                ctx.strokeStyle=hue;ctx.lineWidth=1.7
                ctx.beginPath()
                let started=false
                let lastGain=NaN
                for(let i=0;i<buckets.length;++i) {
                    const frame=buckets[i]
                    if(frame.frameEndExclusive<=begin
                       ||frame.frameEndExclusive>audible)continue
                    const data=frame.bands[b]
                    if(!data)continue
                    const x0=Math.max(left,at(frame.frameBegin))
                    const x1=Math.min(right,at(frame.frameEndExclusive))
                    if(x0>right || x1<left)continue
                    const top=valueY(data.gainMaxDb)
                    const bottom=valueY(data.gainMinDb)
                    ctx.fillStyle=hue
                    ctx.globalAlpha=0.24
                    ctx.fillRect(x0,top,Math.max(1,x1-x0),
                                 Math.max(1,bottom-top))
                    ctx.globalAlpha=1.0
                    const xm=(x0+x1)/2.0
                    const ym=valueY(data.gainLastDb)
                    if(!started){ctx.moveTo(xm,ym);started=true}
                    else ctx.lineTo(xm,ym)
                    lastGain=data.gainLastDb
                }
                if(started)ctx.stroke()
                if(isFinite(lastGain)) {
                    ctx.fillStyle=hue;ctx.font="11px Consolas"
                    ctx.fillText((lastGain>0?"+":"")
                        +lastGain.toFixed(2)+" dB",
                        Math.max(left,right-94),y0-laneH*0.27)
                }
            }
            ctx.fillStyle="#F2B632"
            ctx.fillRect(right-2,0,2,height)
        }
    }
}
