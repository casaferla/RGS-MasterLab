import QtQuick

// One interaction policy for every RGS hover hint in every DSP editor.
// This transparent item observes pointer hover without handling clicks.
Item {
    id: guard

    property bool canShow: true
    // Also suppress hints while selecting/editing a TextInput.
    property bool interacting: parent !== null
        && (parent.pressed === true
            || (parent.activeFocus === true && parent.selectByMouse === true))
    property bool dwellComplete: false
    readonly property bool ready: dwellComplete && pointer.hovered
        && canShow && !interacting && enabled && visible

    function resetDwell() {
        dwellComplete = false
        stationaryTimer.stop()
        if (pointer.hovered && canShow && !interacting && enabled && visible)
            stationaryTimer.start()
    }

    onCanShowChanged: resetDwell()
    onInteractingChanged: resetDwell()
    onEnabledChanged: resetDwell()
    onVisibleChanged: resetDwell()

    HoverHandler {
        id: pointer
        onHoveredChanged: guard.resetDwell()
        // Every movement restarts the 1000-ms stationary interval.
        onPointChanged: guard.resetDwell()
    }

    Timer {
        id: stationaryTimer
        interval: 1000
        repeat: false
        onTriggered: guard.dwellComplete = pointer.hovered
            && guard.canShow && !guard.interacting
    }
}
