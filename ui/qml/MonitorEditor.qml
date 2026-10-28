import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// MASTER / DEVICE monitoring is a workflow utility, not an audio DSP.
// One editor instance belongs to the common dock/undock host. All shown
// rates, clocks and admission states come from the typed monitor ViewModel.
Item {
    id: root
    objectName: "masterMonitorEditor"
    property var monitorViewModel: null
    // Shared product PROJECT rate, not a separate DEVICE-rate preference.
    property var masterRateState: null
    property bool advancedNavigationExpanded: false
    readonly property color slate: "#718092"
    readonly property color fg: "#D5E2EC"
    readonly property color secondary: "#98AFC0"
    readonly property bool active: !!monitorViewModel && monitorViewModel.monitorInstalled
    readonly property bool controlsAvailable: !!monitorViewModel
        && monitorViewModel.monitorControllable
    readonly property bool ready: !!monitorViewModel && monitorViewModel.canInstall
    implicitWidth: 610
    implicitHeight: 460

    ColumnLayout {
        id: page
        anchors.fill: parent
        spacing: root.height < 420 ? 3 : 6

            // Title/identity shares the other DSP editors' 11px section
            // grammar, dark wells and restrained family accent.
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: root.height < 420 ? 20 : 24
                spacing: 8
                Text {
                    text: "Master Audition"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    color: root.secondary
                }
                Rectangle {
                    Layout.preferredWidth: 2
                    Layout.preferredHeight: 15
                    color: root.slate
                }
                Text {
                    text: "LISTENING ONLY"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    color: root.secondary
                }
                Item { Layout.fillWidth: true }
                Text {
                    objectName: "monitorStateHeadline"
                    text: root.monitorViewModel
                        ? root.monitorViewModel.transportState : "UNAVAILABLE"
                    color: root.active ? "#B7D6DB" : root.secondary
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
            }

            GridLayout {
                objectName: "monitorMasterDevicePair"
                Layout.fillWidth: true
                // PO: stable geometry at EVERY window height/width.
                // No responsive two-to-one-column jump at minimum size:
                // each card keeps its own logical column and shrinks only
                // its internal free space, not its position.
                columns: 2
                columnSpacing: 8
                rowSpacing: 8

                Rectangle {
                    objectName: "monitorMasterCard"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: root.height < 420 ? 110 : 132
                    radius: 6
                    color: "#0A1C2D"
                    border.color: "#31506B"
                    border.width: 1
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 4
                        Text {
                            text: "01  FINAL MASTER"
                            color: root.secondary
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Text {
                                text: "MASTER RATE"
                                color: root.secondary
                                font.pixelSize: 10
                            }
                            Item { Layout.fillWidth: true }
                            ComboBox {
                                id: masterRate
                                objectName: "monitorMasterRateSelector"
                                model: ["44100", "48000", "88200", "96000", "176400", "192000"]
                                readonly property var supportedRates: [44100,48000,88200,96000,176400,192000]
                                currentIndex: root.masterRateState
                                    ? supportedRates.indexOf(root.masterRateState.masterRateHz) : -1
                                Layout.preferredWidth: 108
                                enabled: !!root.monitorViewModel
                                    && root.monitorViewModel.canPrepare
                                    && !!root.masterRateState
                                    && root.masterRateState.hasSupportedMasterRate
                                onActivated: function(index) {
                                    if (root.masterRateState
                                        && root.monitorViewModel
                                        && root.monitorViewModel.canPrepare)
                                        root.masterRateState.choose_master_rate(supportedRates[index])
                                }
                                Accessible.name: "Project MASTER rate, shared with M17"
                            }
                            Text { text: "Hz"; color: root.secondary; font.pixelSize: 10 }
                        }
                        StudioButton {
                            objectName: "monitorPrepareMaster"
                            text: "Prepare MASTER"
                            Layout.fillWidth: true
                            accentColor: root.slate
                            enabled: !!root.monitorViewModel
                                && root.monitorViewModel.canPrepare
                                && !!root.masterRateState
                                && root.masterRateState.hasSupportedMasterRate
                            onClicked: {
                                // Always submit the authoritative Project rate;
                                // never use a stale local combo text or a
                                // default 48-kHz placeholder for the DAC.
                                if (root.monitorViewModel
                                    && root.masterRateState
                                    && root.masterRateState.hasSupportedMasterRate
                                    && root.monitorViewModel.requestMaster(
                                        root.masterRateState.masterRateHz))
                                    limitedConsent.checked = false
                            }
                            Accessible.name: "Prepare verified full master offline"
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.monitorViewModel
                                && root.monitorViewModel.masterRateHz > 0
                                ? "MASTER " + root.monitorViewModel.masterRateHz + " Hz · immutable"
                                : "No prepared MASTER"
                            color: root.secondary
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }
                }

                Rectangle {
                    objectName: "monitorDeviceCard"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: root.height < 420 ? 110 : 132
                    radius: 6
                    color: "#0A1C2D"
                    border.color: "#31506B"
                    border.width: 1
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 4
                        Text {
                            text: "02  DEVICE AUDITION"
                            color: root.secondary
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "DEVICE RATE"
                                color: root.secondary
                                font.pixelSize: 10
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                objectName: "monitorDeviceRate"
                                text: root.monitorViewModel && root.monitorViewModel.deviceRateHz > 0
                                    ? root.monitorViewModel.deviceRateHz + " Hz" : "UNAVAILABLE"
                                color: root.fg
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                        }
                        StudioButton {
                            objectName: "monitorAllocateAndInstall"
                            text: "Start Device Audition"
                            Layout.fillWidth: true
                            accentColor: root.slate
                            enabled: !!root.monitorViewModel
                                && root.monitorViewModel.canInstall
                            onClicked: root.monitorViewModel.installMonitor(limitedConsent.checked)
                            Accessible.name: "Allocate authorized device audition PCM"
                        }
                        CheckBox {
                            id: limitedConsent
                            objectName: "monitorCautionConsent"
                            text: "Permit limited memory headroom"
                            checked: false
                            enabled: !!root.monitorViewModel
                                && root.monitorViewModel.canInstall
                            font.pixelSize: 10
                            ToolTip.text: "Explicit consent for CAUTION only. UNKNOWN and DISCOURAGED remain refused."
                            ToolTip.visible: hovered && monitorMemoryConsentDwell.ready
                            ToolTip.delay: 0
                            StudioTooltipDwell {
                                id: monitorMemoryConsentDwell
                                anchors.fill: parent
                            }
                        }
                    }
                }
            }

            // Truthful, readable operational status; this is not a fake
            // spectral display, loudness meter or verified M16 LIVE reading.
            Rectangle {
                objectName: "monitorStatusWell"
                // Compact editor: SEEK/LOOP replaces the secondary status
                // panel while expanded. The primary MASTER/DEVICE pair is
                // never collapsed or moved. No vertical scroll is allowed.
                visible: !root.advancedNavigationExpanded || root.height >= 420
                Layout.fillWidth: true
                Layout.preferredHeight: root.height < 420 ? 42 : 52
                radius: 6
                color: "#0B1D2B"
                border.color: "#31506B"
                border.width: 1
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4
                    Text {
                        Layout.fillWidth: true
                        text: root.monitorViewModel
                            ? root.monitorViewModel.statusText : "AUDITION UNAVAILABLE"
                        color: root.fg
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.monitorViewModel
                            ? (root.monitorViewModel.errorText.length
                                ? root.monitorViewModel.errorText
                                : "No device signal becomes the FINAL MASTER or M16 LIVE.")
                            : "No authoritative monitoring owner"
                        color: root.monitorViewModel
                            && root.monitorViewModel.errorText.length
                            ? "#F27683" : root.secondary
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }

            Rectangle {
                objectName: "monitorTransportWell"
                // At the minimum supported Main height, the user's
                // explicit SEEK/LOOP expansion temporarily replaces the
                // transport actions; collapsing restores Play/Pause/Stop.
                // This keeps every *visible* interactive control inside
                // the editor, without a Flickable or hidden lower region.
                visible: !root.advancedNavigationExpanded || root.height >= 420
                Layout.fillWidth: true
                Layout.preferredHeight: root.height < 420 ? 62 : 72
                radius: 6
                color: "#0A1C2D"
                border.color: "#31506B"
                border.width: 1
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "MASTER TRANSPORT"
                            color: root.secondary
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.monitorViewModel
                                ? root.monitorViewModel.masterPositionFrames
                                  + " / " + root.monitorViewModel.masterDurationFrames
                                  + " FRAMES" : "—"
                            color: root.secondary
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        StudioButton {
                            objectName: "monitorPlay"
                            text: "Play"
                            enabled: root.controlsAvailable
                            onClicked: root.monitorViewModel.play()
                        }
                        StudioButton {
                            objectName: "monitorPause"
                            text: "Pause"
                            enabled: root.controlsAvailable
                            onClicked: root.monitorViewModel.pause()
                        }
                        StudioButton {
                            objectName: "monitorStop"
                            text: "Stop"
                            enabled: root.controlsAvailable
                            onClicked: root.monitorViewModel.stop()
                        }
                        Item { Layout.fillWidth: true }
                        StudioButton {
                            objectName: "monitorReturnManual"
                            text: root.active ? root.monitorViewModel.returnTargetText : "Cancel"
                            accentColor: root.slate
                            enabled: !!root.monitorViewModel
                                && root.monitorViewModel.canDismiss
                            onClicked: root.monitorViewModel.cancel()
                        }
                    }
                }
            }

            // Hidden by USER intent, never by a width breakpoint. The
            // two primary cards and the transport never change columns.
            RowLayout {
                objectName: "monitorAdvancedNavigationToggle"
                Layout.fillWidth: true
                Layout.preferredHeight: root.height < 420 ? 22 : 27
                StudioButton {
                    objectName: "monitorAdvancedNavigationButton"
                    text: root.advancedNavigationExpanded
                        ? "SEEK / LOOP  ▾" : "SEEK / LOOP  ▸"
                    accentColor: root.slate
                    onClicked: root.advancedNavigationExpanded
                        = !root.advancedNavigationExpanded
                    Accessible.name: "Show or hide secondary seek and loop commands"
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "Secondary · MASTER-frame coordinates"
                    color: root.secondary
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
            RowLayout {
                objectName: "monitorNavigationRow"
                visible: root.advancedNavigationExpanded
                Layout.fillWidth: true
                spacing: 5
                TextField {
                    id: seekFrame
                    objectName: "monitorSeekMasterFrame"
                    Layout.fillWidth: true
                    placeholderText: "Seek · MASTER frame"
                    font.pixelSize: 10
                    inputMethodHints: Qt.ImhDigitsOnly
                }
                StudioButton {
                    objectName: "monitorSeekCommit"
                    text: "Seek"
                    enabled: root.controlsAvailable
                    onClicked: {
                        const n=Number(seekFrame.text)
                        if(Number.isSafeInteger(n) && n>=0)root.monitorViewModel.seekMaster(n)
                    }
                }
            }
            RowLayout {
                objectName: "monitorLoopRow"
                visible: root.advancedNavigationExpanded
                Layout.fillWidth: true
                spacing: 5
                TextField {
                    id: loopStart
                    objectName: "monitorLoopStartFrame"
                    Layout.fillWidth: true
                    placeholderText: "MASTER loop from"
                    font.pixelSize: 10
                    inputMethodHints: Qt.ImhDigitsOnly
                }
                TextField {
                    id: loopEnd
                    objectName: "monitorLoopEndFrame"
                    Layout.fillWidth: true
                    placeholderText: "to (exclusive)"
                    font.pixelSize: 10
                    inputMethodHints: Qt.ImhDigitsOnly
                }
                StudioButton {
                    objectName: "monitorSetLoop"
                    text: "Loop"
                    enabled: root.controlsAvailable
                    onClicked: {
                        const a=Number(loopStart.text), b=Number(loopEnd.text)
                        if(Number.isSafeInteger(a)&&Number.isSafeInteger(b)&&a>=0&&b>a)
                            root.monitorViewModel.setMasterLoop(a,b,true)
                    }
                }
                StudioButton {
                    objectName: "monitorClearLoop"
                    text: "No loop"
                    enabled: root.controlsAvailable
                    onClicked: root.monitorViewModel.setMasterLoop(0,0,false)
                }
            }
            Text {
                objectName: "monitorAdvancedNavigationFooter"
                Layout.fillWidth: true
                visible: root.advancedNavigationExpanded
                text: "Master Audition is a listening branch. Memory and DEVICE SRC are independently checked; final MASTER PCM is unchanged."
                color: root.secondary
                font.family: "Segoe UI"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
        }
  }
