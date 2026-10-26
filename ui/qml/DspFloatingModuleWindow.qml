import QtQuick
import QtQuick.Controls
import QtQml.Models

// No second visible row: the ONLY Dock icon stays in the SAME shared
// DspEditorHost header, immediately before the module title.
// The native OS Window title bar provides all free window movement.
Window {
    id: root
    objectName: "dspFloatingModuleWindow"
    property Item editorTarget: null
    property string moduleInstanceId: ""
    property string moduleTitle: "Module"
    property color familyAccent: "#718092"
    property bool detached: false
    signal redockRequested(string moduleInstanceId)
    // Retained as no-op compatibility interfaces for existing fixtures.
    signal headerDragged(real globalX, real globalY)
    signal headerReleased(real globalX, real globalY)
    property bool dragOverDock: false
    property bool wasMovedByGrip: false

    title: moduleTitle + " — RGS MasterLab"
    visible: detached && editorTarget !== null
    flags: Qt.Window
    width: 920
    height: 620
    minimumWidth: 560
    minimumHeight: 420
    color: "#0B1724"

    function returnToDock() {
        if (detached) redockRequested(moduleInstanceId)
    }
    function focusExisting() {
        if (!detached) return
        if (visibility === Window.Minimized) showNormal()
        raise()
        requestActivate()
    }
    onClosing: function(close) {
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
    LayoutItemProxy {
        id: floatingProxy
        objectName: "dspFloatingEditorProxy"
        anchors.fill: parent
        anchors.margins: 10
        visible: root.detached && root.editorTarget !== null
        target: root.editorTarget
    }
}
