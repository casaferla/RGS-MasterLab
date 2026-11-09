import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    // Snapshot of the actual docked editor before LayoutItemProxy migration.
    property string editorKind: ""
    property real dockReferenceWidth: 0
    property real dockReferenceHeight: 0
    // Extra fixed-size header and controls need a real per-module minimum.
    readonly property int safeEditorWidth: {
        switch (editorKind) {
        case "INPUT_GAIN": return 700
        case "PARAMETRIC_EQ": return 940
        case "COMPRESSOR": return 940
        case "STEREO_MS": return 920
        case "TRUE_PEAK_METER": return 760
        case "TRUE_PEAK_LIMITER": return 940
        case "MASTER_MONITOR": return 760
        default: return 920
        }
    }
    readonly property int safeEditorHeight: {
        switch (editorKind) {
        case "INPUT_GAIN": return 500
        case "PARAMETRIC_EQ": return 620
        case "COMPRESSOR": return 600
        case "STEREO_MS": return 585
        case "TRUE_PEAK_METER": return 590
        case "TRUE_PEAK_LIMITER": return 640
        case "MASTER_MONITOR": return 560
        default: return 600
        }
    }
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
    // An owned transient Qt window remains above Main even when Main
    // is focused. Independent top-level DSP windows follow native Z order.
    // Never use global WindowStaysOnTopHint.
    transientParent: null
    width: 920
    height: 620
    readonly property int floatingChromeInset: 20
    minimumWidth: Math.ceil(Math.max(safeEditorWidth, dockReferenceWidth))
        + floatingChromeInset
    minimumHeight: Math.ceil(Math.max(safeEditorHeight, dockReferenceHeight))
        + floatingChromeInset
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
