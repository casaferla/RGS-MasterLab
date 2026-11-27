import QtQuick
import QtQuick.Layouts
// Research-only detached companion. The entire original graph stays visible.
Rectangle {
    id: root
    property var dynamicsViewModel: null
    readonly property bool editable: dynamicsViewModel
        && dynamicsViewModel.loaded && dynamicsViewModel.manualEditable
    height: 112
    radius: 5
    color: "#142B3C"
    border.color: "#27465F"
    border.width: 1
    RowLayout {
        anchors.fill: parent
        anchors.margins: 7
        spacing: 7
        Repeater {
            model: [
                {key:"range",label:"RANGE",unit:"dB",lo:-18,hi:18,step:0.1,log:false},
                {key:"threshold",label:"THRESHOLD",unit:"dBFS",lo:-120,hi:0,step:0.5,log:false},
                {key:"ratio",label:"RATIO",unit:"",lo:1,hi:20,step:0.1,log:false},
                {key:"rmsTime",label:"RMS TIME",unit:"ms",lo:1,hi:500,step:1,log:true},
                {key:"attack",label:"ATTACK",unit:"ms",lo:0.1,hi:500,step:0.1,log:true},
                {key:"release",label:"RELEASE",unit:"ms",lo:1,hi:5000,step:1,log:true}
            ]
            delegate: StudioNumericSliderPair {
                required property var modelData
                objectName: "dynamicEqControl_" + modelData.key
                Layout.fillWidth: true
                Layout.minimumWidth: 90
                Layout.maximumWidth: 150
                enabled: root.editable
                viewModel: root.dynamicsViewModel
                compact: true
                sliderCompact: true
                compactFieldWidth: 112
                labelText: modelData.label
                unitText: modelData.unit
                fieldName: modelData.key
                rawText: root.dynamicsViewModel
                    ? root.dynamicsViewModel.texts[modelData.key] : "—"
                value: root.dynamicsViewModel
                    ? root.dynamicsViewModel.values[modelData.key] : modelData.lo
                from: modelData.lo
                to: modelData.hi
                stepSize: modelData.step
                logarithmic: modelData.log
                accentColor: "#C4774A"
                fillFromOrigin: modelData.key === "range"
                positionFillOrigin: 0
                interactionHint: root.editable
                    ? "Adjust " + modelData.label + ". Enter or release to apply; Esc to cancel."
                    : "Read-only accepted AutoMatch parameter."
            }
        }
    }
}
