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

    function updateChainParenting() {
        if (root.isCompact && root.chainContainer) {
            chainSelector.parent = root.chainContainer
            chainSelector.anchors.fill = chainSelector.parent
        } else {
            chainSelector.parent = dspWorkspaceRow
            chainSelector.anchors.fill = undefined
        }
    }

    onIsCompactChanged: Qt.callLater(updateChainParenting)
    onChainContainerChanged: Qt.callLater(updateChainParenting)
    Component.onCompleted: updateChainParenting()

    RowLayout {
        id: dspWorkspaceRow
        objectName: "dspWorkspaceRow"
        anchors.fill: parent
        spacing: 8

        DspChainSelector {
            id: chainSelector
            objectName: "dspChainSelector"
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
