import QtQuick
import QtQuick.Controls

Item {
    id: control

    property real value: 0.0
    property real from: 0.0
    property real to: 1.0
    property real stepSize: 0.1
    property bool logarithmic: false
    property bool compact: false
    property bool enabled: true
    property color accentColor: "#00C8FF"
    property string fieldName: ""
    property var viewModel: null

    property string accessibleName: ""
    property string accessibleDescription: ""

    signal draftValueChanged(real draftVal)
    signal commitRequested(real finalVal)

    implicitWidth: compact ? 120 : 160
    implicitHeight: compact ? 18 : 24

    // Value <-> normalized position [0, 1] mapping
    function valueToPosition(val) {
        if (control.to <= control.from) return 0.0;
        var clamped = Math.max(control.from, Math.min(control.to, val));
        if (control.logarithmic && control.from > 0 && control.to > 0) {
            return Math.log(clamped / control.from) / Math.log(control.to / control.from);
        } else {
            return (clamped - control.from) / (control.to - control.from);
        }
    }

    function positionToValue(pos) {
        var clampedPos = Math.max(0.0, Math.min(1.0, pos));
        var rawVal = 0.0;
        if (control.logarithmic && control.from > 0 && control.to > 0) {
            rawVal = control.from * Math.pow(control.to / control.from, clampedPos);
        } else {
            rawVal = control.from + clampedPos * (control.to - control.from);
        }
        if (control.stepSize > 0) {
            rawVal = Math.round((rawVal - control.from) / control.stepSize) * control.stepSize + control.from;
        }
        return Math.max(control.from, Math.min(control.to, rawVal));
    }

    Slider {
        id: internalSlider
        anchors.fill: parent
        from: 0.0
        to: 1.0
        stepSize: 0.001
        enabled: control.enabled
        activeFocusOnTab: true

        value: control.valueToPosition(control.value)

        Binding {
            target: internalSlider
            property: "value"
            value: control.valueToPosition(control.value)
            when: !internalSlider.pressed
        }

        onMoved: {
            var currentVal = control.positionToValue(internalSlider.visualPosition);
            if (internalSlider.pressed) {
                // Pointer drag: draft only
                if (control.viewModel && control.fieldName.length > 0) {
                    if (typeof control.viewModel.setDraftFieldValue === "function") {
                        control.viewModel.setDraftFieldValue(control.fieldName, currentVal);
                    }
                }
                control.draftValueChanged(currentVal);
            } else {
                // Keyboard / discrete move: commit immediately
                if (control.viewModel) {
                    if (control.fieldName.length > 0 && typeof control.viewModel.setDraftFieldValue === "function") {
                        control.viewModel.setDraftFieldValue(control.fieldName, currentVal);
                    }
                    if (typeof control.viewModel.commitDraft === "function") {
                        control.viewModel.commitDraft();
                    }
                }
                control.commitRequested(currentVal);
            }
        }

        onPressedChanged: {
            if (!internalSlider.pressed) {
                // Pointer release: validate + commit once
                var finalVal = control.positionToValue(internalSlider.visualPosition);
                if (control.viewModel) {
                    if (control.fieldName.length > 0 && typeof control.viewModel.setDraftFieldValue === "function") {
                        control.viewModel.setDraftFieldValue(control.fieldName, finalVal);
                    }
                    if (typeof control.viewModel.commitDraft === "function") {
                        control.viewModel.commitDraft();
                    }
                }
                control.commitRequested(finalVal);
            }
        }

        background: Rectangle {
            x: internalSlider.leftPadding
            y: internalSlider.topPadding + internalSlider.availableHeight / 2 - height / 2
            width: internalSlider.availableWidth
            height: control.compact ? 6 : 8
            radius: control.compact ? 3 : 4
            color: "#08121C"
            border.color: "#1E354A"
            border.width: 1

            // Subdued subtle track fill showing current level
            Rectangle {
                x: 0
                y: 0
                width: internalSlider.visualPosition * parent.width
                height: parent.height
                radius: parent.radius
                color: Qt.alpha(control.accentColor, 0.25)
            }
        }

        handle: Rectangle {
            x: internalSlider.leftPadding + internalSlider.visualPosition * (internalSlider.availableWidth - width)
            y: internalSlider.topPadding + internalSlider.availableHeight / 2 - height / 2
            width: control.compact ? 14 : 18
            height: control.compact ? 14 : 18
            radius: width / 2
            color: internalSlider.enabled ? "#E6EEF0" : "#586773"
            border.color: internalSlider.activeFocus ? "#00C8FF" : "#1E354A"
            border.width: internalSlider.activeFocus ? 2 : 1
        }

        Accessible.name: control.accessibleName.length > 0 ? control.accessibleName : (control.fieldName + " slider")
        Accessible.description: control.accessibleDescription
    }
}
