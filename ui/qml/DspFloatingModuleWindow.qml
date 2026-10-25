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

        Item {
            id: floatingGrip
            objectName: "dspFloatingDragGrip"
            Layout.preferredWidth: 28
            Layout.minimumWidth: 28
            Layout.maximumWidth: 28
            Layout.preferredHeight: 32
            property real lastX: 0
            property real lastY: 0
            property real startWindowX: 0
            property real startWindowY: 0
            property bool cancelled: false
            Accessible.name: "Drag floating " + root.moduleTitle + " to dock area"
            Text {
                anchors.centerIn: parent
                text: "⠿"
                font.family: "Segoe UI"
                font.pixelSize: 20
                color: root.familyAccent
            }
            DragHandler {
                id: floatingHeaderDrag
                target: null
                acceptedButtons: Qt.LeftButton
                onActiveChanged: {
                    if(active) {
                        floatingGrip.lastX=0
                        floatingGrip.lastY=0
                        floatingGrip.startWindowX=root.x
                        floatingGrip.startWindowY=root.y
                        floatingGrip.cancelled=false
                        root.wasMovedByGrip=false
                    } else if(root.wasMovedByGrip && !floatingGrip.cancelled) {
                        // Native WINDOW motion is not a QML DropArea drag:
                        // drop permission is decided by the Main coordinator
                        // against a globally mapped docking rectangle.
                        root.headerReleased(
                            root.x+header.x+floatingGrip.x+floatingGrip.width/2,
                            root.y+header.y+floatingGrip.y+floatingGrip.height/2)
                    }
                }
                onTranslationChanged: {
                    if(!active || floatingGrip.cancelled)return
                    const dx=translation.x-floatingGrip.lastX
                    const dy=translation.y-floatingGrip.lastY
                    if(!Number.isFinite(dx)||!Number.isFinite(dy))return
                    floatingGrip.lastX=translation.x
                    floatingGrip.lastY=translation.y
                    if(Math.abs(dx)+Math.abs(dy)<0.01)return
                    root.x+=dx
                    root.y+=dy
                    root.wasMovedByGrip=true
                    root.headerDragged(
                        root.x+header.x+floatingGrip.x+floatingGrip.width/2,
                        root.y+header.y+floatingGrip.y+floatingGrip.height/2)
                }
            }
            Shortcut {
                sequence: "Escape"
                enabled: floatingHeaderDrag.active
                onActivated: {
                    floatingGrip.cancelled=true
                    root.x=floatingGrip.startWindowX
                    root.y=floatingGrip.startWindowY
                    root.headerDragged(-1,-1)
                }
            }
            StudioTooltipDwell {
                id: floatingGripDwell
                anchors.fill: parent
            }
            ToolTip.text: "Drag this handle into the main editor to dock"
            ToolTip.visible: floatingGripDwell.ready
            ToolTip.delay: 0
        }
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
    Rectangle {
        objectName: "floatingDockCandidate"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 10
        radius: 7
        z: 10
        visible: root.dragOverDock
        color: "#1700C8FF"
        border.width: 2
        border.color: "#00C8FF"
        Text {
            anchors.centerIn: parent
            text: "Release to dock"
            font.family: "Segoe UI"
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: "#BFEAF6"
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
