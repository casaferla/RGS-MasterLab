import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspWorkspace"

    property var viewModel: null
    property int selectedModuleIndex: 0
    property bool isCompact: false

    property alias chainSelector: realChainSelector
    property alias editorHost: realEditorHost

    color: "transparent"

    // Logical workspace geometry follows the active docked editor/chain geometry.
    // The visual children are independently rehosted by Main.qml LayoutItemProxy
    // instances so no proxy ever targets both a parent and its descendant.
    width: realEditorHost.width
    height: Math.max(realEditorHost.height, realChainSelector.height)

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

        DspEditorHost {
            id: realEditorHost
            objectName: "dspEditorHost"
            viewModel: root.viewModel
            activeChainRow: realChainSelector.findChild ? realChainSelector.findChild("dspChainRow_0") : null
        }
    }
}
