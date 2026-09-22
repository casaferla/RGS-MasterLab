import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string labelText: ""
    property string unitText: ""
    property string fieldName: ""
    property string rawText: ""
    property var viewModel: null
    property bool isInvalid: viewModel !== null && viewModel !== undefined && viewModel.validationField === fieldName && viewModel.validationMessage.length > 0

    signal commitRequested()
    signal cancelRequested()

    implicitWidth: 160
    implicitHeight: 32

    ColumnLayout {
        anchors.fill: parent
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: root.labelText
                color: root.isInvalid ? "#F27683" : "#8A97A3"
                font.family: "Segoe UI"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.unitText
                color: "#586773"
                font.family: "Segoe UI"
                font.pixelSize: 10
            }
        }

        Rectangle {
            id: inputBorder
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#050C11"
            border.color: {
                if (root.isInvalid) return "#F27683"
                if (editField.activeFocus) return "#00C8FF"
                return "#2A3947"
            }
            border.width: editField.activeFocus || root.isInvalid ? 2 : 1
            radius: 3

            TextInput {
                id: editField
                objectName: root.fieldName + "Input"
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                verticalAlignment: Text.AlignVCenter
                text: root.rawText
                color: root.isInvalid ? "#F27683" : "#E6EEF0"
                font.family: "Consolas"
                font.pixelSize: 12
                selectByMouse: true

                onTextEdited: {
                    if (root.fieldName === "frequency") {
                        if (root.viewModel) root.viewModel.setDraftFrequencyText(text)
                    } else if (root.fieldName === "gain") {
                        if (root.viewModel) root.viewModel.setDraftGainText(text)
                    } else if (root.fieldName === "q") {
                        if (root.viewModel) root.viewModel.setDraftQText(text)
                    } else if (root.fieldName === "shelfSlope") {
                        if (root.viewModel) root.viewModel.setDraftShelfSlopeText(text)
                    }
                }

                Keys.onReturnPressed: {
                    if (root.viewModel) root.viewModel.commitDraft()
                    root.commitRequested()
                }

                Keys.onEnterPressed: {
                    if (root.viewModel) root.viewModel.commitDraft()
                    root.commitRequested()
                }

                Keys.onEscapePressed: {
                    if (root.viewModel) root.viewModel.cancelDraft()
                    root.cancelRequested()
                }

                onActiveFocusChanged: {
                    if (!activeFocus && root.viewModel) {
                        root.viewModel.commitDraft()
                    }
                }
            }
        }
    }
}
