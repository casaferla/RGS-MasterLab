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
    // Delegate lifecycle invalidates all original-index row handles.
    property int rowGeneration: 0

    // Fit the fixed-width chain row without truncating semantic words.
    // Diagnostic/ownership detail stays in the stationary-hover tooltip;
    // this is presentation only and never changes the real M17 state.
    function readableState(moduleAdapter) {
        if (!moduleAdapter) return ""
        const state = moduleAdapter.stateText || ""
        if (moduleAdapter.editorContentKey !== "TRUE_PEAK_LIMITER")
            return moduleAdapter.bypassSupported && moduleAdapter.bypass
                ? "Bypass · " + state : state
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
            return moduleAdapter.bypassSupported && moduleAdapter.bypass
                ? "Processing bypassed · " + state : state
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

    // A SINGLE vertical list is authoritative for processing DSPs,
    // analysis M16, terminal M17 and workflow utility MONITOR. One model
    // index is one adapter identity; no separate Finalization fixed dock.
    readonly property var chainModules: adapterModel ? adapterModel.modules : []
    function rowAt(idx) {
        if(idx<0 ||idx>=root.chainModules.length)return null
        return rowRepeater.itemAt(idx)
    }
    function ensureRowVisible(idx) {
        // Selection can precede QtQuick layout polish or delegate creation.
        Qt.callLater(function() {
            const row=root.rowAt(idx)
            const flick=chainScrollView.contentItem
            if(!row || !flick || !chainScrollView.visible)return
            const top=row.y
            const bottom=top+row.height
            const pad=2
            if(top< flick.contentY+pad)
                flick.contentY=Math.max(0,top-pad)
            else if(bottom>flick.contentY+flick.height-pad)
                flick.contentY=Math.min(Math.max(0,flick.contentHeight-flick.height),
                                        bottom-flick.height+pad)
        })
    }
    onSelectedIndexChanged: ensureRowVisible(selectedIndex)
    onRowGenerationChanged: ensureRowVisible(selectedIndex)
    // itemAt() has no reactive notifier. Every delegate-creation/removal
    // advances rowGeneration so offscreen/visible LayoutItemProxy instances
    // cannot retain an initially null handle at unchanged model length.
    readonly property Item activeRow: {
        const generation = root.rowGeneration
        return generation >= 0 ? root.rowAt(
            root.adapterModel ? root.adapterModel.selectedIndex : root.selectedIndex)
            : null
    }
    // Exact original adapter indices are preserved across both Repeaters.
    readonly property Item inputGainRow: {
        const generation = root.rowGeneration
        return generation >= 0 ? root.rowAt(0) : null
    }
    readonly property Item parametricEqRow: {
        const generation = root.rowGeneration
        return generation >= 0 ? root.rowAt(1) : null
    }

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
        objectName: "dspChainUnifiedScroll"
        anchors.top: chainHeader.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 4
        clip: true
        // Do not use availableWidth: it changes when a platform scrollbar
        // is reserved and makes DSP labels jump at short window heights.
        contentWidth: width
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            width: 3
            z: 10
            contentItem: Rectangle {
                radius: 2
                color: "#60768A"
                opacity: parent.active ? 0.85 : 0.45
            }
        }
        ColumnLayout {
            width: chainScrollView.width
            spacing: 4
            Repeater {
                id: rowRepeater
                model: root.chainModules.length
                delegate: chainRowDelegate
                onItemAdded: root.rowGeneration += 1
                onItemRemoved: root.rowGeneration += 1
            }
        }
    }

    // Shared renderer for ALL module and service rows.
    Component {
        id: chainRowDelegate
                Rectangle {
                    id: rowItem
                    objectName: "dspChainRow_" + rowItem.modelIndex
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    radius: 4

                    readonly property int modelIndex: index
                    readonly property var moduleAdapter: (rowItem.modelIndex >= 0
                        && rowItem.modelIndex < root.chainModules.length)
                        ? root.chainModules[rowItem.modelIndex] : null
                    readonly property bool isSelected: root.adapterModel ? (root.adapterModel.selectedInstanceId === (moduleAdapter ? moduleAdapter.instanceId : "")) : (root.selectedIndex === rowItem.modelIndex)
                    readonly property bool isDefaultState: moduleAdapter ? (moduleAdapter.configurationState === "Default") : true
                    readonly property bool isBypassed: moduleAdapter ? (moduleAdapter.bypassSupported && moduleAdapter.bypass) : false
                    readonly property bool hasError: moduleAdapter ? moduleAdapter.hasError : false

                    // Bypass is a ROW treatment, not a positional BYP badge.
                    // Keep title/LED/error at fixed coordinates in every state.
                    color: isSelected
                        ? (isBypassed ? "#172A38" : "#1A324A")
                        : isBypassed ? (rowMouse.containsMouse ? "#0E1C27" : "#091620")
                        : (rowMouse.containsMouse ? "#122538" : "#0D1D2B")
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
                        opacity: rowItem.isBypassed ? 0.28 : 1.0
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
                                color: rowItem.isBypassed ? "#9FB1BF"
                                    : rowItem.isSelected ? "#F5F8FC" : "#C4D4E0"
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
                                color: rowItem.isBypassed ? "#8196A8" : "#7A8E9E"
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
