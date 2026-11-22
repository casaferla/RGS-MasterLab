import QtQuick
import QtQuick.Controls
import "../../ui/qml" as Studio

// Identical geometry / visual slots. Source EQ remains hidden by opacity,
// but fully instantiated to expose measured layout positions.
ApplicationWindow {
    id: root
    objectName: "dynamicEqResearchWindow"
    width: 1184
    height: 688
    visible: true
    color: "#081824"
    title: "RGS Dynamic EQ — isolated Parametric EQ geometry parity"
    Studio.ParametricEqEditor {
        objectName: "originalEqResearch"
        x: 16; y: 16
        width: root.width - 32
        height: root.height - 32
        opacity: 0.0
        viewModel: null
        spectrumViewModel: null
    }
    Studio.DynamicEqEditor {
        objectName: "dynamicEqResearch"
        x: 16; y: 16
        width: root.width - 32
        height: root.height - 32
        viewModel: null
        spectrumViewModel: null
    }
}
