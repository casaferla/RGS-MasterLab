// Design-only native Qt dock/floating lifecycle for each independent analyzer.
// No application Main.qml activation, no DSP calculation and no fake C++ data.
// The exact same QML panel instance is handed to only one LayoutItemProxy.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: root
    objectName: "analysisModuleDockHostPreview"
    width: 1184
    height: 688
    property int moduleIndex: 0
    property bool detached: false
    property bool fixtureMode: false
    property var presentation: ({ready:false})
    property int transitionEpoch: 0
    readonly property int safeWidth: 1184
    readonly property int safeHeight: 688
    readonly property string moduleLabel: moduleIndex === 0 ? "M19 · LEVELS"
        : (moduleIndex === 1 ? "M20 · TONAL & STEREO" : "M21 · METRICS / DELTA")
    readonly property string moduleFile: moduleIndex === 0
        ? "M19LevelsPanelPreview.qml" : (moduleIndex === 1
            ? "M20TonalStereoPanelPreview.qml" : "M21MetricsDeltaPanelPreview.qml")
    readonly property Item panelObject: moduleLoader.item

    function requestUndock() {
        if (!panelObject) return false
        if (detached) {
            focusFloating()
            return true
        }
        detached = true
        transitionEpoch += 1
        Qt.callLater(focusFloating)
        return true
    }
    function focusFloating() {
        if (!detached) return
        if (floatingWindow.visibility === Window.Minimized)
            floatingWindow.showNormal()
        floatingWindow.raise()
        floatingWindow.requestActivate()
    }
    function requestDock() {
        if (!detached) return true
        detached = false
        transitionEpoch += 1
        return true
    }
    onPresentationChanged: {
        if (moduleLoader.item) moduleLoader.item.presentation = root.presentation
    }
    onFixtureModeChanged: {
        if (moduleLoader.item) moduleLoader.item.fixtureMode = root.fixtureMode
    }

    Item {
        id: ownershipStore
        width: 0
        height: 0
        visible: false
        Loader {
            id: moduleLoader
            objectName: "analysisPersistentModuleLoader"
            asynchronous: false
            source: root.moduleFile
            onLoaded: {
                if (item) {
                    item.presentation = root.presentation
                    item.fixtureMode = root.fixtureMode
                }
            }
        }
    }

    Rectangle {
        id: dockSurface
        objectName: "analysisDockedSurface"
        anchors.fill: parent
        visible: !root.detached
        radius: 6
        color: "#071117"
        border.width: 1
        border.color: "#2A3947"
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Text {
                    Layout.fillWidth: true
                    text: root.moduleLabel + " · DOCKED PREVIEW"
                    color: "#8FAAB9"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                }
                Button {
                    objectName: "analysisUndockButton"
                    text: "Undock"
                    Accessible.name: "Undock " + root.moduleLabel
                    onClicked: root.requestUndock()
                }
            }
            LayoutItemProxy {
                id: dockedProxy
                objectName: "analysisDockedLayoutProxy"
                target: moduleLoader.item
                visible: !root.detached
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    Window {
        id: floatingWindow
        objectName: "analysisNativeFloatingWindow"
        visible: root.detached
        flags: Qt.Window
        transientParent: null
        width: 1184
        height: 688
        minimumWidth: root.safeWidth
        minimumHeight: root.safeHeight
        title: root.moduleLabel + " — RGS MasterLab · Analysis Preview"
        color: "#071117"
        onClosing: function(close) {
            // Closing a floating analysis is equivalent to Dock, not destroy.
            close.accepted = false
            root.requestDock()
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Text {
                    Layout.fillWidth: true
                    text: root.moduleLabel + " · FLOATING PREVIEW"
                    color: "#8FAAB9"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                }
                Button {
                    objectName: "analysisRedockButton"
                    text: "Dock"
                    Accessible.name: "Dock " + root.moduleLabel
                    onClicked: root.requestDock()
                }
            }
            LayoutItemProxy {
                id: floatingProxy
                objectName: "analysisFloatingLayoutProxy"
                target: moduleLoader.item
                visible: root.detached
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
