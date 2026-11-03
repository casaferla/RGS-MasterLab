// M21 PRIVATE Qt-only presentation candidate. Never in Main.qml.
import QtQuick 2.15
Rectangle {
    id: root
    objectName: "m21PrivatePreview"
    width: 1100
    height: 765
    color: "#14202A"
    property var presentation: ({ready:false})
    readonly property bool modelReady: !!(presentation && presentation.ready === true)
    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8
        Text {
            text: "METRICS / DELTA · PREPARED | GOLD | PROCESSED · DEMO DATA"
            font.bold: true
            font.family: "Arial"; font.pixelSize: 18
            color: "#DCEAF2"
        }
        Text {
            objectName: "m21ScopeWarning"
            text: root.modelReady ? root.presentation.scopeWarning :
                  "Independent GOLD analysis scope; no time alignment is implied."
            font.family: "Arial"; font.pixelSize: 12
            color: "#EAC28C"
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Text {
            objectName: "m21NoData"
            visible: !root.modelReady
            text: "MISSING · No authenticated metrics available"
            font.family: "Arial"; font.pixelSize: 13
            color: "#EAC28C"
        }
        Row {
            visible: root.modelReady
            width: parent.width
            height: 36
            spacing: 8
            Text {
                objectName: "m21PreparedScope"
                width: (parent.width-16)/3
                text: root.modelReady ? "PREPARED · " +
                    root.presentation.scopes[0] + " · " + root.presentation.states[0] : ""
                color: "#C8DFEF"
                font.family: "Arial"; font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Text {
                objectName: "m21GoldScope"
                width: (parent.width-16)/3
                text: root.modelReady ? "GOLD · " +
                    root.presentation.scopes[1] + " · " + root.presentation.states[1] : ""
                color: "#EFCB8D"
                font.family: "Arial"; font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Text {
                objectName: "m21ProcessedScope"
                width: (parent.width-16)/3
                text: root.modelReady ? "PROCESSED · " +
                    root.presentation.scopes[2] + " · " + root.presentation.states[2] : ""
                color: "#C8DFEF"
                font.family: "Arial"; font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
        Item {
            objectName: "m21MetricsTable"
            visible: root.modelReady
            width: parent.width
            height: 446
            Column {
                anchors.fill: parent
                spacing: 3
                Rectangle {
                    width: parent.width
                    height: 38
                    color: "#314352"
                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: ["METRIC","PREPARED","GOLD","PROCESSED",
                                    "Δ PREPARED→GOLD","Δ PROCESSED→GOLD"]
                            delegate: Text {
                                width: parent.width/6
                                height: parent.height
                                text: modelData
                                font.bold: true
                                font.family: "Arial"; font.pixelSize: 10
                                color: "#F5F7FA"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
                Repeater {
                    model: root.modelReady ? root.presentation.rows : []
                    delegate: Rectangle {
                        width: parent.width
                        height: 42
                        color: index%2===0 ? "#1C2B37" : "#253540"
                        Row {
                            anchors.fill: parent
                            Repeater {
                                model: [modelData.name,modelData.prepared,modelData.gold,
                                        modelData.processed,modelData.preparedDelta,
                                        modelData.processedDelta]
                                delegate: Text {
                                    width: parent.width/6
                                    height: parent.height
                                    text: modelData
                                    font.family: "Arial"; font.pixelSize: 10
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
        Text {
            objectName: "m21SpectrumAvailability"
            visible: root.modelReady
            width: parent.width
            text: root.modelReady ? "AVERAGE SPECTRUM · PREPARED " +
                root.presentation.spectrumStates[0] + " | GOLD " +
                root.presentation.spectrumStates[1] + " | PROCESSED " +
                root.presentation.spectrumStates[2] : ""
            color: "#A7CADC"
            font.family: "Arial"; font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
        Text {
            visible: root.modelReady
            width: parent.width
            text: "TONAL DELTAS · ONLY externally certified normalized shapeDb " +
                  "and common-band evidence. BAND ID is not a frequency label."
            color: "#EAC28C"
            font.family: "Arial"; font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
        ListView {
            objectName: "m21TonalList"
            visible: root.modelReady
            width: parent.width
            height: 105
            clip: true
            model: root.modelReady ? root.presentation.tonalBands : []
            delegate: Rectangle {
                width: ListView.view.width
                height: 25
                color: index%2===0 ? "#1C2B37" : "#253540"
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 7
                    text: "BAND ID " + modelData.bandId +
                          "  |  PREPARED Δ " + modelData.preparedDelta +
                          "  |  PROCESSED Δ " + modelData.processedDelta
                    color: "#DCEAF2"
                    font.family: "Arial"; font.pixelSize: 11
                }
            }
        }
    }
}
