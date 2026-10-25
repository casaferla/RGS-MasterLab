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

    // Shape identifies the individual module; tint conveys its family.
    // Fixed 16px slot in the space recovered from the old 28px BYP badge;
    // never resized or hidden with bypass / scrolling / DPI.
    function moduleIconKind(moduleAdapter) {
        if(!moduleAdapter)return ""
        switch(moduleAdapter.editorContentKey) {
        case "INPUT_GAIN": return "module-gain"
        case "PARAMETRIC_EQ": return "module-eq"
        case "COMPRESSOR": return "module-compressor"
        case "STEREO_MS": return "module-stereo"
        case "TRUE_PEAK_METER": return "module-peak-meter"
        case "TRUE_PEAK_LIMITER": return "module-limiter"
        case "MASTER_MONITOR": return "module-headphones"
        default: return ""
        }
    }

    function moduleIconColor(moduleAdapter) {
        if(!moduleAdapter)return "#6683A1"
        // Color denotes the frozen family, not current bypass intent.
        // M16's pre-existing cyan meter accent and M17's bypass/active
        // title colors stay unchanged; only their pictograms use family hues.
        if(moduleAdapter.editorContentKey === "TRUE_PEAK_METER")
            return "#6683A1"  // ANALYSIS / METERING
        if(moduleAdapter.editorContentKey === "TRUE_PEAK_LIMITER")
            return "#C4774A"  // DYNAMICS
        if(moduleAdapter.editorContentKey === "MASTER_MONITOR")
            return "#718092"  // WORKFLOW / UTILITY
        return moduleAdapter.familyAccent
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
                        // PO: one 16px semantic icon REPLACES 8px config LED.
                        // Only independent 8px error indicator remains.
                        anchors.leftMargin: 7
                        anchors.rightMargin: 4
                        spacing: 2

                        // One symbol REPLACES the previous configuration LED.
                        // Fixed 16px glyph slot, no extra 8px dot to its right.
                        // Family hue stays constant: DIM = Default, BRIGHT = Manual.
                        // Bypass separately mutes the ROW while retaining the
                        // true Default/Manual semantic and stable coordinates.
                        StudioIcon {
                            id: moduleSymbol
                            objectName: "dspChainModuleIcon_" + rowItem.modelIndex
                            Layout.minimumWidth: 16
                            Layout.preferredWidth: 16
                            Layout.maximumWidth: 16
                            Layout.minimumHeight: 16
                            Layout.preferredHeight: 16
                            Layout.maximumHeight: 16
                            kind: root.moduleIconKind(rowItem.moduleAdapter)
                            strokeWidth: rowItem.isDefaultState ? 1.65 : 2.1
                            strokeColor: root.moduleIconColor(rowItem.moduleAdapter)
                            opacity: (rowItem.isDefaultState ? 0.37 : 1.0)
                                * (rowItem.isBypassed ? 0.7 : 1.0)
                            Accessible.name: rowItem.moduleAdapter
                                ? rowItem.moduleAdapter.displayName + " · "
                                    + (rowItem.isDefaultState ? "Default" : "Manual")
                                : "Module icon"
                            ToolTip.text: rowItem.moduleAdapter
                                ? rowItem.moduleAdapter.displayName + " · "
                                    + (rowItem.isDefaultState
                                        ? "Default settings" : "Manual non-default settings")
                                    + (rowItem.isBypassed ? " · Bypassed" : "")
                                : "Module"
                            ToolTip.visible: moduleSymbolHover.hovered && moduleSymbolTooltipDwell.ready
                            ToolTip.delay: 0
                            HoverHandler { id: moduleSymbolHover }
                            StudioTooltipDwell {
                                id: moduleSymbolTooltipDwell
                                anchors.fill: parent
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
