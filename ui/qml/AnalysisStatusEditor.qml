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

        // DspEditorHost owns M19/M20 source choices in its fixed top-right
        // action slot. M21 is a simultaneous three-column comparison and
        // intentionally has no source-selection affordance. Keep only
        // the truthful status and context here, not a second action row.
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            spacing: 8
            Text {
                objectName: "analysisSourceHeading"
                text: root.panelIndex === 2
                    ? "THREE INDEPENDENT MEASUREMENTS"
                    : "CURRENT SCOPE · " + root.selectedRole
                font.family: "Segoe UI"
                font.pixelSize: 11
                font.bold: true
                color: "#A4BBCB"
            }
            Item { Layout.fillWidth: true }
            Text {
                objectName: "analysisProductAuthorityLabel"
                // The exact owner-thread worker informs this status.
                // Playback is unrelated; NEVER invent progress percentages.
                text: root.statusViewModel
                    && root.statusViewModel.runningAnalysisRole
                    && root.statusViewModel.runningAnalysisRole.length > 0
                    ? "ANALYZING " + root.statusViewModel.runningAnalysisRole
                        + " · DEVELOPMENT"
                    : root.authorized ? "PRODUCT DATA" : "ANALYSIS · DEVELOPMENT"
                font.family: "Segoe UI"
                font.pixelSize: 10
                color: root.authorized ? "#B6D6CA" : "#F0C87D"
            }
        }

        // Approved independent Product visuals are strictly NO-SCROLL.
        // Their responsive layout fits the 1184x688 Product minimum.
        // Keep this editor instance permanently alive.
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
        // Preserve the compatibility object, but not the redundant
        // paragraph already explained by the read-only host/authority badges.
        Item {
            objectName: "analysisEvidenceState"
            visible: false
            Layout.preferredHeight: 0
            Layout.minimumHeight: 0
        }
    }
}
