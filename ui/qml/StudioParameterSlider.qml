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
    property color semanticAccent: accentColor
    property bool hasSemanticAccent: false
    // Secondary controls stay neutral except while actively selected.
    // The default remains false to preserve every existing DSP slider.
    property bool focusAccentOnly: false
    // Optional EQ editor: a persistent 1px band-identity border plus
    // a stronger focus/drag halo, without recoloring its common track.
    property bool haloOnFocus: false
    property bool lightAccentSeparator: false
    readonly property bool accentActive: (focusAccentOnly || haloOnFocus)
        && internalSlider.enabled
        && (internalSlider.activeFocus || internalSlider.pressed)
    property bool fillFromOrigin: false
    property real positionFillOrigin: 0.0
    property color positionFillColor: Qt.alpha("#C4774A", 0.38)
    property string fieldName: ""
    property var viewModel: null

    property string accessibleName: ""
    property string accessibleDescription: ""
    // Optional tooltip for new slider instances; legacy DSP stays unchanged.
    property string interactionHint: ""

    signal draftValueChanged(real draftVal)
    signal commitRequested(real finalVal)

    implicitWidth: compact ? 120 : 160
    implicitHeight: compact ? 14 : 24

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
        hoverEnabled: true
        ToolTip.text: control.interactionHint
        ToolTip.visible: hovered && control.interactionHint.length > 0
        ToolTip.delay: 550

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
            id: trackBackground
            objectName: "studioSliderTrack"
            x: internalSlider.leftPadding
            y: internalSlider.topPadding + internalSlider.availableHeight / 2 - height / 2
            width: internalSlider.availableWidth
            height: control.compact ? 4 : 8
            radius: control.compact ? 2 : 4
            color: "#08121C"
            border.color: "#1E354A"
            border.width: 1

            // Use the SAME geometry as the actual handle. A handle's CENTER
            // moves within [radius, trackWidth-radius]; its visualPosition
            // must not be multiplied by the full background width.
            function thumbCenterAt(pos) {
                const radius = internalSlider.handle
                    ? internalSlider.handle.width / 2 : (control.compact ? 6 : 9)
                return radius + Math.max(0, width - 2 * radius)
                    * Math.max(0, Math.min(1, pos))
            }

            Rectangle {
                id: trackFill
                objectName: "studioSliderTrackFill"
                readonly property real originPx: control.fillFromOrigin
                    ? trackBackground.thumbCenterAt(
                        control.valueToPosition(control.positionFillOrigin)) : 0
                readonly property real currentPx:
                    trackBackground.thumbCenterAt(internalSlider.visualPosition)
                x: Math.min(originPx, currentPx)
                y: 0
                width: Math.abs(currentPx - originPx)
                height: parent.height
                radius: parent.radius
                color: control.positionFillColor
            }
        }

        handle: Rectangle {
            objectName: "studioSliderThumb"
            x: internalSlider.leftPadding + internalSlider.visualPosition * (internalSlider.availableWidth - width)
            y: internalSlider.topPadding + internalSlider.availableHeight / 2 - height / 2
            width: control.compact ? 12 : 18
            height: control.compact ? 12 : 18
            radius: width / 2
            color: internalSlider.enabled ? "#E6EEF0" : "#586773"
            border.color: control.focusAccentOnly
                ? (control.accentActive ? control.accentColor : "#1E354A")
                : (control.hasSemanticAccent ? control.semanticAccent
                    : (internalSlider.activeFocus ? "#00C8FF" : "#1E354A"))
            border.width: control.focusAccentOnly
                ? (control.accentActive ? 2 : 1)
                : (control.hasSemanticAccent
                    ? (internalSlider.activeFocus ? 2 : 1.5)
                    : (internalSlider.activeFocus ? 2 : 1))
            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 8
                height: parent.height + 8
                radius: width / 2
                color: Qt.alpha(control.accentColor, 0.23)
                border.color: Qt.alpha(control.accentColor, 0.53)
                border.width: 1
                z: -1
                visible: control.accentActive
            }
            // The EQ's sixth band is white. A dark inner separator keeps
            // its white identity ring distinct from the white thumb.
            Rectangle {
                anchors.centerIn: parent
                width: parent.width - 5
                height: parent.height - 5
                radius: width / 2
                color: "#E6EEF0"
                border.color: "#142635"
                border.width: 1
                visible: control.lightAccentSeparator
            }
        }

        Accessible.name: control.accessibleName.length > 0 ? control.accessibleName : (control.fieldName + " slider")
        Accessible.description: control.interactionHint.length > 0
            ? control.interactionHint : control.accessibleDescription
    }
}
