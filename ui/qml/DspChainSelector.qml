import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dspChainSelector"

    implicitWidth: 180
    implicitHeight: 300

    property var adapterModel: null
    property int selectedIndex: adapterModel ? adapterModel.selectedIndex : 0
    property var gainViewModel: null
    property var eqViewModel: null

    // Explicit focus targets for the unified editor host.
    readonly property Item inputGainRow: rowRepeater.count > 0 ? rowRepeater.itemAt(0) : null
    readonly property Item parametricEqRow: rowRepeater.count > 1 ? rowRepeater.itemAt(1) : null

    color: "#0B1622"
    border.color: "#1E354A"
    border.width: 1
    radius: 6

    // Header label
    Rectangle {
        id: chainHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 28
        color: "#0F2030"
        border.color: "#1E354A"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Text {
                text: "DSP CHAIN"
                color: "#8A9EA8"
                font.family: "Segoe UI"
                font.pixelSize: 10
                font.weight: Font.Bold
            }
        }
    }

    ScrollView {
        id: chainScrollView
        anchors.top: chainHeader.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 4
        clip: true

        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: chainScrollView.availableWidth
            spacing: 4

            Repeater {
                id: rowRepeater
                model: root.adapterModel ? root.adapterModel.modules : 2

                delegate: Rectangle {
                    id: rowItem
                    objectName: "dspChainRow_" + index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    radius: 4

                    readonly property var moduleAdapter: root.adapterModel && root.adapterModel.modules ? root.adapterModel.modules[index] : null
                    readonly property bool isSelected: root.selectedIndex === index
                    readonly property bool isDefaultState: moduleAdapter ? (moduleAdapter.configurationState === "Default") : true
                    readonly property bool isBypassed: moduleAdapter ? moduleAdapter.bypass : false
                    readonly property bool hasError: moduleAdapter ? moduleAdapter.hasError : false

                    color: isSelected ? "#1A324A" : (rowMouse.containsMouse ? "#122538" : "#0D1D2B")
                    border.color: isSelected ? "#00C8FF" : "#1A324A"
                    border.width: isSelected ? 2 : 1

                    activeFocusOnTab: true

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (root.adapterModel) {
                                root.adapterModel.setSelectedIndex(index)
                            } else {
                                root.selectedIndex = index
                            }
                            rowItem.forceActiveFocus()
                        }
                    }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                            if (root.adapterModel) {
                                root.adapterModel.setSelectedIndex(index)
                            } else {
                                root.selectedIndex = index
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_Down) {
                            if (index + 1 < rowRepeater.count) {
                                if (root.adapterModel) {
                                    root.adapterModel.setSelectedIndex(index + 1)
                                } else {
                                    root.selectedIndex = index + 1
                                }
                                var nextItem = rowRepeater.itemAt(index + 1)
                                if (nextItem) nextItem.forceActiveFocus()
                                event.accepted = true
                            }
                        } else if (event.key === Qt.Key_Up) {
                            if (index > 0) {
                                if (root.adapterModel) {
                                    root.adapterModel.setSelectedIndex(index - 1)
                                } else {
                                    root.selectedIndex = index - 1
                                }
                                var prevItem = rowRepeater.itemAt(index - 1)
                                if (prevItem) prevItem.forceActiveFocus()
                                event.accepted = true
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        // Configuration Indicator LED
                        Rectangle {
                            objectName: "dspChainConfigLed_" + index
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: rowItem.isDefaultState ? "#273A4D" : "#00D47A"
                            border.color: rowItem.isDefaultState ? "#3A4D60" : "#00FF94"
                            border.width: 1

                            ToolTip.text: rowItem.isDefaultState
                                ? (index === 0 ? "0.0 dB Default" : "Canonical Default / Flat")
                                : (index === 0 ? "Manual Non-Default Gain" : "Manual Non-Default")
                            ToolTip.visible: ledMouse.containsMouse

                            MouseArea {
                                id: ledMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: rowItem.moduleAdapter ? rowItem.moduleAdapter.displayName : (index === 0 ? "Input Gain" : "Parametric EQ")
                                color: rowItem.isSelected ? "#F5F8FC" : "#C4D4E0"
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                objectName: "dspChainStateText_" + index
                                text: rowItem.moduleAdapter ? rowItem.moduleAdapter.stateText : (index === 0 ? "0.0 dB Default" : "Flat Default")
                                color: "#7A8E9E"
                                font.family: "Segoe UI"
                                font.pixelSize: 9
                                visible: true
                            }
                        }

                        // Bypass Badge ("BYP")
                        Rectangle {
                            objectName: "dspChainBypassBadge_" + index
                            visible: rowItem.isBypassed
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 16
                            radius: 3
                            color: "#3A2A0D"
                            border.color: "#F2B632"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "BYP"
                                color: "#F2B632"
                                font.family: "Segoe UI"
                                font.pixelSize: 8
                                font.weight: Font.Bold
                            }
                        }

                        // Error Indicator
                        Rectangle {
                            objectName: "dspChainErrorBadge_" + index
                            visible: rowItem.hasError
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: "#F27683"
                        }
                    }
                }
            }
        }
    }
}
