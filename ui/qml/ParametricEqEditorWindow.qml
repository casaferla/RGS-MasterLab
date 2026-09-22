import QtQuick
import QtQuick.Controls

Window {
    id: root
    objectName: "parametricEqToolWindow"
    width: 1040
    height: 660
    minimumWidth: 900
    minimumHeight: 580
    visible: false
    title: "Parametric EQ — RGS MasterLab"
    color: "#071117"
    flags: Qt.Window

    property var viewModel: null

    onClosing: function(close) {
        close.accepted = false
        root.hide()
    }

    ParametricEqEditor {
        id: editor
        objectName: "parametricEqEditor"
        anchors.fill: parent
        anchors.margins: 8
        viewModel: root.viewModel
    }
}
