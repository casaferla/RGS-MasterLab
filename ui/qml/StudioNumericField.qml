import QtQuick
import QtQuick.Controls

Item {
    id: control

    property string labelText: ""
    property string unitText: ""
    property string fieldName: ""
    property string rawText: ""
    property var viewModel: null

    readonly property bool isInvalid: viewModel !== null && viewModel !== undefined && viewModel.validationField === fieldName

    implicitWidth: 164 + (unitText.length > 0 ? unitTextLabel.implicitWidth + 8 : 0)
    implicitHeight: 50

    Column {
        anchors.fill: parent
        spacing: 6

        Text {
            id: label
            text: control.labelText
            color: "#A1B5C9"
            font.family: "Segoe UI"
            font.pixelSize: 12
            font.weight: Font.Bold
            height: 16
        }

        Row {
            spacing: 8

            Rectangle {
                width: 164
                height: 28
                radius: 4
                color: "#0E1F2E"
                border.width: input.activeFocus ? 2 : 1
                border.color: {
                    if (control.isInvalid) return "#F27683"
                    if (input.activeFocus) return "#00C8FF"
                    if (fieldMouseArea.containsMouse) return "#5900C8FF"
                    return "#2C5A78"
                }

                // Focus/Invalid Overlays
                Rectangle {
                    anchors.fill: parent
                    radius: 3
                    color: control.isInvalid ? "#14F27683" : (input.activeFocus ? "#1400C8FF" : "transparent")
                }

                MouseArea {
                    id: fieldMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: input.forceActiveFocus()
                }

                TextInput {
                    id: input
                    objectName: control.fieldName + "Input"
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: TextInput.AlignVCenter
                    horizontalAlignment: TextInput.AlignRight

                    text: control.rawText
                    font.family: "Consolas"
                    font.pixelSize: 16
                    color: control.isInvalid ? "#F27683" : "#F5F8FC"
                    selectByMouse: true
                    selectionColor: "#4000C8FF"
                    selectedTextColor: "#F5F8FC"
                    activeFocusOnTab: true

                    onTextEdited: {
                        if (control.viewModel) {
                            if (control.fieldName === "frequency") {
                                control.viewModel.setDraftFrequencyText(input.text)
                            } else if (control.fieldName === "gain") {
                                control.viewModel.setDraftGainText(input.text)
                            } else if (control.fieldName === "q") {
                                control.viewModel.setDraftQText(input.text)
                            } else if (control.fieldName === "shelfSlope") {
                                control.viewModel.setDraftShelfSlopeText(input.text)
                            }
                        }
                    }

                    Keys.onReturnPressed: {
                        if (control.viewModel) control.viewModel.commitDraft()
                    }
                    Keys.onEnterPressed: {
                        if (control.viewModel) control.viewModel.commitDraft()
                    }
                    Keys.onEscapePressed: {
                        if (control.viewModel) control.viewModel.cancelDraft()
                    }

                    onActiveFocusChanged: {
                        if (!activeFocus && control.viewModel) {
                            control.viewModel.commitDraft()
                        }
                    }

                    Accessible.name: control.labelText + " numeric input field"
                    Accessible.description: control.isInvalid ? (control.viewModel ? control.viewModel.validationMessage : "") : ""
                }
            }

            Text {
                id: unitTextLabel
                visible: control.unitText.length > 0
                anchors.verticalCenter: parent.verticalCenter
                text: control.unitText
                color: "#A1B5C9"
                font.family: "Segoe UI"
                font.pixelSize: 12
            }
        }
    }
}
