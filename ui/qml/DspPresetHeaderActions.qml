import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// ONE minimal Open/Save pair, reused by the DSP CHAIN header and
// every editable docked/floating module. All file I/O and actual DSP
// state mutation are executed by the native Product controller.
// The Qt FileDialog + explicit confirmation are user-owned actions;
// no preset load can act as an M12 AutoMatch controller permission.
Item {
    id: root
    objectName: "dspPresetHeaderActions"
    property var presetController: null
    property bool entireChain: false
    property string moduleInstanceId: ""
    property string moduleDisplayName: ""
    readonly property bool actionsAvailable: !!presetController
        && (entireChain || moduleInstanceId.length > 0)
    visible: actionsAvailable
    implicitWidth: 45
    implicitHeight: 22
    width: implicitWidth
    height: implicitHeight

    function scopeName() {
        return entireChain ? "DSP chain" : moduleDisplayName
    }
    function showFailure() {
        problemMessage.text = presetController && presetController.errorText
            ? presetController.errorText
            : "The preset operation could not be completed."
        failureDialog.open()
    }

    Row {
        anchors.centerIn: parent
        spacing: 3
        StudioIconButton {
            id: presetOpen
            objectName: root.entireChain
                ? "dspChainPresetOpen" : "dspModulePresetOpen"
            iconKind: "preset-open"
            controlSize: 21
            iconSize: 16
            enabled: root.actionsAvailable
            onClicked: openFileDialog.open()
            Accessible.name: "Open settings for " + root.scopeName()
            ToolTip.text: root.entireChain
                ? "Open DSP chain settings"
                : "Open " + root.scopeName() + " settings"
            ToolTip.visible: hovered && openTooltipDwell.ready
            ToolTip.delay: 0
            StudioTooltipDwell {
                id: openTooltipDwell
                anchors.fill: parent
            }
        }
        StudioIconButton {
            id: presetSave
            objectName: root.entireChain
                ? "dspChainPresetSave" : "dspModulePresetSave"
            iconKind: "preset-save"
            controlSize: 21
            iconSize: 16
            enabled: root.actionsAvailable
            onClicked: saveFileDialog.open()
            Accessible.name: "Save settings for " + root.scopeName()
            ToolTip.text: root.entireChain
                ? "Save DSP chain settings"
                : "Save " + root.scopeName() + " settings"
            ToolTip.visible: hovered && saveTooltipDwell.ready
            ToolTip.delay: 0
            StudioTooltipDwell {
                id: saveTooltipDwell
                anchors.fill: parent
            }
        }
    }

    FileDialog {
        id: saveFileDialog
        title: "Save " + root.scopeName() + " preset"
        fileMode: FileDialog.SaveFile
        nameFilters: ["RGS DSP Preset (*.rgspreset)", "JSON (*.json)"]
        defaultSuffix: "rgspreset"
        onAccepted: {
            if (!root.presetController) return
            const ok = root.entireChain
                ? root.presetController.saveChainPreset(selectedFile)
                : root.presetController.saveModulePreset(
                    selectedFile,root.moduleInstanceId)
            if (!ok) root.showFailure()
        }
    }
    FileDialog {
        id: openFileDialog
        title: "Open " + root.scopeName() + " preset"
        fileMode: FileDialog.OpenFile
        nameFilters: ["RGS DSP Preset (*.rgspreset)", "JSON (*.json)"]
        onAccepted: confirmRestoreDialog.open()
    }
    Dialog {
        id: confirmRestoreDialog
        title: "Apply " + root.scopeName() + " settings?"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        parent: Overlay.overlay
        anchors.centerIn: parent
        contentItem: Label {
            text: "This will replace the current MANUAL settings for "
                + root.scopeName()
                + ". The change is not an AutoMatch approval."
            color: "#DDE6F3"
            wrapMode: Text.WordWrap
            width: 340
            font.pixelSize: 12
        }
        onAccepted: {
            if (!root.presetController) return
            const ok = root.entireChain
                ? root.presetController.openChainPreset(openFileDialog.selectedFile)
                : root.presetController.openModulePreset(
                    openFileDialog.selectedFile,root.moduleInstanceId)
            if (!ok) root.showFailure()
        }
    }
    Dialog {
        id: failureDialog
        title: "DSP preset"
        modal: true
        standardButtons: Dialog.Ok
        parent: Overlay.overlay
        anchors.centerIn: parent
        contentItem: Label {
            id: problemMessage
            color: "#F2B3BB"
            wrapMode: Text.WordWrap
            width: 340
            font.pixelSize: 12
        }
    }
}
