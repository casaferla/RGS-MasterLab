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

        // Header Action Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

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
                        horizontalAlignment: Text.AlignHCenter
                        Layout.preferredWidth: 240
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

                        // UX-002: accumulate sub-detent motion without
                        // scheduling a render for each high-resolution tick.
                        property real wheelRemainder: 0

                        // Transparent whole-slider wheel target: preserve
                        // the Slider's existing mouse drag/handle semantics.
                        MouseArea {
                            id: gainWheelArea
                            objectName: "inputGainWheelArea"
                            anchors.fill: parent
                            z: 5
                            acceptedButtons: Qt.NoButton
                            hoverEnabled: true
                            enabled: gainSlider.enabled && !gainSlider.pressed
                            onWheel: function(wheel) {
                                gainSlider.wheelRemainder += wheel.angleDelta.y
                                var steps = Math.trunc(gainSlider.wheelRemainder / 120)
                                if (steps !== 0) {
                                    gainSlider.wheelRemainder -= steps * 120
                                    var next = Math.round(
                                        (root.currentGainDb + steps * 0.1) * 10) / 10
                                    next = Math.max(gainSlider.from,
                                                    Math.min(gainSlider.to, next))
                                    if (next !== root.currentGainDb && root.viewModel) {
                                        gainSlider.pendingGainDb = next
                                        // Existing undo and preview; never
                                        // commit sub-detent high-resolution ticks.
                                        root.viewModel.setGainDb(next)
                                    }
                                }
                                wheel.accepted = true
                            }
                        }

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
                            id: gainSliderTrack
                            objectName: "gainSliderTrack"
                            x: gainSlider.leftPadding
                            y: gainSlider.topPadding + gainSlider.availableHeight / 2 - height / 2
                            width: gainSlider.availableWidth
                            height: 8
                            radius: 4
                            color: "#08121C"
                            border.color: "#1E354A"
                            border.width: 1

                            // Same bipolar convention as Compressor MAKE-UP
                            // and Stereo/M-S gains: fill from the 0 dB handle
                            // CENTER to the actual handle CENTER, never to
                            // the visual track edge. No fill at exactly 0 dB.
                            readonly property real thumbRadius:
                                gainSlider.handle ? gainSlider.handle.width / 2 : 9
                            readonly property real zeroCenterX:
                                thumbRadius + 0.5 * (width - 2 * thumbRadius)
                            readonly property real thumbCenterX:
                                thumbRadius + gainSlider.visualPosition
                                * Math.max(0, width - 2 * thumbRadius)
                            Rectangle {
                                objectName: "gainSliderTrackFill"
                                x: Math.min(gainSliderTrack.zeroCenterX,
                                            gainSliderTrack.thumbCenterX)
                                y: 0
                                width: Math.abs(gainSliderTrack.thumbCenterX
                                                - gainSliderTrack.zeroCenterX)
                                height: parent.height
                                radius: parent.radius
                                color: Qt.alpha(root.accent, 0.38)
                            }

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
                            objectName: "gainSliderThumb"
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
                        objectName: "gainParameterLabel"
                        text: "GAIN"
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
                            font.pixelSize: 16
                            color: root.hasError ? root.error : root.textPrimary
                            selectByMouse: true
                            selectionColor: "#4000C8FF"
                            selectedTextColor: root.textPrimary
                            activeFocusOnTab: true
                            property string interactionHint: "Drag the slider above to adjust\nEnter to apply • Esc to cancel"

                            HoverHandler {
                                id: gainInputHover
                            }

                            ToolTip.text: gainInput.interactionHint
                            ToolTip.visible: gainInputHover.hovered
                            ToolTip.delay: 550

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
                            Accessible.description: gainInput.interactionHint
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
