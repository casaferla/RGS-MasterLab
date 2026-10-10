import QtQuick
import QtQuick.Layouts

// Canonical pair for EVERY NEW numeric-box + parameter-slider control.
// The slider starts at numeric-box x and ends at numeric-box x + fieldWidth,
// NOT the numeric field's total implicitWidth (which includes units).
ColumnLayout {
    id: control
    spacing: 2

    property var viewModel: null
    property alias fieldObjectName: numericField.objectName
    property alias sliderObjectName: parameterSlider.objectName
    property alias labelText: numericField.labelText
    property alias unitText: numericField.unitText
    property alias fieldName: numericField.fieldName
    property alias rawText: numericField.rawText
    property alias compactFieldWidth: numericField.compactFieldWidth
    property alias compact: numericField.compact
    property alias interactionHint: numericField.interactionHint

    property alias from: parameterSlider.from
    property alias to: parameterSlider.to
    property alias value: parameterSlider.value
    property alias stepSize: parameterSlider.stepSize
    property alias logarithmic: parameterSlider.logarithmic
    property alias positionFillColor: parameterSlider.positionFillColor
    property alias fillFromOrigin: parameterSlider.fillFromOrigin
    property alias positionFillOrigin: parameterSlider.positionFillOrigin
    property alias accentColor: parameterSlider.accentColor
    property alias hasSemanticAccent: parameterSlider.hasSemanticAccent
    property alias semanticAccent: parameterSlider.semanticAccent
    property alias sliderAccessibleName: parameterSlider.accessibleName

    StudioNumericField {
        id: numericField
        viewModel: control.viewModel
        compact: true
    }

    StudioParameterSlider {
        id: parameterSlider
        viewModel: control.viewModel
        fieldName: numericField.fieldName
        compact: numericField.compact

        Layout.fillWidth: false
        Layout.alignment: Qt.AlignLeft
        Layout.minimumWidth: numericField.fieldWidth
        Layout.preferredWidth: numericField.fieldWidth
        Layout.maximumWidth: numericField.fieldWidth
    }
}
