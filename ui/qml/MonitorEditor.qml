import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One independently authored MONITOR editor, selectable from the same
// adaptive workspace as the mastering DSP editors. MONITOR is a SERVICE:
// it owns no ProcessingChain node, True Peak Limiter bypass or mastered PCM.
// Window docking is provided by the generic host, never by this editor.
Item {
    id: root
    objectName: "masterMonitorEditor"
    property var monitorViewModel: null
    implicitWidth: 610
    implicitHeight: 460

    ScrollView {
        id: monitorScroll
        anchors.fill: parent
        clip: true
        implicitWidth: monitorForm.implicitWidth
        implicitHeight: monitorForm.implicitHeight
        contentWidth: Math.max(580,availableWidth)
        contentHeight: monitorForm.implicitHeight
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
        ColumnLayout {
            id: monitorForm
            width: Math.max(580,monitorScroll.availableWidth)
            spacing: 8
        Text {
            Layout.fillWidth: true
            text: "EXPERIMENTAL MASTER / DEVICE MONITOR · NOT M16 LIVE"
            color: "#E5EEF4"
            font.pixelSize: 12
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Text {text: "MASTER Hz"; color: "#B8CCD8"; font.pixelSize: 11}
            ComboBox {
                id: masterRate
                objectName: "monitorMasterRateSelector"
                model: ["44100","48000","88200","96000","176400","192000"]
                currentIndex: 1
                Layout.preferredWidth: 108
                enabled: root.monitorViewModel && root.monitorViewModel.canPrepare
            }
            StudioButton {
                objectName: "monitorPrepareMaster"
                text: "Prepare MASTER"
                enabled: root.monitorViewModel && root.monitorViewModel.canPrepare
                onClicked: {
                    if (root.monitorViewModel.requestMaster(Number(masterRate.currentText)))
                        allowLimited.checked = false
                }
            }
            Item {Layout.fillWidth: true}
            StudioButton {
                objectName: "monitorReturnManual"
                text: root.monitorViewModel && root.monitorViewModel.monitorInstalled
                    ? "Return MANUAL" : "Cancel"
                enabled: root.monitorViewModel && root.monitorViewModel.canDismiss
                onClicked: root.monitorViewModel.cancel()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            CheckBox {
                id: allowLimited
                objectName: "monitorCautionConsent"
                text: "Permit limited headroom"
                checked: false
                enabled: root.monitorViewModel && root.monitorViewModel.canInstall
            }
            StudioButton {
                objectName: "monitorAllocateAndInstall"
                text: "Allocate DEVICE / Monitor"
                enabled: root.monitorViewModel && root.monitorViewModel.canInstall
                onClicked: root.monitorViewModel.installMonitor(allowLimited.checked)
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.monitorViewModel ? root.monitorViewModel.statusText : "UNAVAILABLE"
            color: "#90D2E1"
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
        Text {
            Layout.fillWidth: true
            text: root.monitorViewModel ? root.monitorViewModel.errorText : ""
            visible: text.length > 0
            color: "#F27683"
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
        Text {
            Layout.fillWidth: true
            text: root.monitorViewModel
                ? ("MASTER " + root.monitorViewModel.masterRateHz
                    + " Hz / DEVICE " + root.monitorViewModel.deviceRateHz
                    + " Hz · " + root.monitorViewModel.transportState
                    + " · MASTER frame " + root.monitorViewModel.masterPositionFrames
                    + " / " + root.monitorViewModel.masterDurationFrames)
                : "MASTER and DEVICE clocks unavailable"
            color: "#D0E0EA"
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            StudioButton {
                objectName: "monitorPlay"
                text: "Play"
                enabled: root.monitorViewModel && root.monitorViewModel.monitorInstalled
                onClicked: root.monitorViewModel.play()
            }
            StudioButton {
                objectName: "monitorPause"
                text: "Pause"
                enabled: root.monitorViewModel && root.monitorViewModel.monitorInstalled
                onClicked: root.monitorViewModel.pause()
            }
            StudioButton {
                objectName: "monitorStop"
                text: "Stop"
                enabled: root.monitorViewModel && root.monitorViewModel.monitorInstalled
                onClicked: root.monitorViewModel.stop()
            }
            TextField {
                id: seekFrame
                objectName: "monitorSeekMasterFrame"
                placeholderText: "MASTER frame"
                inputMethodHints: Qt.ImhDigitsOnly
                Layout.fillWidth: true
            }
            StudioButton {
                objectName: "monitorSeekCommit"
                text: "Seek"
                enabled: root.monitorViewModel && root.monitorViewModel.monitorInstalled
                onClicked: {
                    const pos = Number(seekFrame.text)
                    if (Number.isSafeInteger(pos) && pos >= 0)
                        root.monitorViewModel.seekMaster(pos)
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            TextField {
                id: loopStart
                objectName: "monitorLoopStartFrame"
                placeholderText: "Loop start (MASTER)"
                inputMethodHints: Qt.ImhDigitsOnly
                Layout.fillWidth: true
            }
            TextField {
                id: loopEnd
                objectName: "monitorLoopEndFrame"
                placeholderText: "Loop end exclusive"
                inputMethodHints: Qt.ImhDigitsOnly
                Layout.fillWidth: true
            }
            StudioButton {
                objectName: "monitorSetLoop"
                text: "Loop"
                enabled: root.monitorViewModel && root.monitorViewModel.monitorInstalled
                onClicked: {
                    const a = Number(loopStart.text)
                    const b = Number(loopEnd.text)
                    if (Number.isSafeInteger(a) && Number.isSafeInteger(b)
                            && a >= 0 && b > a)
                        root.monitorViewModel.setMasterLoop(a,b,true)
                }
            }
            StudioButton {
                objectName: "monitorClearLoop"
                text: "No loop"
                enabled: root.monitorViewModel && root.monitorViewModel.monitorInstalled
                onClicked: root.monitorViewModel.setMasterLoop(0,0,false)
            }
        }
        Text {
            Layout.fillWidth: true
            text: "Device SRC: experimental full-track RAM; CPU/DAC not qualified. "
                + "Normal SOURCE transport is locked during MONITOR."
            color: "#92A5B5"
            font.pixelSize: 10
            wrapMode: Text.Wrap
        }
        }
    }
}