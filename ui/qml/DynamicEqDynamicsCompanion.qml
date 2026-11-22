import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Detached research-only editor: the source EQ graph and Spectrum stay intact.
// Window chrome intentionally matches the WAV Export popup's palette, edge,
// copper top arc, title typography, and close action.
Window {
    id: root
    objectName: "dynamicEqDynamicsCompanion"
    property var dynamicsViewModel: null
    property var bandViewModel: null
    readonly property color familyAccent: "#C4774A"
    readonly property string selectedBandLabel: bandViewModel
        && bandViewModel.bandCount > 0
        ? "BAND " + (bandViewModel.selectedIndex + 1)
          + " · " + bandViewModel.filter.replace("_", " ")
        : "NO BAND SELECTED"
    width: 1050
    height: 196
    minimumWidth: 980
    maximumWidth: 1280
    minimumHeight: 196
    maximumHeight: 196
    color: "#0B1724"
    title: "Dynamic EQ · Dynamics — RGS MasterLab"
    flags: Qt.Tool

    Rectangle {
        id: panel
        objectName: "dynamicEqDynamicsWavFamilyPanel"
        anchors.fill: parent
        anchors.margins: 6
        radius: 7
        color: "#101F30"
        border.color: "#27465F"
        border.width: 1

        // Same rounded top accent rule as WAV Export, including HiDPI ends.
        Canvas {
            id: copperTopArc
            objectName: "dynamicEqDynamicsCopperArc"
            anchors.fill: parent
            onPaint: {
                const c = getContext("2d")
                c.clearRect(0, 0, width, height)
                c.beginPath()
                c.moveTo(1, 7)
                c.quadraticCurveTo(1, 1, 7, 1)
                c.lineTo(width - 7, 1)
                c.quadraticCurveTo(width - 1, 1, width - 1, 7)
                c.lineWidth = 2
                c.strokeStyle = root.familyAccent.toString()
                c.stroke()
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                Text {
                    objectName: "dynamicEqDynamicsPanelTitle"
                    text: "DYNAMIC EQ · DYNAMICS"
                    color: "#F0F6FB"
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                Item { Layout.fillWidth: true }
                Button {
                    objectName: "dynamicEqDynamicsCloseButton"
                    text: "×"
                    implicitWidth: 25
                    implicitHeight: 22
                    padding: 0
                    onClicked: root.close()
                    Accessible.name: "Close Dynamic EQ dynamics controls"
                    contentItem: Text {
                        text: "×"
                        color: "#DDE6F3"
                        font.family: "Segoe UI"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: "#142B3C"
                        radius: 4
                        border.color: "#33526B"
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 16
                Text {
                    objectName: "dynamicEqDynamicsSelectedBand"
                    text: root.selectedBandLabel
                    color: "#D1E0EB"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
                Item { Layout.fillWidth: true }
                Text {
                    objectName: "dynamicEqControlsReadOnlyStatus"
                    text: root.dynamicsViewModel && root.dynamicsViewModel.manualEditable
                        ? "MANUAL EDIT" : "READ ONLY"
                    color: root.dynamicsViewModel && root.dynamicsViewModel.manualEditable
                        ? "#B6D6CA" : "#92A9BB"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#27465F"
            }
            DynamicEqDynamicsStrip {
                objectName: "dynamicEqDynamicsStrip"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 112
                dynamicsViewModel: root.dynamicsViewModel
            }
        }
    }
}
