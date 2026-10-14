#!/usr/bin/env python3
"""Minor Fix G1 QML source contract smoke: NO graphical/physical PASS claim.

The app's Windows UI smoke and later actual visual acceptance remain
separate. This guard catches accidental removal of reactive bypass paint,
unintentional opacity changes in the GR HISTORY, or the redundant header.
"""
from pathlib import Path
import sys


def check() -> None:
    root = Path(__file__).resolve().parents[3]
    qml = (root / "ui/qml/CompressorEditor.qml").read_text(encoding="utf-8")
    if 'text: "DYNAMICS COMPRESSOR"' in qml:
        raise ValueError("UX-003 redundant editor heading still present")
    start = qml.index("id: curveCanvas")
    end = qml.index("id: curveMouseArea", start)
    curve = qml[start:end]
    if curve.count("onIsBypassedChanged: requestPaint()") != 1:
        raise ValueError("FIX-UI-006 reactive repaint missing")
    if 'readonly property bool isBypassed: root.viewModel ? root.viewModel.bypass : false' not in curve:
        raise ValueError("FIX-UI-006 missing committed bypass binding")
    grid = curve.index("// Grid Lines")
    alpha = curve.index("ctx.globalAlpha = isBypassed ? 0.5 : 1.0")
    transfer = curve.index("// Dynamics Copper Transfer Curve")
    handles = curve.index("// C++ Transfer Curve Handles")
    restore = curve.index("ctx.globalAlpha = 1.0")
    if not (grid < alpha < transfer < handles < restore):
        raise ValueError("FIX-UI-006 must fade only static response and handles")
    if curve.count("ctx.globalAlpha = ") != 2:
        raise ValueError("FIX-UI-006 unexpected alpha scope")
    if 'id: grRibbonCanvas' not in qml[end:]:
        raise ValueError("GR HISTORY canvas missing / inadvertently changed")
    if 'text: "STATIC TRANSFER CURVE"' not in qml:
        raise ValueError("Static transfer label accidentally removed")
    if 'objectName: "resetCompressorButton"' not in qml:
        raise ValueError("Reset button displaced/missing")
    if 'onIsBypassedChanged: requestPaint()' not in curve:
        raise ValueError("Bypass state lacks paint invalidation")


if __name__ == "__main__":
    try:
        check()
    except (ValueError, OSError) as exc:
        print("MINOR_FIX_G1_QML_STATIC_GUARD_FAIL: " + str(exc),
              file=sys.stderr)
        sys.exit(1)
    print("MINOR_FIX_G1_QML_STATIC_GUARD_PASS__PHYSICAL_VISUAL_PENDING")
