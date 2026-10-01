import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspEditorHost"

    property int selectedModuleIndex: 0
    property var gainViewModel: null
    property var eqViewModel: null
    property var spectrumViewModel: null
    property Item activeChainRow: null

    // Backward-compatibility alias so code referencing host.viewModel accesses eqViewModel
    property alias viewModel: root.eqViewModel

    readonly property var activeViewModel: selectedModuleIndex === 0 ? gainViewModel : eqViewModel
    readonly property string moduleTitle: selectedModuleIndex === 0 ? "Input Gain" : "Parametric EQ"
    readonly property string moduleContext: selectedModuleIndex === 0 ? "Gain Staging / Manual Mastering" : "Manual Mastering"

    implicitWidth: 992
    implicitHeight: 612

    // Board02 Editor Panel Material
    radius: 8
    color: "#0F2236"
    border.color: "#27465F"
    border.width: 1

    gradient: Gradient {
        GradientStop { position: 0.0; color: "#17324D" }
        GradientStop { position: 1.0; color: "#0C1A2A" }
    }

    // Top highlight
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        height: 1
        color: "#262A6C9F"
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape && activeChainRow) {
            activeChainRow.forceActiveFocus()
            event.accepted = true
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: activeChainRow !== null
        onActivated: {
            if (activeChainRow) {
                activeChainRow.forceActiveFocus()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Host Header Strip (40 lp)
        RowLayout {
            objectName: "dspHostHeaderRegion"
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            spacing: 12

            ColumnLayout {
                spacing: 1
                Text {
                    text: root.moduleTitle
                    color: "#F5F8FC"
                    font.family: "Segoe UI"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
                Text {
                    text: root.moduleContext
                    color: "#A1B5C9"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                }
            }

            Item { Layout.fillWidth: true }

            // Preview Status Badge
            Rectangle {
                objectName: "dspHostPreviewBadge"
                Layout.preferredHeight: 28
                Layout.preferredWidth: statusText.implicitWidth + 24
                radius: 6
                color: {
                    if (!root.activeViewModel) return "#0F2236"
                    const status = root.activeViewModel.previewStatus
                    if (status === "RENDERING") return "#2A2814"
                    if (status === "READY") return "#1400D47A"
                    if (status === "ERROR") return "#14F27683"
                    return "#0F2236"
                }
                border.color: {
                    if (!root.activeViewModel) return "#27465F"
                    const status = root.activeViewModel.previewStatus
                    if (status === "RENDERING") return "#F2B632"
                    if (status === "READY") return "#00D47A"
                    if (status === "ERROR") return "#F27683"
                    return "#27465F"
                }
                border.width: 1

                Text {
                    id: statusText
                    objectName: "dspHostStatusText"
                    anchors.centerIn: parent
                    text: {
                        if (!root.activeViewModel) return "NO PREVIEW"
                        const status = root.activeViewModel.previewStatus
                        if (status === "NO_PREPARED_REALIZATION" || status === "IDLE") return "NO PREVIEW"
                        return status
                    }
                    color: {
                        if (!root.activeViewModel) return "#A1B5C9"
                        const status = root.activeViewModel.previewStatus
                        if (status === "RENDERING") return "#F2B632"
                        if (status === "READY") return "#00D47A"
                        if (status === "ERROR") return "#F27683"
                        return "#A1B5C9"
                    }
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
            }

            // Undo / Redo + A/B Bypass Controls
            RowLayout {
                spacing: 2

                StudioButton {
                    id: undoButton
                    objectName: "eqUndoButton"
                    text: "Undo"
                    minimumControlWidth: 64
                    visible: root.selectedModuleIndex === 1
                    enabled: root.selectedModuleIndex === 1 && root.eqViewModel ? root.eqViewModel.canUndo : false
                    onClicked: if (root.eqViewModel) root.eqViewModel.undo()
                    Accessible.name: "Undo edit"
                    ToolTip.text: "Undo last edit (Ctrl+Z)"
                    ToolTip.visible: hovered
                    Shortcut { sequence: "StandardKey.Undo"; enabled: root.selectedModuleIndex === 1 && root.eqViewModel && root.eqViewModel.canUndo; onActivated: root.eqViewModel.undo() }
                }

                StudioButton {
                    id: redoButton
                    objectName: "eqRedoButton"
                    text: "Redo"
                    minimumControlWidth: 64
                    visible: root.selectedModuleIndex === 1
                    enabled: root.selectedModuleIndex === 1 && root.eqViewModel ? root.eqViewModel.canRedo : false
                    onClicked: if (root.eqViewModel) root.eqViewModel.redo()
                    Accessible.name: "Redo edit"
                    ToolTip.text: "Redo last edit (Ctrl+Y)"
                    ToolTip.visible: hovered
                    Shortcut { sequence: "StandardKey.Redo"; enabled: root.selectedModuleIndex === 1 && root.eqViewModel && root.eqViewModel.canRedo; onActivated: root.eqViewModel.redo() }
                }

                Item {
                    Layout.preferredWidth: 8
                    visible: root.selectedModuleIndex === 1
                }

                StudioButton {
                    id: abActiveButton
                    objectName: "abButtonActive"
                    text: root.selectedModuleIndex === 0 ? "A: Gain Active" : "A: EQ Active"
                    selected: root.activeViewModel ? !root.activeViewModel.bypass : true
                    tone: "primary"
                    accentColor: "#00C8FF"
                    onClicked: if (root.activeViewModel) root.activeViewModel.setBypass(false)
                    Accessible.name: text
                    ToolTip.text: "Activate module processing"
                    ToolTip.visible: hovered
                }

                StudioButton {
                    id: abBypassButton
                    objectName: "abButtonBypass"
                    text: "B: Bypass"
                    selected: root.activeViewModel ? root.activeViewModel.bypass : false
                    tone: "gold"
                    accentColor: "#F2B632"
                    onClicked: if (root.activeViewModel) root.activeViewModel.setBypass(true)
                    Accessible.name: "B: Bypass"
                    ToolTip.text: "Bypass module processing"
                    ToolTip.visible: hovered
                }
            }
        }

        // Active Editor Content Area
        InputGainEditor {
            id: inputGainEditor
            objectName: "inputGainEditor"
            visible: root.selectedModuleIndex === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.gainViewModel
        }

        ParametricEqEditor {
            id: parametricEqEditor
            objectName: "parametricEqEditor"
            visible: root.selectedModuleIndex === 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.eqViewModel
            spectrumViewModel: root.spectrumViewModel
        }
    }
}
