import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspWorkspace"

    property var adapterModel: null
    // Handles Chain selections consistently whether an editor is docked
    // or already floating, without allocating a duplicate DSP QML editor.
    property var moduleActivationHandler: null
    property var tearOffHandler: null
    property bool errorsInGlobalStatusBar: false
    property int selectedModuleIndex: adapterModel ? adapterModel.selectedIndex : 0
    property var gainViewModel: null
    property var eqViewModel: null
    property var spectrumViewModel: null
    property var truePeakStaticViewModel: null
    property var truePeakLiveViewModel: null
    property var terminalPostLimiterTruePeak: null
    property var terminalLimiterViewModel: null
    property var terminalMonitorViewModel: null
    property var limiterGrProvider: null

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
            moduleActivationHandler: root.moduleActivationHandler
            gainViewModel: root.gainViewModel
            eqViewModel: root.eqViewModel
        }

        DspEditorHost {
            id: realEditorHost
            objectName: "dspEditorHost"
            adapterModel: root.adapterModel
            gainViewModel: root.gainViewModel
            eqViewModel: root.eqViewModel
            errorsInGlobalStatusBar: root.errorsInGlobalStatusBar
            spectrumViewModel: root.spectrumViewModel
            truePeakStaticViewModel: root.truePeakStaticViewModel
            truePeakLiveViewModel: root.truePeakLiveViewModel
            terminalPostLimiterTruePeak: root.terminalPostLimiterTruePeak
            terminalLimiterViewModel: root.terminalLimiterViewModel
        terminalMonitorViewModel: root.terminalMonitorViewModel
            limiterGrProvider: root.limiterGrProvider
            activeChainRow: realChainSelector.activeRow
            onTearOffRequested: function(globalX,globalY) {
                if(root.tearOffHandler)root.tearOffHandler(globalX,globalY)
            }
        }
    }
}
