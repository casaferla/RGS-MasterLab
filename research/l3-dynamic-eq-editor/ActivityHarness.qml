import QtQuick
import QtQuick.Controls
import "../../ui/qml" as Studio
// Real C01 DSP fixture observations; test PlaybackSnapshot is simulated.
ApplicationWindow {
    id: root
    width: 1184
    height: 688
    visible: true
    color: "#081824"
    title: "RGS Dynamic EQ C01 — real DSP / test playback snapshot"
    Studio.DynamicEqEditor {
        objectName: "dynamicEqActivityResearchEditor"
        anchors.fill: parent
        anchors.margins: 16
        viewModel: null
        spectrumViewModel: null
        dynamicActivity: c01ActivityQt
        showActivity: true
    }
    Text {
        x: 15;y: 2
        text: "C01 GAINS ARE REAL; PLAYBACK SNAPSHOT SIMULATED"
        color: "#F2B632"
        font.pixelSize: 10
        font.bold: true
    }
}
