import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// M19/M20/M21 real Product navigation surface. No demo/fixture metrics.
// PO approved authentic development numbers; canonical Gate G and release
// certification remain separately tracked, not falsely claimed here.
Item {
    id: root
    objectName: "analysisStatusEditor"
    property var statusViewModel: null
    property int panelIndex: 0
    readonly property string panelId: panelIndex === 0 ? "M19"
        : panelIndex === 1 ? "M20" : "M21"
    readonly property string panelTitle: panelIndex === 0
        ? "Basic Level Analyzer" : panelIndex === 1
        ? "Basic Tonal & Stereo Analyzer" : "Metrics / Delta"
    readonly property var frame: !statusViewModel ? ({})
        : panelIndex === 0 ? statusViewModel.m19
        : panelIndex === 1 ? statusViewModel.m20 : statusViewModel.m21
    readonly property bool authorized: !!statusViewModel
        && statusViewModel.productionAuthorized === true
    // PO authorized genuine analysis values during development. This is
    // deliberately separate from the still-closed official Gate G channel.
    readonly property var observedReadouts: statusViewModel
        ? statusViewModel.analysisReadouts : ({})
    readonly property string selectedRole: frame && frame.sourceRole
        ? String(frame.sourceRole) : "PREPARED"
    readonly property string lifecycle: frame && frame.privateEvidenceStatus
        ? String(frame.privateEvidenceStatus) : "MISSING"
    implicitWidth: 0
    implicitHeight: 0

    Rectangle {
        anchors.fill: parent
        color: "#0A1C2D"
        border.color: "#31506B"
        radius: 6
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Rectangle {
                Layout.preferredWidth: 4
                Layout.preferredHeight: 42
                radius: 2
                color: "#6CBCCF"
            }
            ColumnLayout {
                spacing: 2
                Text {
                    text: root.panelId + " · " + root.panelTitle
                    font.family: "Segoe UI"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#E4EDF5"
                }
                Text {
                    text: "READ-ONLY ANALYSIS · NO AUDIO MUTATION"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    color: "#9DB8C8"
                }
            }
            Item {Layout.fillWidth: true}
            Text {
                objectName: "analysisProductAuthorityLabel"
                text: root.authorized ? "PRODUCT DATA" : "ANALYSIS · DEVELOPMENT"
                font.family: "Segoe UI"
                font.pixelSize: 10
                color: root.authorized ? "#B6D6CA" : "#F0C87D"
            }
        }

        Text {
            text: "INDEPENDENT ANALYSIS SOURCE"
            font.family: "Segoe UI"
            font.pixelSize: 11
            font.bold: true
            color: "#A4BBCB"
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: ["PREPARED", "REFERENCE", "PROCESSED"]
                delegate: StudioButton {
                    objectName: "analysisSourceOption_" + index
                    text: modelData
                    minimumControlWidth: 114
                    selected: root.selectedRole === modelData
                    accentColor: "#6CBCCF"
                    enabled: !!root.statusViewModel
                    onClicked: {
                        if (root.statusViewModel)
                            root.statusViewModel.selectPanelSource(
                                root.panelIndex, modelData)
                    }
                    ToolTip.text: modelData === "REFERENCE"
                        ? "Analyze the independently selected REFERENCE WAV, "
                          + "using its own sample rate and full-track scope."
                        : modelData === "PREPARED"
                        ? "Show the current source after PREPARATION; "
                          + "no DSP or audio state changes by selecting it."
                        : "Show the current processed source analysis; "
                          + "selection does not render or alter processing."
                    ToolTip.visible: sourceChoiceDwell.ready
                    ToolTip.delay: 0
                    StudioTooltipDwell {
                        id: sourceChoiceDwell
                        anchors.fill: parent
                    }
                }
            }
            Item {Layout.fillWidth: true}
        }

        // Approved independent Product visuals, clipped and scrollable INSIDE
        // the existing host. Keep this editor instance permanently alive.
        AnalysisApprovedContent {
            objectName: "analysisApprovedContent"
            panelIndex: root.panelIndex
            lifecycle: root.lifecycle
            observedReadouts: root.observedReadouts
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
        }
        Text {
            objectName: "analysisEvidenceState"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "This panel is read-only. Source selection cannot prepare, "
                + "process or modify audio. No demo, fixture or stale values are displayed."
            color: "#839BAD"
            font.family: "Segoe UI"
            font.pixelSize: 11
        }
    }
}
