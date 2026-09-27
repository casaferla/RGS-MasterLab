import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspWorkspace"

    property var eqViewModel: null
    property int selectedModuleIndex: 0

    color: "transparent"

    RowLayout {
        anchors.fill: parent
        spacing: 8

        DspChainSelector {
            id: chainSelector
            objectName: "dspChainSelector"
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            selectedIndex: root.selectedModuleIndex
            eqViewModel: root.eqViewModel
            onSelectedIndexChanged: root.selectedModuleIndex = selectedIndex
        }

        DspEditorHost {
            id: editorHost
            objectName: "dspEditorHost"
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.eqViewModel
            activeChainRow: chainSelector.findChild ? chainSelector.findChild("dspChainRow_0") : null
        }
    }
}
