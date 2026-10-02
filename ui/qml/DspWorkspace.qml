import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspWorkspace"

    property var adapterModel: null
    property int selectedModuleIndex: adapterModel ? adapterModel.selectedIndex : 0
    property var gainViewModel: null
    property var eqViewModel: null
    property var spectrumViewModel: null

    // Backward-compatibility alias so code referencing workspace.viewModel sets/gets eqViewModel
    property alias viewModel: root.eqViewModel

    property alias chainSelector: realChainSelector
    property alias editorHost: realEditorHost

    onSelectedModuleIndexChanged: {
        if (adapterModel && adapterModel.selectedIndex !== selectedModuleIndex) {
            adapterModel.setSelectedIndex(selectedModuleIndex)
        }
    }

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
            adapterModel: root.adapterModel
            selectedIndex: root.selectedModuleIndex
            gainViewModel: root.gainViewModel
            eqViewModel: root.eqViewModel
            onSelectedIndexChanged: {
                root.selectedModuleIndex = selectedIndex
                if (root.adapterModel) {
                    root.adapterModel.setSelectedIndex(selectedIndex)
                }
            }
        }

        DspEditorHost {
            id: realEditorHost
            objectName: "dspEditorHost"
            adapterModel: root.adapterModel
            selectedModuleIndex: root.selectedModuleIndex
            gainViewModel: root.gainViewModel
            eqViewModel: root.eqViewModel
            spectrumViewModel: root.spectrumViewModel
            activeChainRow: root.selectedModuleIndex === 0
                ? realChainSelector.inputGainRow
                : realChainSelector.parametricEqRow
        }
    }
}
