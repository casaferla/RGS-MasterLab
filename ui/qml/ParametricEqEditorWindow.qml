import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    property var viewModel: null

    objectName: "parametricEqToolWindow"
    title: "Parametric EQ — RGS MasterLab"

    width: 1040
    height: 660
    minimumWidth: 900
    minimumHeight: 580

    transientParent: ApplicationWindow.window
    visible: false

    // Window Frame Slate Material
    background: Rectangle {
        radius: 8
        color: "#0A1624"
        border.color: "#1E3B56"
        border.width: 1

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#102338" }
            GradientStop { position: 1.0; color: "#08121C" }
        }

        // Top highlight
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 1
            height: 1
            color: "#332A4C68"
        }
    }

    ParametricEqEditor {
        id: editor
        objectName: "parametricEqEditor"
        anchors.fill: parent
        anchors.margins: 24
        viewModel: root.viewModel
    }

    onClosing: function(close) {
        close.accepted = false
        root.hide()
    }
}
