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

    // Generic active row lookup for host Escape/focus management
    readonly property Item activeRow: {
        var idx = root.adapterModel ? root.adapterModel.selectedIndex : root.selectedIndex
        if (idx >= 0 && idx < rowRepeater.count) {
            return rowRepeater.itemAt(idx)
        }
        return null
    }

    // Explicit focus targets retained for existing smoke assertions
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
                model: root.adapterModel ? root.adapterModel.modules : null

                delegate: Rectangle {
                    id: rowItem
                    objectName: "dspChainRow_" + index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    radius: 4

                    readonly property var moduleAdapter: modelData
                    readonly property bool isSelected: root.adapterModel ? (root.adapterModel.selectedInstanceId === (moduleAdapter ? moduleAdapter.instanceId : "")) : (root.selectedIndex === index)
                    readonly property bool isDefaultState: moduleAdapter ? (moduleAdapter.configurationState === "Default") : true
                    readonly property bool isBypassed: moduleAdapter ? moduleAdapter.bypass : false
                    readonly property bool hasError: moduleAdapter ? moduleAdapter.hasError : false

                    color: isSelected ? "#1A324A" : (rowMouse.containsMouse ? "#122538" : "#0D1D2B")
                    border.color: isSelected ? "#00C8FF" : "#1A324A"
                    border.width: 1

                    activeFocusOnTab: true

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (root.adapterModel && moduleAdapter) {
                                root.adapterModel.selectModuleByInstanceId(moduleAdapter.instanceId)
                            } else {
                                root.selectedIndex = index
                            }
                            rowItem.forceActiveFocus()
                        }
                    }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                            if (root.adapterModel && moduleAdapter) {
                                root.adapterModel.selectModuleByInstanceId(moduleAdapter.instanceId)
                            } else {
                                root.selectedIndex = index
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_Down) {
                            if (index + 1 < rowRepeater.count) {
                                var nextItem = rowRepeater.itemAt(index + 1)
                                if (nextItem && nextItem.moduleAdapter && root.adapterModel) {
                                    root.adapterModel.selectModuleByInstanceId(nextItem.moduleAdapter.instanceId)
                                } else {
                                    root.selectedIndex = index + 1
                                }
                                if (nextItem) nextItem.forceActiveFocus()
                                event.accepted = true
                            }
                        } else if (event.key === Qt.Key_Up) {
                            if (index > 0) {
                                var prevItem = rowRepeater.itemAt(index - 1)
                                if (prevItem && prevItem.moduleAdapter && root.adapterModel) {
                                    root.adapterModel.selectModuleByInstanceId(prevItem.moduleAdapter.instanceId)
                                } else {
                                    root.selectedIndex = index - 1
                                }
                                if (prevItem) prevItem.forceActiveFocus()
                                event.accepted = true
                            }
                        }
                    }

                    // Generic Family Accent Strip (Left edge secondary identity)
                    Rectangle {
                        id: accentStrip
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 3
                        radius: 1
                        color: rowItem.moduleAdapter ? rowItem.moduleAdapter.familyAccent : "#00C8FF"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 6

                        // Configuration Indicator LED
                        Rectangle {
                            objectName: "dspChainConfigLed_" + index
                            Layout.minimumWidth: 8
                            Layout.preferredWidth: 8
                            Layout.maximumWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: rowItem.isDefaultState ? "#273A4D" : "#00D47A"
                            border.color: rowItem.isDefaultState ? "#3A4D60" : "#00FF94"
                            border.width: 1

                            ToolTip.text: rowItem.isDefaultState
                                ? (rowItem.moduleAdapter ? (rowItem.moduleAdapter.displayName + " Default") : "Default")
                                : (rowItem.moduleAdapter ? ("Manual Non-Default " + rowItem.moduleAdapter.displayName) : "Manual Non-Default")
                            ToolTip.visible: ledMouse.containsMouse

                            MouseArea {
                                id: ledMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 2

                            Text {
                                objectName: "dspChainModuleTitle_" + index
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.alignment: Qt.AlignLeft
                                text: rowItem.moduleAdapter ? rowItem.moduleAdapter.displayName : ""
                                color: rowItem.isSelected ? "#F5F8FC" : "#C4D4E0"
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                objectName: "dspChainStateText_" + index
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.alignment: Qt.AlignLeft
                                elide: Text.ElideRight
                                text: rowItem.moduleAdapter ? rowItem.moduleAdapter.stateText : ""
                                color: "#7A8E9E"
                                font.family: "Segoe UI"
                                font.pixelSize: 9
                                visible: true
                            }
                        }

                        // Bypass Badge ("BYP") fixed width container to prevent title shift (UI-MINOR-03)
                        Item {
                            Layout.minimumWidth: 28
                            Layout.preferredWidth: 28
                            Layout.maximumWidth: 28
                            Layout.preferredHeight: 16

                            Rectangle {
                                objectName: "dspChainBypassBadge_" + index
                                visible: rowItem.isBypassed
                                anchors.fill: parent
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
                        }

                        // Error indicator reserves its slot even when hidden.
                        // FIX-UI-003: state/badge changes cannot move the title.
                        Item {
                            Layout.minimumWidth: 8
                            Layout.preferredWidth: 8
                            Layout.maximumWidth: 8
                            Layout.preferredHeight: 8

                            Rectangle {
                                objectName: "dspChainErrorBadge_" + index
                                anchors.fill: parent
                                visible: rowItem.hasError
                                radius: 4
                                color: "#F27683"
                            }
                        }
                    }
                }
            }
        }
    }
}
