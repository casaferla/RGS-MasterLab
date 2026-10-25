import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One reusable floating window for ONE existing module/editor instance.
// This shell never constructs a DSP editor: it rehosts the EXACT QQuickItem
// already owned by DspWorkspace/DspEditorHost through LayoutItemProxy.
// The paired DOCK proxy must be hidden before setting detached=true.
// Only the parent coordinator may decide the authoritative target/instance.
Window {
    id: root
    objectName: "dspFloatingModuleWindow"
    property Item editorTarget: null
    property string moduleInstanceId: ""
    property string moduleTitle: "Module"
    property color familyAccent: "#718092"
    property bool detached: false

    signal redockRequested(string moduleInstanceId)

    title: moduleTitle + " — RGS MasterLab"
    visible: detached && editorTarget !== null
    flags: Qt.Window
    width: 920
    height: 620
    minimumWidth: 560
    minimumHeight: 420
    color: "#0B1724"

    function returnToDock() {
        // Request the SINGLE parent-owned state transition. Assigning
        // detached here breaks its QML binding to Main.editorDetached,
        // making a second undock silently fail.
        if (detached) redockRequested(moduleInstanceId)
    }
    function focusExisting() {
        if (!detached) return
        if (visibility === Window.Minimized)
            showNormal()
        raise()
        requestActivate()
    }
    onClosing: function(close) {
        // Closing always docks; never destroys the existing QML editor.
        close.accepted = false
        returnToDock()
    }

    Rectangle {
        anchors.fill: parent
        color: "#0B1724"
        border.color: "#27465F"
        border.width: 1
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 2
            color: root.familyAccent
        }
    }
    RowLayout {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        height: 38
        spacing: 8

        Text {
            objectName: "dspFloatingModuleTitle"
            text: root.moduleTitle
            Layout.fillWidth: true
            font.family: "Segoe UI"
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: "#E0EAF2"
            elide: Text.ElideRight
        }
        StudioButton {
            objectName: "dspFloatingRedockButton"
            text: "Dock"
            accentColor: root.familyAccent
            onClicked: root.returnToDock()
            Accessible.name: "Return " + root.moduleTitle + " to DSP workspace"
        }
    }
    LayoutItemProxy {
        id: floatingProxy
        objectName: "dspFloatingEditorProxy"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 10
        visible: root.detached && root.editorTarget !== null
        target: root.editorTarget
    }
}
