import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

Rectangle {
    id: root
    objectName: "auditionSourceSelectorPanel"
    height: content.implicitHeight + 32
    radius: 10
    color: "#22262c"
    border.color: "#343a42"

    FileDialog {
        id: goldDialog
        objectName: "goldFileDialog"
        title: "Open Gold Reference WAV"
        fileMode: FileDialog.OpenFile
        nameFilters: ["WAV audio (*.wav *.wave)"]
        onAccepted: goldSelection.selectGold(selectedFile)
        onRejected: goldSelection.cancelGoldSelection()
    }

    Column {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            color: "#f1f3f5"
            font.pixelSize: 16
            font.weight: Font.DemiBold
            text: "Audition source"
        }

        Row {
            spacing: 8

            Button {
                objectName: "auditionPreparedButton"
                text: "PREPARED"
                enabled: auditionSelector.preparedAvailable
                         && auditionSelector.activeTarget !== "PREPARED"
                onClicked: auditionSelector.selectPrepared()
                Accessible.name: "Audition prepared realization"
            }
            Button {
                objectName: "auditionProcessedButton"
                text: "PROCESSED"
                enabled: auditionSelector.processedAvailable
                         && auditionSelector.activeTarget !== "PROCESSED"
                onClicked: auditionSelector.selectProcessed()
                Accessible.name: "Audition processed realization"
            }
            Button {
                objectName: "auditionGoldButton"
                text: "GOLD"
                enabled: auditionSelector.goldAvailable
                         && auditionSelector.activeTarget !== "GOLD"
                onClicked: auditionSelector.selectGold()
                Accessible.name: "Audition Gold Reference"
            }
        }

        Label {
            objectName: "activeAuditionTargetLabel"
            color: "#8ee3b1"
            text: "Active: " + auditionSelector.activeTarget
        }

        Row {
            spacing: 8
            Button {
                objectName: "goldOpenButton"
                text: "Pick Gold WAV…"
                onClicked: goldDialog.open()
                Accessible.name: "Pick independent Gold Reference WAV"
            }
            Button {
                objectName: "goldClearButton"
                text: "Clear Gold"
                enabled: goldSelection.hasGold
                onClicked: goldSelection.clearGold()
                Accessible.name: "Clear Gold Reference"
            }
        }

        Label {
            objectName: "goldStateLabel"
            width: parent.width
            color: goldSelection.hasGold ? "#d7dce2" : "#aeb5bd"
            elide: Text.ElideMiddle
            text: goldSelection.hasGold
                ? "Gold: " + goldSelection.displayName
                : "Gold: not loaded"
        }
        Label {
            objectName: "goldMetadataLabel"
            visible: goldSelection.hasGold
            color: "#aeb5bd"
            text: goldSelection.metadata
        }
        Label {
            objectName: "goldErrorLabel"
            visible: goldSelection.errorMessage.length > 0
            width: parent.width
            wrapMode: Text.Wrap
            color: "#ffd9dc"
            text: goldSelection.errorMessage
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
        Label {
            objectName: "auditionRoutingStatus"
            visible: auditionSelector.statusText.length > 0
            width: parent.width
            wrapMode: Text.Wrap
            color: "#ffd9dc"
            text: auditionSelector.statusText
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
    }
}
