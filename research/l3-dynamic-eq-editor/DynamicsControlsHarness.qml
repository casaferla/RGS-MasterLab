import QtQuick
import QtQuick.Controls
import "../../ui/qml" as Studio
ApplicationWindow {
    id: root
    width: 1184
    height: 688
    visible: true
    color: "#081824"
    title: "RGS Dynamic EQ C01 — isolated controls research"
    Studio.DynamicEqEditor {
        objectName: "dynamicEqResearchEditor"
        anchors.fill: parent
        anchors.margins: 16
        viewModel: null
        spectrumViewModel: null
        dynamicsControls: researchDynamics
        showDynamics: true
    }
}
