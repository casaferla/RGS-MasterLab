import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string endpointName: "Region"
    property string objectPrefix: "regionTime"
    property string hoursText: "00"
    property string minutesText: "00"
    property string secondsText: "00"
    property string fractionText: "000000000"
    property Item previousTabItem
    property Item nextTabItem
    property alias firstField: hoursField
    property alias lastField: fractionField

    signal commitRequested(string hours, string minutes, string seconds, string fraction)
    signal escapeRequested()
    signal nudgeBackwardRequested()
    signal nudgeForwardRequested()

    implicitWidth: fields.implicitWidth
    implicitHeight: fields.implicitHeight

    function commitCandidate() {
        commitRequested(hoursField.text,
                        minutesField.text,
                        secondsField.text,
                        fractionField.text)
        Qt.callLater(root.restoreCommitted)
    }

    function restoreCommitted() {
        hoursField.text = hoursText
        minutesField.text = minutesText
        secondsField.text = secondsText
        fractionField.text = fractionText
        if (hoursField.activeFocus)
            hoursField.selectAll()
        else if (minutesField.activeFocus)
            minutesField.selectAll()
        else if (secondsField.activeFocus)
            secondsField.selectAll()
        else if (fractionField.activeFocus)
            fractionField.selectAll()
    }

    component NumericSegment: TextField {
        id: segment
        required property string segmentName
        property int segmentMaximumLength: 0

        width: segmentName === "fraction" ? 100
             : segmentName === "hours" ? 54 : 40
        height: 36
        color: enabled ? "#E6EEF0" : "#6B7A87"
        selectedTextColor: "#07131B"
        selectionColor: "#43C7F3"
        placeholderTextColor: "#6B7A87"
        font.family: "Cascadia Mono"
        font.pixelSize: 13
        font.weight: Font.Medium
        horizontalAlignment: TextInput.AlignHCenter
        selectByMouse: true
        activeFocusOnTab: true
        inputMethodHints: Qt.ImhDigitsOnly
        maximumLength: segmentMaximumLength > 0
            ? segmentMaximumLength : 32767
        validator: RegularExpressionValidator {
            regularExpression: /^[0-9]*$/
        }
        background: Rectangle {
            radius: 3
            color: segment.enabled ? "#071117" : "#0C151C"
            border.width: segment.activeFocus ? 2 : 1
            border.color: segment.activeFocus ? "#3DA6FF" : "#2A3947"
        }
        Accessible.name: root.endpointName + " " + segmentName
        Accessible.description: segmentName === "fraction"
            ? "Decimal fraction, one through nine digits"
            : segmentName === "hours"
                ? "Hours, decimal digits with no clock limit"
                : segmentName + ", decimal value zero through fifty-nine"
        onActiveFocusChanged: {
            if (activeFocus)
                Qt.callLater(selectAll)
        }
        onEditingFinished: root.commitCandidate()
        Keys.onEscapePressed: {
            root.restoreCommitted()
            root.escapeRequested()
        }
        Keys.onUpPressed: {
            root.nudgeForwardRequested()
            Qt.callLater(root.restoreCommitted)
        }
        Keys.onDownPressed: {
            root.nudgeBackwardRequested()
            Qt.callLater(root.restoreCommitted)
        }
    }

    Row {
        id: fields
        spacing: 3

        NumericSegment {
            id: hoursField
            objectName: root.objectPrefix + "Hours"
            segmentName: "hours"
            KeyNavigation.backtab: root.previousTabItem
            KeyNavigation.tab: minutesField
            Binding {
                target: hoursField
                property: "text"
                value: root.hoursText
                when: !hoursField.activeFocus
            }
        }

        Label {
            objectName: root.objectPrefix + "HoursMinutesSeparator"
            anchors.verticalCenter: hoursField.verticalCenter
            text: ":"
            color: "#8A97A3"
            font.family: "Cascadia Mono"
            font.pixelSize: 14
            Accessible.role: Accessible.StaticText
        }

        NumericSegment {
            id: minutesField
            objectName: root.objectPrefix + "Minutes"
            segmentName: "minutes"
            segmentMaximumLength: 2
            KeyNavigation.backtab: hoursField
            KeyNavigation.tab: secondsField
            Binding {
                target: minutesField
                property: "text"
                value: root.minutesText
                when: !minutesField.activeFocus
            }
        }

        Label {
            objectName: root.objectPrefix + "MinutesSecondsSeparator"
            anchors.verticalCenter: hoursField.verticalCenter
            text: ":"
            color: "#8A97A3"
            font.family: "Cascadia Mono"
            font.pixelSize: 14
            Accessible.role: Accessible.StaticText
        }

        NumericSegment {
            id: secondsField
            objectName: root.objectPrefix + "Seconds"
            segmentName: "seconds"
            segmentMaximumLength: 2
            KeyNavigation.backtab: minutesField
            KeyNavigation.tab: fractionField
            Binding {
                target: secondsField
                property: "text"
                value: root.secondsText
                when: !secondsField.activeFocus
            }
        }

        Label {
            objectName: root.objectPrefix + "SecondsFractionSeparator"
            anchors.verticalCenter: hoursField.verticalCenter
            text: "."
            color: "#8A97A3"
            font.family: "Cascadia Mono"
            font.pixelSize: 14
            Accessible.role: Accessible.StaticText
        }

        NumericSegment {
            id: fractionField
            objectName: root.objectPrefix + "Fraction"
            segmentName: "fraction"
            segmentMaximumLength: 9
            KeyNavigation.backtab: secondsField
            KeyNavigation.tab: root.nextTabItem
            Binding {
                target: fractionField
                property: "text"
                value: root.fractionText
                when: !fractionField.activeFocus
            }
        }
    }
}
