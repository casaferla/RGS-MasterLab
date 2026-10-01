import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "inputGainEditor"

    property var viewModel: null

    readonly property color textPrimary: "#F5F8FC"
    readonly property color textSecondary: "#A1B5C9"
    readonly property color textMuted: "#586773"
    readonly property color accent: "#00C8FF"
    readonly property color error: "#F27683"
    readonly property color borderDark: "#1E354A"
    readonly property color panelBg: "#0B1824"

    readonly property double currentGainDb: root.viewModel ? root.viewModel.gainDb : 0.0
    readonly property string currentGainText: root.viewModel ? root.viewModel.gainDbText : "0.0"
    readonly property string validationErrorText: root.viewModel ? root.viewModel.validationError : ""
    readonly property bool hasError: validationErrorText.length > 0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // Header / Module Role Banner
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                spacing: 2

                Text {
                    text: "GAIN STAGING"
                    color: root.accent
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                }

                Text {
                    text: "Fixed First Mastering Stage"
                    color: root.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                }
            }

            Item { Layout.fillWidth: true }

            // Reset to Default Button
            StudioButton {
                id: resetButton
                objectName: "resetGainButton"
                text: "Reset to Default (0.0 dB)"
                minimumControlWidth: 160
                enabled: root.viewModel !== null
                onClicked: if (root.viewModel) root.viewModel.resetToDefault()
                Accessible.name: "Reset Input Gain to 0.0 dB"
                ToolTip.text: "Reset gain to canonical 0.0 dB"
                ToolTip.visible: hovered
            }
        }

        // Center Panel: Large Display, Slider, Direct Numeric Input
        Rectangle {
            id: mainPanel
            objectName: "gainMainPanel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: root.panelBg
            border.color: root.hasError ? root.error : root.borderDark
            border.width: 1
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 20

                // Readout Section
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 4

                    Text {
                        text: "INPUT GAIN"
                        color: root.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        id: readout
                        objectName: "gainDbDisplay"
                        text: (root.currentGainDb > 0 ? "+" : "") + root.currentGainText + " dB"
                        color: root.hasError ? root.error : (root.currentGainDb === 0.0 ? root.textPrimary : root.accent)
                        font.family: "Consolas"
                        font.pixelSize: 36
                        font.weight: Font.Bold
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                // Continuous Slider with 0 dB Tick & Range Labels
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 640
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 6

                    // Top Range Labels
                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "-24.0 dB"
                            color: root.textMuted
                            font.family: "Consolas"
                            font.pixelSize: 10
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: "0.0 dB"
                            color: root.currentGainDb === 0.0 ? root.accent : root.textSecondary
                            font.family: "Consolas"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: "+24.0 dB"
                            color: root.textMuted
                            font.family: "Consolas"
                            font.pixelSize: 10
                        }
                    }

                    // Slider Track & Handle
                    Slider {
                        id: gainSlider
                        objectName: "gainSlider"
                        Layout.fillWidth: true
                        from: -24.0
                        to: 24.0
                        stepSize: 0.1
                        value: root.currentGainDb
                        enabled: root.viewModel !== null
                        activeFocusOnTab: true

                        Binding {
                            target: gainSlider
                            property: "value"
                            value: root.currentGainDb
                            when: !gainSlider.pressed
                        }

                        // Keep continuous pointer motion presentation-only.
                        // Committing every pixel would request a full-chain render for
                        // every move. Track the drag locally and commit exactly once
                        // at release. Keyboard moves are discrete and may commit
                        // immediately because Slider.pressed is false for them.
                        property real pendingGainDb: root.currentGainDb

                        onMoved: {
                            pendingGainDb = gainSlider.value
                            if (!gainSlider.pressed && root.viewModel) {
                                root.viewModel.setGainDb(pendingGainDb)
                            }
                        }

                        onPressedChanged: {
                            if (gainSlider.pressed) {
                                pendingGainDb = gainSlider.value
                            } else if (root.viewModel) {
                                root.viewModel.setGainDb(pendingGainDb)
                            }
                        }

                        background: Rectangle {
                            x: gainSlider.leftPadding
                            y: gainSlider.topPadding + gainSlider.availableHeight / 2 - height / 2
                            width: gainSlider.availableWidth
                            height: 8
                            radius: 4
                            color: "#08121C"
                            border.color: "#1E354A"
                            border.width: 1

                            // Center 0 dB Reference Line
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.verticalCenter: parent.verticalCenter
                                width: 2
                                height: 14
                                color: root.currentGainDb === 0.0 ? root.accent : "#3A5268"
                            }
                        }

                        handle: Rectangle {
                            x: gainSlider.leftPadding + gainSlider.visualPosition * (gainSlider.availableWidth - width)
                            y: gainSlider.topPadding + gainSlider.availableHeight / 2 - height / 2
                            width: 18
                            height: 18
                            radius: 9
                            color: gainSlider.enabled ? "#E6EEF0" : "#586773"
                            border.color: gainSlider.activeFocus ? root.accent : "#1E354A"
                            border.width: gainSlider.activeFocus ? 2 : 1
                        }

                        Accessible.name: "Input Gain continuous adjustment slider"
                        Accessible.description: "Range -24.0 dB to +24.0 dB"
                    }
                }

                // Numeric Input Field
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 12

                    Text {
                        text: "Direct Entry:"
                        color: root.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        font.weight: Font.Bold
                    }

                    Rectangle {
                        width: 120
                        height: 32
                        radius: 4
                        color: "#0E1F2E"
                        border.width: gainInput.activeFocus ? 2 : 1
                        border.color: {
                            if (root.hasError) return root.error
                            if (gainInput.activeFocus) return root.accent
                            return "#2C5A78"
                        }

                        TextInput {
                            id: gainInput
                            objectName: "gainDbInput"
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            verticalAlignment: TextInput.AlignVCenter
                            horizontalAlignment: TextInput.AlignRight

                            text: root.currentGainText
                            font.family: "Consolas"
                            font.pixelSize: 14
                            color: root.hasError ? root.error : root.textPrimary
                            selectByMouse: true
                            selectionColor: "#4000C8FF"
                            selectedTextColor: root.textPrimary
                            activeFocusOnTab: true

                            // Treat typed text as a local draft. Committing on
                            // every keystroke would both reformat partial input
                            // (for example "3" -> "3.0") and request a full-chain
                            // preview before the user has finished entering a value.
                            // Enter/Return commits the complete text exactly once.
                            Keys.onReturnPressed: {
                                if (root.viewModel) {
                                    root.viewModel.setGainDbText(gainInput.text)
                                }
                            }
                            Keys.onEnterPressed: {
                                if (root.viewModel) {
                                    root.viewModel.setGainDbText(gainInput.text)
                                }
                            }
                            Keys.onEscapePressed: {
                                if (root.viewModel) {
                                    gainInput.text = root.viewModel.gainDbText
                                }
                            }

                            Accessible.name: "Input Gain numeric text input field"
                        }
                    }

                    Text {
                        text: "dB"
                        color: root.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                    }
                }

                // Validation Feedback Label
                Text {
                    id: validationErrorLabel
                    objectName: "gainValidationError"
                    text: root.validationErrorText
                    color: root.error
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    visible: root.hasError
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }
    }
}
