// PRIVATE M21 static preview. No Main.qml registration or product API.
import QtQuick 2.15
Rectangle {
    id: root
    objectName: "m21PrivatePreview"
    width: 1100; height: 610
    color: "#14202A"
    property var presentation: ({ready:false})
    readonly property bool modelReady: !!(presentation && presentation.ready === true)
    Column {
        anchors.fill: parent; anchors.margins: 16; spacing: 9
        Text {
            text: "METRICS / DELTA · PREPARED | GOLD | PROCESSED"
            font.bold: true; font.pixelSize: 18; color: "#DCEAF2"
        }
        Text {
            objectName: "m21ScopeWarning"
            text: root.modelReady ? root.presentation.scopeWarning :
                  "Independent GOLD analysis scope; no time alignment is implied."
            font.pixelSize: 12; color: "#EAC28C"
        }
        Text {
            objectName: "m21NoData"
            visible: !root.modelReady
            text: "MISSING · No authenticated metrics available"
            font.pixelSize: 13; color: "#EAC28C"
        }
        Item {
            objectName: "m21MetricsTable"
            visible: root.modelReady
            width: parent.width; height: 490
            Column {
                anchors.fill: parent; spacing: 3
                Rectangle {
                    width: parent.width; height: 38; color: "#314352"
                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: ["METRIC","PREPARED","GOLD","PROCESSED","Δ PREPARED→GOLD","Δ PROCESSED→GOLD"]
                            delegate: Text {
                                width: parent.width/6; height: parent.height
                                text: modelData; font.bold: true; font.pixelSize: 10
                                color: "#F5F7FA"; horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
                Repeater {
                    model: root.modelReady ? root.presentation.rows : []
                    delegate: Rectangle {
                        width: parent.width; height: 44
                        color: index%2===0 ? "#1C2B37" : "#253540"
                        Row {
                            anchors.fill: parent
                            Repeater {
                                model: [modelData.name,modelData.prepared,modelData.gold,
                                        modelData.processed,modelData.preparedDelta,
                                        modelData.processedDelta]
                                delegate: Text {
                                    width: parent.width/6; height: parent.height
                                    text: modelData; font.pixelSize: 10
                                    color: text.indexOf("STALE")>=0 ? "#EAC28C" : "#DFE9F0"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
