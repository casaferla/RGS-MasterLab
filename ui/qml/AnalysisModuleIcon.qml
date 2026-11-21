import QtQuick

// Backward-compatible QML name only. Not instantiated by DspChainSelector.
// All vector paths, glow, dim, bypass and repaint behavior live exclusively
// in StudioIcon's Qt Canvas 2D, including M19/M20/M21. No Shapes/PathSvg.
StudioIcon {
    implicitWidth: 16
    implicitHeight: 16
}
