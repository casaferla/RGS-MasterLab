import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// M19/M20/M21 real Product navigation surface. Never use demo fixtures or
// invent metrics from private preproduction snapshots. The current backend
// publishes lifecycle/readiness only until Phase-6 feature authorization.
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
    readonly property string selectedRole: frame && frame.sourceRole
        ? String(frame.sourceRole) : "PREPARED"
    readonly property string lifecycle: frame && frame.privateEvidenceStatus
        ? String(frame.privateEvidenceStatus) : "MISSING"
    implicitWidth: 640
    implicitHeight: 360

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
                    text: "ANALYSIS ONLY · NEVER CHANGES THE MASTERING CHAIN"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    color: "#9DB8C8"
                }
            }
            Item {Layout.fillWidth: true}
            Text {
                objectName: "analysisProductAuthorityLabel"
                text: root.authorized ? "PRODUCT DATA" : "AWAITING PRODUCT GATE"
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
                }
            }
            Item {Layout.fillWidth: true}
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            color: "#091723"
            radius: 5
            border.color: "#27465F"
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 9
                Text {
                    objectName: "analysisUnavailableTitle"
                    Layout.alignment: Qt.AlignHCenter
                    text: "VERIFIED ANALYSIS UNAVAILABLE"
                    font.family: "Segoe UI"
                    font.pixelSize: 17
                    font.bold: true
                    color: "#D5E2EC"
                }
                Text {
                    objectName: "analysisUnavailableExplanation"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 590
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: root.panelId + " · " + root.selectedRole
                        + ": real metrics, graphs and deltas will become visible"
                        + " only after canonical Product/Phase-6 publication."
                    color: "#A4BBCB"
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                }
                Text {
                    objectName: "analysisEvidenceState"
                    Layout.alignment: Qt.AlignHCenter
                    text: "PRIVATE EVIDENCE: " + root.lifecycle
                        + " · NO PRODUCT NUMERIC AUTHORITY"
                    color: "#F0C87D"
                    font.family: "Consolas"
                    font.pixelSize: 11
                }
            }
        }
        Text {
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
