import QtQuick
import QtQuick.Shapes

// PO 2026-11-10: optical vector pictograms for M19/M20/M21,
// adapted from the approved RGS DSP icon-board visual language.
// Independent of the seven frozen core StudioIcon silhouettes.
// SVG PATH GEOMETRY, not raster assets; 24x24 vector master with a
// simplified optical rendition for the fixed 16x16 DSP Chain slot.
// M18 dither bars-to-dots is reserved for a future Export affordance.
// M22 is an integration gate, not a user-selectable DSP Chain module.
Item {
    id: root
    objectName: "analysisModuleVectorIcon"
    property string kind: ""
    property color strokeColor: "#6683A1"
    property bool glowEnabled: false
    implicitWidth: 16
    implicitHeight: 16
    clip: false

    readonly property bool small: Math.min(width,height) <= 20
    readonly property real designSize: 24
    // Native narrow-pixel optics: 2.55/24 * 16 = 1.70px.
    readonly property real opticalStroke: small ? 2.55 : 2.05

    // M19: calibrated loudness/level trace between fixed upper/lower
    // bounds. Not the M16 True Peak Meter's three-bar histogram.
    readonly property string m19Compact:
        "M 2.5 5 H 21.5 M 2.5 19 H 21.5 " +
        "M 3 12 H 6 L 8 15 L 11 7 L 13.2 13 L 15.5 10 L 18 12 H 21"
    readonly property string m19Large:
        "M 2.5 5 H 21.5 M 2.5 19 H 21.5 " +
        "M 3 12 H 6 L 8.2 16.4 L 11 6.6 L 13.3 13.8 " +
        "L 15.7 9.2 L 18.2 12 H 21"

    // M20: paired spectral contours, visually distinct from the
    // M/S branch, an equalizer fader and the single M19 level trace.
    readonly property string m20Compact:
        "M 2.5 10.5 L 5 8.5 L 7.5 10 L 10.7 4 L 13.2 9 " +
        "L 16 7 L 19 10.5 L 21.5 10.5 " +
        "M 2.5 15.5 L 5 13.5 L 7.5 15 L 10.7 9 L 13.2 14 " +
        "L 16 12 L 19 15.5 L 21.5 15.5"
    readonly property string m20Large:
        "M 2.5 11 L 4.8 8 L 7.5 10.2 L 10.8 3.5 " +
        "L 13.2 9.2 L 16.2 6.6 L 19.1 11 L 21.5 11 " +
        "M 2.5 15 L 4.8 12 L 7.5 14.2 L 10.8 7.5 " +
        "L 13.2 13.2 L 16.2 10.6 L 19.1 15 L 21.5 15"

    // M21: two independent measures, and a bidirectional DELTA
    // bracket. Deliberately no numeric value or fictitious metric.
    readonly property string m21Compact:
        "M 3 6 H 12.3 M 3 18 H 12.3 M 17.1 5 V 19 " +
        "M 14.6 7.6 L 17.1 5 L 19.6 7.6 " +
        "M 14.6 16.4 L 17.1 19 L 19.6 16.4"
    readonly property string m21Large:
        "M 2.5 6 H 7 L 9.2 3.8 L 11.4 6 H 13 " +
        "M 2.5 18 H 7 L 9.2 15.8 L 11.4 18 H 13 " +
        "M 18 4.5 V 19.5 " +
        "M 15 7.5 L 18 4.5 L 21 7.5 " +
        "M 15 16.5 L 18 19.5 L 21 16.5"

    // M18 final Export Dither concept from the supplied icon board:
    // graduated level bars transforming into isolated precision dots.
    // Defined for reuse, NOT added to the current DSP Chain model.
    readonly property string m18OutputDither:
        "M 3 20 V 16.5 M 7 20 V 12 M 11 20 V 8 " +
        "M 15 7 V 7.2 M 19 9 V 9.2 M 16 12.5 V 12.7 " +
        "M 20 14 V 14.2 M 15 18 V 18.2 M 20 19 V 19.2"

    readonly property string vectorPath: kind === "analysis-levels"
        ? (small ? m19Compact : m19Large)
        : kind === "analysis-spectrum"
        ? (small ? m20Compact : m20Large)
        : kind === "analysis-delta"
        ? (small ? m21Compact : m21Large)
        : kind === "module-output-dither" ? m18OutputDither : ""

    Shape {
        id: vector
        objectName: "analysisVectorShape"
        anchors.centerIn: parent
        width: root.designSize
        height: root.designSize
        scale: Math.max(0.01,Math.min(root.width,root.height)/root.designSize)
        visible: root.vectorPath.length > 0
        // Subtle same-hue second stroke: NO independent LED or raster glow.
        ShapePath {
            strokeColor: Qt.rgba(root.strokeColor.r,root.strokeColor.g,
                root.strokeColor.b,root.glowEnabled ? 0.20 : 0)
            strokeWidth: root.opticalStroke + 2.1
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            fillColor: "transparent"
            PathSvg { path: root.vectorPath }
        }
        ShapePath {
            strokeColor: root.strokeColor
            strokeWidth: root.opticalStroke
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            fillColor: "transparent"
            PathSvg { path: root.vectorPath }
        }
    }
}
