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

    // Fit the fixed-width chain row without truncating semantic words.
    // Diagnostic/ownership detail stays in the stationary-hover tooltip;
    // this is presentation only and never changes the real M17 state.
    function readableState(moduleAdapter) {
        if (!moduleAdapter) return ""
        const state = moduleAdapter.stateText || ""
        if (moduleAdapter.editorContentKey !== "TRUE_PEAK_LIMITER")
            return state
        if (state.startsWith("NEEDS PREPARATION")) return "Needs preparation"
        if (state.startsWith("READY")) return "Ready · Preview"
        if (state.startsWith("ERROR")) return "Error · Bypass"
        if (state.startsWith("ACTIVE")) return "Limiter active"
        if (state.startsWith("Preparing ·")) return state
        if (state.startsWith("Preparing") || state.startsWith("RENDERING"))
            return "Preparing audio"
        if (state.startsWith("BYPASS")) return "Meter · Bypass"
        return state
    }

    // Preserve immutable M17 DSP identity. Explain the distinction between
    // ON intent and an authenticated, audible post-limiter installation in
    // the existing stationary-hover tooltip; do not overload the narrow row.
    function stateHint(moduleAdapter) {
        if (!moduleAdapter) return ""
        const state = moduleAdapter.stateText || ""
        if (moduleAdapter.editorContentKey !== "TRUE_PEAK_LIMITER")
            return state
        if (state.startsWith("Preparing") || state.startsWith("RENDERING"))
            return state + " · Elapsed preparation time, not a countdown. "
                + "The current Meter still observes MANUAL PROCESSED audio; "
                + "Ceiling is not yet applied to what you hear."
        if (state.startsWith("ACTIVE"))
            return state + " · Limiter output installed. Live True Peak "
                + "comes from verified post-limiter audio when available."
        if (state.startsWith("BYPASS"))
            return state + " · Meter observes MANUAL PROCESSED audio; "
                + "no Limiter processing is applied."
        return state
    }

    // M16 is an analysis-only LIVE row. M17 and future real Dither adapters
    // form a separate, anchored-bottom finalization group.
    readonly property var chainModules: adapterModel ? adapterModel.modules : []
    function isTerminal(module) {
        return !!module && (module.editorContentKey === "TRUE_PEAK_LIMITER"
            || module.editorContentKey === "DITHER"
            || module.typeId === "rgsml.dsp.dither")
    }
    // Stable numeric original-chain indices, not QVariant/QObject JS-wrapper
    // identity comparisons. Both Repeaters retain the same adapter authority.
    readonly property var liveModuleIndices: {
        var out = []
        for (var i = 0; i < root.chainModules.length; ++i)
            if (!root.isTerminal(root.chainModules[i])) out.push(i)
        return out
    }
    readonly property var terminalModuleIndices: {
        var out = []
        for (var i = 0; i < root.chainModules.length; ++i)
            if (root.isTerminal(root.chainModules[i])) out.push(i)
        return out
    }
    function rowAt(idx) {
        if (idx < 0 || idx >= root.chainModules.length) return null
        var liveIdx = root.liveModuleIndices.indexOf(idx)
        if (liveIdx >= 0) return rowRepeater.itemAt(liveIdx)
        var terminalIdx = root.terminalModuleIndices.indexOf(idx)
        return terminalIdx >= 0 ? terminalRepeater.itemAt(terminalIdx) : null
    }
    readonly property Item activeRow: root.rowAt(
        root.adapterModel ? root.adapterModel.selectedIndex : root.selectedIndex)

    // Explicit focus targets retained for existing smoke assertions
    readonly property Item inputGainRow: root.rowAt(0)
    readonly property Item parametricEqRow: root.rowAt(1)

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
        anchors.bottom: finalizationDock.visible ? finalizationDock.top : parent.bottom
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
                model: root.liveModuleIndices
                delegate: chainRowDelegate
            }
        }
    }

    // Terminal DSPs remain VISIBLE at the bottom while only the LIVE
    // chain scrolls above. Dither is not synthesized as a placeholder.
    Rectangle {
        id: finalizationDock
        objectName: "dspTerminalFinalizationDock"
        visible: root.terminalModuleIndices.length > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 4
        height: visible ? 34 + root.terminalModuleIndices.length * 52
            + Math.max(0, root.terminalModuleIndices.length - 1) * 4 : 0
        radius: 4
        color: "#0C1A28"
        border.width: 1
        border.color: "#365069"

        Rectangle {
            id: finalizationDivider
            objectName: "dspFinalizationDivider"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 2
            color: "#C4774A"
        }
        Text {
            objectName: "dspFinalizationHeading"
            anchors.left: parent.left
            anchors.top: finalizationDivider.bottom
            anchors.leftMargin: 8
            anchors.topMargin: 5
            text: "FINALIZATION · OFFLINE"
            font.family: "Segoe UI"
            font.pixelSize: 9
            font.weight: Font.DemiBold
            color: "#C4774A"
        }
        ColumnLayout {
            id: terminalStack
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            anchors.bottomMargin: 4
            spacing: 4
            Repeater {
                id: terminalRepeater
                model: root.terminalModuleIndices
                delegate: chainRowDelegate
            }
        }
    }

    // Reuse the same LED, focus, state, tooltip and navigation grammar
    // for both groups; original chain indexes remain authoritative.
    Component {
        id: chainRowDelegate
                Rectangle {
                    id: rowItem
                    objectName: "dspChainRow_" + rowItem.modelIndex
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    radius: 4

                    readonly property int modelIndex: modelData
                    readonly property var moduleAdapter: (rowItem.modelIndex >= 0
                        && rowItem.modelIndex < root.chainModules.length)
                        ? root.chainModules[rowItem.modelIndex] : null
                    readonly property bool isFinalization: root.isTerminal(moduleAdapter)
                    readonly property bool isSelected: root.adapterModel ? (root.adapterModel.selectedInstanceId === (moduleAdapter ? moduleAdapter.instanceId : "")) : (root.selectedIndex === rowItem.modelIndex)
                    readonly property bool isDefaultState: moduleAdapter ? (moduleAdapter.configurationState === "Default") : true
                    readonly property bool isBypassed: moduleAdapter ? (moduleAdapter.bypassSupported && moduleAdapter.bypass) : false
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
                                root.selectedIndex = rowItem.modelIndex
                            }
                            rowItem.forceActiveFocus()
                        }
                    }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                            if (root.adapterModel && moduleAdapter) {
                                root.adapterModel.selectModuleByInstanceId(moduleAdapter.instanceId)
                            } else {
                                root.selectedIndex = rowItem.modelIndex
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_Down) {
                            if (rowItem.modelIndex + 1 < root.chainModules.length) {
                                var nextItem = root.rowAt(rowItem.modelIndex + 1)
                                if (nextItem && nextItem.moduleAdapter && root.adapterModel) {
                                    root.adapterModel.selectModuleByInstanceId(nextItem.moduleAdapter.instanceId)
                                } else {
                                    root.selectedIndex = rowItem.modelIndex + 1
                                }
                                if (nextItem) nextItem.forceActiveFocus()
                                event.accepted = true
                            }
                        } else if (event.key === Qt.Key_Up) {
                            if (rowItem.modelIndex > 0) {
                                var prevItem = root.rowAt(rowItem.modelIndex - 1)
                                if (prevItem && prevItem.moduleAdapter && root.adapterModel) {
                                    root.adapterModel.selectModuleByInstanceId(prevItem.moduleAdapter.instanceId)
                                } else {
                                    root.selectedIndex = rowItem.modelIndex - 1
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
                        // FIX-UI-004: reclaim label width at the 164px chain
                        // minimum while preserving fixed LED/BYP/error slots.
                        anchors.leftMargin: 7
                        anchors.rightMargin: 4
                        spacing: 2

                        // Configuration Indicator LED
                        Rectangle {
                            objectName: "dspChainConfigLed_" + rowItem.modelIndex
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
                            ToolTip.visible: (ledMouse.containsMouse) && stationaryTooltipDwell1.ready
                            ToolTip.delay: 0
                            StudioTooltipDwell {
                                id: stationaryTooltipDwell1
                                anchors.fill: parent
                            }

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
                                objectName: "dspChainModuleTitle_" + rowItem.modelIndex
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
                                objectName: "dspChainStateText_" + rowItem.modelIndex
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.alignment: Qt.AlignLeft
                                // Never show an ellipsis in the chain state.
                                // A concise human label fits the canonical
                                // 164px column; diagnostics remain discoverable.
                                wrapMode: Text.WordWrap
                                elide: Text.ElideNone
                                text: root.readableState(rowItem.moduleAdapter)
                                color: "#7A8E9E"
                                font.family: "Segoe UI"
                                font.pixelSize: 9
                                visible: true
                                ToolTip.text: root.stateHint(rowItem.moduleAdapter)
                                ToolTip.visible: chainStateTooltipDwell.ready
                                ToolTip.delay: 0
                                StudioTooltipDwell {
                                    id: chainStateTooltipDwell
                                    anchors.fill: parent
                                    canShow: !!rowItem.moduleAdapter
                                        && rowItem.moduleAdapter.stateText.length > 0
                                }
                            }
                        }

                        // Bypass Badge ("BYP") fixed width container to prevent title shift (UI-MINOR-03)
                        Item {
                            Layout.minimumWidth: 28
                            Layout.preferredWidth: 28
                            Layout.maximumWidth: 28
                            Layout.preferredHeight: 16

                            Rectangle {
                                objectName: "dspChainBypassBadge_" + rowItem.modelIndex
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
                                objectName: "dspChainErrorBadge_" + rowItem.modelIndex
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
