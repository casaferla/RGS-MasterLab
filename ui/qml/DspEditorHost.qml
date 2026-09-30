import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspEditorHost"

    property var viewModel: null
    property var spectrumViewModel: null
    property string moduleTitle: "Parametric EQ"
    property string moduleContext: "Manual Mastering"
    property Item activeChainRow: null

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

    Keys.onEscapePressed: function(event) {
        if (activeChainRow) {
            activeChainRow.forceActiveFocus()
            event.accepted = true
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
                    if (!root.viewModel) return "#0F2236"
                    const status = root.viewModel.previewStatus
                    if (status === "RENDERING") return "#2A2814"
                    if (status === "READY") return "#1400D47A"
                    if (status === "ERROR") return "#14F27683"
                    return "#0F2236"
                }
                border.color: {
                    if (!root.viewModel) return "#27465F"
                    const status = root.viewModel.previewStatus
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
                        if (!root.viewModel) return "NO PREVIEW"
                        const status = root.viewModel.previewStatus
                        if (status === "NO_PREPARED_REALIZATION" || status === "IDLE") return "NO PREVIEW"
                        return status
                    }
                    color: {
                        if (!root.viewModel) return "#A1B5C9"
                        const status = root.viewModel.previewStatus
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
                    objectName: "eqUndoButton"
                    text: "Undo"
                    minimumControlWidth: 64
                    enabled: root.viewModel ? root.viewModel.canUndo : false
                    onClicked: if (root.viewModel) root.viewModel.undo()
                    Accessible.name: "Undo edit"
                    ToolTip.text: "Undo last edit (Ctrl+Z)"
                    ToolTip.visible: hovered
                    Shortcut { sequence: "StandardKey.Undo"; enabled: root.viewModel && root.viewModel.canUndo; onActivated: root.viewModel.undo() }
                }

                StudioButton {
                    objectName: "eqRedoButton"
                    text: "Redo"
                    minimumControlWidth: 64
                    enabled: root.viewModel ? root.viewModel.canRedo : false
                    onClicked: if (root.viewModel) root.viewModel.redo()
                    Accessible.name: "Redo edit"
                    ToolTip.text: "Redo last edit (Ctrl+Y)"
                    ToolTip.visible: hovered
                    Shortcut { sequence: "StandardKey.Redo"; enabled: root.viewModel && root.viewModel.canRedo; onActivated: root.viewModel.redo() }
                }

                Item { Layout.preferredWidth: 8 }

                StudioButton {
                    objectName: "abButtonActive"
                    text: "A: EQ Active"
                    selected: root.viewModel ? !root.viewModel.bypass : true
                    tone: "primary"
                    accentColor: "#00C8FF"
                    onClicked: if (root.viewModel) root.viewModel.setBypass(false)
                    Accessible.name: "A: EQ Active"
                    ToolTip.text: "Activate module processing"
                    ToolTip.visible: hovered
                }

                StudioButton {
                    objectName: "abButtonBypass"
                    text: "B: Bypass"
                    selected: root.viewModel ? root.viewModel.bypass : false
                    tone: "gold"
                    accentColor: "#F2B632"
                    onClicked: if (root.viewModel) root.viewModel.setBypass(true)
                    Accessible.name: "B: Bypass"
                    ToolTip.text: "Bypass module processing"
                    ToolTip.visible: hovered
                }
            }
        }

        // Active Editor Content Area
        ParametricEqEditor {
            id: parametricEqEditor
            objectName: "parametricEqEditor"
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewModel: root.viewModel
            spectrumViewModel: root.spectrumViewModel
        }
    }
}
