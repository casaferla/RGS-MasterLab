import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "streamingTruePeakLimiterEditor"
    property var limiterModule: null
    readonly property color copper: "#C4774A"
    // Top-left anchoring policy: changing window size may enlarge the
    // rounded Ceiling/Drive well towards the right and bottom, but may
    // never translate its compact-minimum top/left edges. Keep all outer
    // and inner top/left insets at their proven compact minimum. Interior
    // vertical dimensions can adapt continuously without scrolling.
    readonly property real verticalSpaceFactor:
        Math.max(0, Math.min(1, (root.height - 300) / 155))
    readonly property double ceiling: limiterModule ? limiterModule.ceilingDbTp : -1.0
    readonly property double drive: limiterModule ? limiterModule.driveDb : 0.0
    // Numeric drafts never render on each keystroke. Same 120x32,
    // Consolas 16, GAIN-field visual grammar as InputGainEditor.
    function commitNumeric(control, low, high, current, setter) {
        if (!root.limiterModule) return false
        if (!control.numericDirty) return true
        const raw = control.text.trim()
        const parsed = Number(raw.replace(",", "."))
        const field = control === ceilingEntry ? "Ceiling" : "Drive"
        if (!/^[+-]?(?:[0-9]+(?:[.,][0-9]*)?|[.,][0-9]+)$/.test(raw)
                || !Number.isFinite(parsed) || parsed < low || parsed > high) {
            if (root.limiterModule.reportNumericValidation)
                root.limiterModule.reportNumericValidation(field,
                    field + " must be a number between " + low + " and " + high + ".")
            return false
        }
        const canonical = Math.round(parsed * 10) / 10
        if (root.limiterModule.reportNumericValidation)
            root.limiterModule.reportNumericValidation(field, "")
        control.numericDirty = false
        if (canonical !== current) setter(canonical)
        control.text = canonical.toFixed(1)
        return true
    }
    function cancelNumeric(control, field, value) {
        control.numericDirty = false
        if (root.limiterModule && root.limiterModule.reportNumericValidation)
            root.limiterModule.reportNumericValidation(field, "")
        control.text = value.toFixed(1)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 5
        // Reset is in the shared fixed 2x2 utility cluster above.
        Rectangle {
            objectName: "streamingP2MainPanel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            radius: 8
            color: "#0B1824" // InputGainEditor mainPanel
            border.width: 1
            border.color: "#1E354A"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 9
                spacing: 3 + 9 * root.verticalSpaceFactor

                // Input Gain's large centered numeric readout ABOVE its
                // full-width horizontal slider, repeated twice, not two
                // small knobs/side-by-side compact rows. Exact 36px Consolas
                // Bold, 640px maximum track, 8px track and 18px thumb.
                ColumnLayout {
                    objectName: "streamingP2CeilingGroup"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 640
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 1 + 5 * root.verticalSpaceFactor

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 41 + 9 * root.verticalSpaceFactor
                        Text {
                            text: "CEILING"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        Text {
                            objectName: "streamingP2CeilingDisplay"
                            anchors.centerIn: parent
                            text: root.ceiling.toFixed(1) + " dBTP"
                            color: root.ceiling === -1.0 ? "#F5F8FC" : root.copper
                            font.family: "Consolas"
                            font.pixelSize: 36
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 13
                        Text { text: "−12.0 dBTP"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                        Item { Layout.fillWidth: true }
                        Text { text: "−6.0 dBTP"; color: "#A1B5C9"; font.family: "Consolas"; font.pixelSize: 11; font.weight: Font.Bold }
                        Item { Layout.fillWidth: true }
                        Text { text: "0.0 dBTP"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                    }
                    StudioParameterSlider {
                        objectName: "streamingP2CeilingSlider"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        Layout.minimumHeight: 24
                        compact: false // Input Gain 8px track / 18px thumb, even at 1184x688
                        from: -12; to: 0; stepSize: 0.1
                        wheelEnabled: true
                        enabled: !!root.limiterModule
                        // M17 ceiling convention: colored extent begins at
                        // 0 dBTP (right endpoint), moves LEFT to the selected
                        // negative ceiling. Never fill from -12 to value.
                        fillFromOrigin: true
                        positionFillOrigin: 0
                        positionFillColor: Qt.alpha(root.copper, 0.38)
                        accentColor: root.copper
                        value: root.ceiling
                        accessibleName: "TP Limiter Ceiling slider"
                        interactionHint: "Mouse wheel: 0.1 dBTP per detent; drag and release to apply."
                        onCommitRequested: function(v) {
                            if (root.limiterModule) root.limiterModule.setCeilingDbTp(v)
                        }
                    }
                    RowLayout {
                        objectName: "streamingP2CeilingNumericRow"
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12
                        Text {
                            text: "CEILING"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Rectangle {
                            objectName: "streamingP2CeilingNumericWell"
                            width: 120; height: 32; radius: 4
                            color: "#0E1F2E"
                            border.width: ceilingEntry.activeFocus ? 2 : 1
                            border.color: root.limiterModule && root.limiterModule.ceilingValidationMessage
                                ? "#F27683" : ceilingEntry.activeFocus ? root.copper : "#2C5A78"
                            TextInput {
                                id: ceilingEntry
                                property bool numericDirty: false
                                onTextEdited: numericDirty = true
                                objectName: "streamingP2CeilingNumericInput"
                                ToolTip.text: "Set the streaming limiter ceiling in dBTP. Enter or click away to apply; Esc to cancel."
                                ToolTip.visible: streamingP2CeilingNumericInputDwell.ready
                                ToolTip.delay: 0
                                StudioTooltipDwell {
                                    id: streamingP2CeilingNumericInputDwell
                                    anchors.fill: parent
                                    interacting: parent.activeFocus
                                }
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                verticalAlignment: TextInput.AlignVCenter
                                horizontalAlignment: TextInput.AlignRight
                                text: "-1.0"
                                font.family: "Consolas"
                                font.pixelSize: 16
                                color: "#F5F8FC"
                                selectByMouse: true
                                selectionColor: "#40C4774A"
                                selectedTextColor: "#F5F8FC"
                                activeFocusOnTab: true
                                // Synchronize EXTERNAL slider/default edits
                                // without overwriting a live numeric draft
                                // during focus loss (commit runs on blur).
                                Connections {
                                    target: root
                                    function onCeilingChanged() {
                                        if(!ceilingEntry.activeFocus && !ceilingEntry.numericDirty)
                                            ceilingEntry.text=root.ceiling.toFixed(1)
                                    }
                                }
                                Component.onCompleted: text=root.ceiling.toFixed(1)
                                Keys.onReturnPressed: function(event) {
                                    if (root.commitNumeric(ceilingEntry,-12,0,
                                        root.ceiling,function(v) {
                                            root.limiterModule.setCeilingDbTp(v)
                                        })) root.forceActiveFocus(Qt.OtherFocusReason)
                                    event.accepted=true
                                }
                                Keys.onEnterPressed: function(event) {
                                    if (root.commitNumeric(ceilingEntry,-12,0,
                                        root.ceiling,function(v) {
                                            root.limiterModule.setCeilingDbTp(v)
                                        })) root.forceActiveFocus(Qt.OtherFocusReason)
                                    event.accepted=true
                                }
                                Keys.onEscapePressed: function(event) {
                                    root.cancelNumeric(ceilingEntry, "Ceiling", root.ceiling)
                                    ceilingEntry.focus=false
                                    event.accepted=true
                                }
                                onActiveFocusChanged: if(!activeFocus)
                                    root.commitNumeric(ceilingEntry,-12,0,
                                        root.ceiling,function(v) {
                                            root.limiterModule.setCeilingDbTp(v)
                                        })
                                Accessible.name: "Streaming TP Ceiling numeric dBTP"
                            }
                        }
                        Text {
                            text: "dBTP"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                        }
                    }
                }

                ColumnLayout {
                    objectName: "streamingP2DriveGroup"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 640
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 1 + 5 * root.verticalSpaceFactor

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 41 + 9 * root.verticalSpaceFactor
                        Text {
                            text: "DRIVE"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        Text {
                            objectName: "streamingP2DriveDisplay"
                            anchors.centerIn: parent
                            text: (root.drive > 0 ? "+" : "") + root.drive.toFixed(1) + " dB"
                            color: root.drive === 0.0 ? "#F5F8FC" : root.copper
                            font.family: "Consolas"
                            font.pixelSize: 36
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 13
                        Text { text: "−24.0 dB"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                        Item { Layout.fillWidth: true }
                        Text { text: "0.0 dB"; color: root.drive === 0.0 ? root.copper : "#A1B5C9"; font.family: "Consolas"; font.pixelSize: 11; font.weight: Font.Bold }
                        Item { Layout.fillWidth: true }
                        Text { text: "+24.0 dB"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                    }
                    StudioParameterSlider {
                        objectName: "streamingP2DriveSlider"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        Layout.minimumHeight: 24
                        compact: false // Input Gain 8px track / 18px thumb
                        from: -24; to: 24; stepSize: 0.1
                        wheelEnabled: true
                        enabled: !!root.limiterModule
                        fillFromOrigin: true
                        positionFillOrigin: 0
                        showOriginTick: true
                        accentColor: root.copper
                        value: root.drive
                        accessibleName: "TP Limiter Drive slider"
                        interactionHint: "Pre-limiter gain. Mouse wheel: 0.1 dB per detent."
                        onCommitRequested: function(v) {
                            if (root.limiterModule) root.limiterModule.setDriveDb(v)
                        }
                    }
                    RowLayout {
                        objectName: "streamingP2DriveNumericRow"
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12
                        Text {
                            text: "DRIVE"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Rectangle {
                            objectName: "streamingP2DriveNumericWell"
                            width: 120; height: 32; radius: 4
                            color: "#0E1F2E"
                            border.width: driveEntry.activeFocus ? 2 : 1
                            border.color: root.limiterModule && root.limiterModule.driveValidationMessage
                                ? "#F27683" : driveEntry.activeFocus ? root.copper : "#2C5A78"
                            TextInput {
                                id: driveEntry
                                property bool numericDirty: false
                                onTextEdited: numericDirty = true
                                objectName: "streamingP2DriveNumericInput"
                                ToolTip.text: "Set the gain before the streaming limiter in dB. Enter or click away to apply; Esc to cancel."
                                ToolTip.visible: streamingP2DriveNumericInputDwell.ready
                                ToolTip.delay: 0
                                StudioTooltipDwell {
                                    id: streamingP2DriveNumericInputDwell
                                    anchors.fill: parent
                                    interacting: parent.activeFocus
                                }
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                verticalAlignment: TextInput.AlignVCenter
                                horizontalAlignment: TextInput.AlignRight
                                text: "0.0"
                                font.family: "Consolas"
                                font.pixelSize: 16
                                color: "#F5F8FC"
                                selectByMouse: true
                                selectionColor: "#40C4774A"
                                selectedTextColor: "#F5F8FC"
                                activeFocusOnTab: true
                                // Synchronize EXTERNAL slider/default edits
                                // without overwriting a live numeric draft
                                // during focus loss (commit runs on blur).
                                Connections {
                                    target: root
                                    function onDriveChanged() {
                                        if(!driveEntry.activeFocus && !driveEntry.numericDirty)
                                            driveEntry.text=root.drive.toFixed(1)
                                    }
                                }
                                Component.onCompleted: text=root.drive.toFixed(1)
                                Keys.onReturnPressed: function(event) {
                                    if (root.commitNumeric(driveEntry,-24,24,
                                        root.drive,function(v) {
                                            root.limiterModule.setDriveDb(v)
                                        })) root.forceActiveFocus(Qt.OtherFocusReason)
                                    event.accepted=true
                                }
                                Keys.onEnterPressed: function(event) {
                                    if (root.commitNumeric(driveEntry,-24,24,
                                        root.drive,function(v) {
                                            root.limiterModule.setDriveDb(v)
                                        })) root.forceActiveFocus(Qt.OtherFocusReason)
                                    event.accepted=true
                                }
                                Keys.onEscapePressed: function(event) {
                                    root.cancelNumeric(driveEntry, "Drive", root.drive)
                                    driveEntry.focus=false
                                    event.accepted=true
                                }
                                onActiveFocusChanged: if(!activeFocus)
                                    root.commitNumeric(driveEntry,-24,24,
                                        root.drive,function(v) {
                                            root.limiterModule.setDriveDb(v)
                                        })
                                Accessible.name: "Streaming TP Drive numeric dB"
                            }
                        }
                        Text {
                            text: "dB"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                        }
                    }
                }


            }
        }
    }
}
