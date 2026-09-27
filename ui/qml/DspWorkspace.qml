import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspWorkspace"

    property var viewModel: null
    property int selectedModuleIndex: 0
    property bool isCompact: false
    property Item chainContainer: null

    color: "transparent"

    RowLayout {
        id: dspWorkspaceRow
        objectName: "dspWorkspaceRow"
        anchors.fill: parent
        spacing: 8

        DspChainSelector {
            id: chainSelector
            objectName: "dspChainSelector"
            parent: (root.isCompact && root.chainContainer) ? root.chainContainer : dspWorkspaceRow
            Layout.preferredWidth: root.isCompact ? 164 : 180
            Layout.fillHeight: true
            isCompact: root.isCompact
            selectedIndex: root.selectedModuleIndex
            eqViewModel: root.viewModel
            onSelectedIndexChanged: root.selectedModuleIndex = selectedIndex
        }

        DspEditorHost {
            id: editorHost
            objectName: "dspEditorHost"
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.viewModel
            activeChainRow: chainSelector.findChild ? chainSelector.findChild("dspChainRow_0") : null
        }
    }
}
