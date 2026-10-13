"""Scoped G3 Qt Quick UI source guard; Windows physical Gate M remains open."""
from pathlib import Path

chain = Path("ui/qml/DspChainSelector.qml").read_text(encoding="utf-8")
gain = Path("ui/qml/InputGainEditor.qml").read_text(encoding="utf-8")
smoke = Path("tests/ui_smoke/test_source_metadata_panel.cpp").read_text(encoding="utf-8")


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise SystemExit("MINOR_FIX_G3_STATIC_UI_FAIL: " + reason)


require("implicitWidth: 180" in chain, "DSP Chain width changed")
for px in (8, 28):
    for kind in ("minimumWidth", "preferredWidth", "maximumWidth"):
        require(f"Layout.{kind}: {px}" in chain,
                "LED/BYP/error reservation changed")
require("anchors.leftMargin: 7" in chain and
        "anchors.rightMargin: 4" in chain and "spacing: 2" in chain,
        "compact row margin contract changed")
require("font.pixelSize: 12" in chain and
        "elide: Text.ElideRight" in chain,
        "title typography/fallback changed")
require("FIX-UI-004: Parametric EQ title must not be elided" in smoke,
        "native title non-elision test missing")

require('objectName: "inputGainWheelArea"' in gain,
        "transparent full-width wheel MouseArea missing")
require("anchors.fill: parent" in gain and
        "z: 5" in gain and
        "acceptedButtons: Qt.NoButton" in gain and
        "hoverEnabled: true" in gain and
        "enabled: gainSlider.enabled && !gainSlider.pressed" in gain,
        "full-width hover wheel / drag isolation changed")
require("wheel.angleDelta.y" in gain and
        "Math.trunc(gainSlider.wheelRemainder / 120)" in gain,
        "detent accumulation missing")
require("root.viewModel.setGainDb(next)" in gain and
        "wheel.accepted = true" in gain,
        "wheel must transact by VM without workspace scroll")
for test in ("sendGainWheel(0.03, 120)", "sendGainWheel(0.50, 120)",
             "sendGainWheel(0.97, 120)", "sendGainWheel(0.50, 60)",
             "gainViewModel.undo();"):
    require(test in smoke, "missing native L/C/R/partial/undo test " + test)
print("MINOR_FIX_G3_STATIC_UI_GUARD_PASS__PHYSICAL_GATE_M_OPEN")
