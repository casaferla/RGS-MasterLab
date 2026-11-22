import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "transientShaperEditor"
    property var transientModule: null
    readonly property color copper: "#C4774A"
    // Isolated post-L3 editor candidate. No live Product binding at this stage.
    // Future adapter explicitly opts in to manual control; AutoMatch stays sole
    // parameter authority throughout L3.
    readonly property bool editorEnabled: !!transientModule
        && transientModule.manualEditable === true

    // A real render-sidecar / playback owner must supply these values.
    // Empty/mismatched identities show an UNAVAILABLE graph, never fake motion.
    property var activityHistory: []
    property bool activityTelemetryAvailable: false
    property string telemetryRealizationId: ""
    property string audibleRealizationId: ""
    property real audibleFrame: -1
    property int activitySampleRateHz: 48000
    property bool auditioningProcessed: false
    property bool shaperBypassed: false
    implicitWidth: 760
    implicitHeight: 570
    // Preserve P2's top-left-anchored geometry and control interaction.
    // The activity plot below uses only remaining vertical space.
    readonly property real verticalSpaceFactor:
        Math.max(0, Math.min(1, (root.height - 480) / 220))
    readonly property double attackDb: transientModule ? transientModule.attackAmountDb : 0.0
    readonly property double sustainDb: transientModule ? transientModule.sustainAmountDb : 0.0
    // Numeric drafts never trigger DSP work on each keystroke. Same 120x32,
    // Consolas 16, GAIN-field visual grammar as InputGainEditor.
    function commitNumeric(control, low, high, current, setter) {
        if(!root.editorEnabled) return
        const parsed=Number(control.text.trim().replace(",","."))
        if(!Number.isFinite(parsed) || parsed<low || parsed>high) {
            control.text=current.toFixed(1)
            return
        }
        const canonical=Math.round(parsed*10)/10
        if(canonical!==current) setter(canonical)
        control.text=canonical.toFixed(1)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 5
        // Reset belongs to the outer action bar, as in Input Gain and
        // other DSP editors, NEVER buried in the main parameter well.
        RowLayout {
            objectName: "transientShaperActionBar"
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            spacing: 8
            Item { Layout.fillWidth: true }
            StudioButton {
                objectName: "transientShaperResetButton"
                text: "Reset to Default"
                minimumControlWidth: 130
                accentColor: root.copper
                enabled: root.editorEnabled
                onClicked: if(root.transientModule) root.transientModule.resetToDefault()
                Accessible.name: "Reset Transient Shaper to defaults"
            }
        }

        Rectangle {
            objectName: "transientShaperMainPanel"
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
                spacing: 2 + 5 * root.verticalSpaceFactor

                // Input Gain's large centered numeric readout ABOVE its
                // full-width horizontal slider, repeated twice, not two
                // small knobs/side-by-side compact rows. Exact 36px Consolas
                // Bold, 640px maximum track, 8px track and 18px thumb.
                ColumnLayout {
                    objectName: "transientShaperAttackGroup"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 640
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 1 + 5 * root.verticalSpaceFactor

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 41 + 9 * root.verticalSpaceFactor
                        Text {
                            text: "ATTACK"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        Text {
                            objectName: "transientShaperAttackDisplay"
                            anchors.centerIn: parent
                            text: (root.attackDb > 0 ? "+" : "") + root.attackDb.toFixed(1) + " dB"
                            color: root.attackDb === 0.0 ? "#F5F8FC" : root.copper
                            font.family: "Consolas"
                            font.pixelSize: 36
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 13
                        Text { text: "−12.0 dB"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                        Item { Layout.fillWidth: true }
                        Text { text: "0.0 dB"; color: root.attackDb === 0.0 ? root.copper : "#A1B5C9"; font.family: "Consolas"; font.pixelSize: 11; font.weight: Font.Bold }
                        Item { Layout.fillWidth: true }
                        Text { text: "+12.0 dB"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                    }
                    StudioParameterSlider {
                        objectName: "transientShaperAttackSlider"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        Layout.minimumHeight: 24
                        compact: false // Input Gain 8px track / 18px thumb, even at 1184x688
                        from: -12; to: 12; stepSize: 0.1
                        wheelEnabled: true
                        enabled: root.editorEnabled
                        // Signed Attack fill starts at neutral 0 dB.
                        fillFromOrigin: true
                        positionFillOrigin: 0
                        showOriginTick: true
                        positionFillColor: Qt.alpha(root.copper, 0.38)
                        accentColor: root.copper
                        value: root.attackDb
                        accessibleName: "Transient Shaper Attack slider"
                        interactionHint: "Attack amount. Mouse wheel: 0.1 dB per detent; drag and release to apply."
                        onCommitRequested: function(v) {
                            if (root.transientModule) root.transientModule.setAttackAmountDb(v)
                        }
                    }
                    RowLayout {
                        objectName: "transientShaperAttackNumericRow"
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12
                        Text {
                            text: "ATTACK"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Rectangle {
                            objectName: "transientShaperAttackNumericWell"
                            width: 120; height: 32; radius: 4
                            color: "#0E1F2E"
                            border.width: attackEntry.activeFocus ? 2 : 1
                            border.color: attackEntry.activeFocus
                                ? root.copper : "#2C5A78"
                            TextInput {
                                id: attackEntry
                                enabled: root.editorEnabled
                                objectName: "transientShaperAttackNumericInput"
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
                                    function onAttackDbChanged() {
                                        if(!attackEntry.activeFocus)
                                            attackEntry.text=root.attackDb.toFixed(1)
                                    }
                                    function onEditorEnabledChanged() {
                                        // A revoked edit gate invalidates an unfinished draft,
                                        // even if the text field still has keyboard focus.
                                        if(!root.editorEnabled)
                                            attackEntry.text=root.attackDb.toFixed(1)
                                    }
                                }
                                Component.onCompleted: text=root.attackDb.toFixed(1)
                                Keys.onReturnPressed: function(event) {
                                    root.commitNumeric(attackEntry,-12,12,
                                        root.attackDb,function(v) {
                                            root.transientModule.setAttackAmountDb(v)
                                        })
                                    attackEntry.focus=false
                                    event.accepted=true
                                }
                                Keys.onEnterPressed: function(event) {
                                    root.commitNumeric(attackEntry,-12,12,
                                        root.attackDb,function(v) {
                                            root.transientModule.setAttackAmountDb(v)
                                        })
                                    attackEntry.focus=false
                                    event.accepted=true
                                }
                                Keys.onEscapePressed: function(event) {
                                    attackEntry.text=root.attackDb.toFixed(1)
                                    attackEntry.focus=false
                                    event.accepted=true
                                }
                                onActiveFocusChanged: if(!activeFocus)
                                    root.commitNumeric(attackEntry,-12,12,
                                        root.attackDb,function(v) {
                                            root.transientModule.setAttackAmountDb(v)
                                        })
                                Accessible.name: "Transient Shaper Attack numeric dB"
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

                ColumnLayout {
                    objectName: "transientShaperSustainGroup"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 640
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 1 + 5 * root.verticalSpaceFactor

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 41 + 9 * root.verticalSpaceFactor
                        Text {
                            text: "SUSTAIN"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        Text {
                            objectName: "transientShaperSustainDisplay"
                            anchors.centerIn: parent
                            text: (root.sustainDb > 0 ? "+" : "") + root.sustainDb.toFixed(1) + " dB"
                            color: root.sustainDb === 0.0 ? "#F5F8FC" : root.copper
                            font.family: "Consolas"
                            font.pixelSize: 36
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 13
                        Text { text: "−12.0 dB"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                        Item { Layout.fillWidth: true }
                        Text { text: "0.0 dB"; color: root.sustainDb === 0.0 ? root.copper : "#A1B5C9"; font.family: "Consolas"; font.pixelSize: 11; font.weight: Font.Bold }
                        Item { Layout.fillWidth: true }
                        Text { text: "+12.0 dB"; color: "#586773"; font.family: "Consolas"; font.pixelSize: 10 }
                    }
                    StudioParameterSlider {
                        objectName: "transientShaperSustainSlider"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        Layout.minimumHeight: 24
                        compact: false // Input Gain 8px track / 18px thumb
                        from: -12; to: 12; stepSize: 0.1
                        wheelEnabled: true
                        enabled: root.editorEnabled
                        fillFromOrigin: true
                        positionFillOrigin: 0
                        showOriginTick: true
                        accentColor: root.copper
                        value: root.sustainDb
                        accessibleName: "Transient Shaper Sustain slider"
                        interactionHint: "Sustain amount. Mouse wheel: 0.1 dB per detent."
                        onCommitRequested: function(v) {
                            if (root.transientModule) root.transientModule.setSustainAmountDb(v)
                        }
                    }
                    RowLayout {
                        objectName: "transientShaperSustainNumericRow"
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12
                        Text {
                            text: "SUSTAIN"
                            color: "#A1B5C9"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Rectangle {
                            objectName: "transientShaperSustainNumericWell"
                            width: 120; height: 32; radius: 4
                            color: "#0E1F2E"
                            border.width: sustainEntry.activeFocus ? 2 : 1
                            border.color: sustainEntry.activeFocus
                                ? root.copper : "#2C5A78"
                            TextInput {
                                id: sustainEntry
                                enabled: root.editorEnabled
                                objectName: "transientShaperSustainNumericInput"
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
                                    function onSustainDbChanged() {
                                        if(!sustainEntry.activeFocus)
                                            sustainEntry.text=root.sustainDb.toFixed(1)
                                    }
                                    function onEditorEnabledChanged() {
                                        if(!root.editorEnabled)
                                            sustainEntry.text=root.sustainDb.toFixed(1)
                                    }
                                }
                                Component.onCompleted: text=root.sustainDb.toFixed(1)
                                Keys.onReturnPressed: function(event) {
                                    root.commitNumeric(sustainEntry,-12,12,
                                        root.sustainDb,function(v) {
                                            root.transientModule.setSustainAmountDb(v)
                                        })
                                    sustainEntry.focus=false
                                    event.accepted=true
                                }
                                Keys.onEnterPressed: function(event) {
                                    root.commitNumeric(sustainEntry,-12,12,
                                        root.sustainDb,function(v) {
                                            root.transientModule.setSustainAmountDb(v)
                                        })
                                    sustainEntry.focus=false
                                    event.accepted=true
                                }
                                Keys.onEscapePressed: function(event) {
                                    sustainEntry.text=root.sustainDb.toFixed(1)
                                    sustainEntry.focus=false
                                    event.accepted=true
                                }
                                onActiveFocusChanged: if(!activeFocus)
                                    root.commitNumeric(sustainEntry,-12,12,
                                        root.sustainDb,function(v) {
                                            root.transientModule.setSustainAmountDb(v)
                                        })
                                Accessible.name: "Transient Shaper Sustain numeric dB"
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

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 135
                    Layout.preferredHeight: 200
                    radius: 6
                    color: "#081824"
                    border.width: 1
                    border.color: "#1A3E55"

                    TransientActivityScope {
                        objectName: "transientShaperActivityScope"
                        anchors.fill: parent
                        anchors.margins: 5
                        history: root.activityHistory
                        telemetryAvailable: root.activityTelemetryAvailable
                        telemetryRealizationId: root.telemetryRealizationId
                        audibleRealizationId: root.audibleRealizationId
                        audibleFrame: root.audibleFrame
                        sampleRateHz: root.activitySampleRateHz
                        auditioningProcessed: root.auditioningProcessed
                        bypassed: root.shaperBypassed
                    }
                }
            }
        }
    }
}
