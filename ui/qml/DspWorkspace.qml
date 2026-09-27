import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspWorkspace"

    property var viewModel: null
    property int selectedModuleIndex: 0
    property bool isCompact: false

    color: "transparent"

    property alias chainSelector: realChainSelector

    Item {
        id: workspaceItemPool
        visible: false
        width: 0
        height: 0

        DspChainSelector {
            id: realChainSelector
            objectName: "dspChainSelector"
            isCompact: root.isCompact
            selectedIndex: root.selectedModuleIndex
            eqViewModel: root.viewModel
            onSelectedIndexChanged: root.selectedModuleIndex = selectedIndex
        }
    }

    RowLayout {
        id: dspWorkspaceRow
        objectName: "dspWorkspaceRow"
        anchors.fill: parent
        spacing: 8

        LayoutItemProxy {
            id: standardChainProxy
            target: realChainSelector
            visible: !root.isCompact
            Layout.preferredWidth: 180
            Layout.fillHeight: true
        }

        DspEditorHost {
            id: editorHost
            objectName: "dspEditorHost"
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.viewModel
            activeChainRow: realChainSelector.findChild ? realChainSelector.findChild("dspChainRow_0") : null
        }
    }
}
