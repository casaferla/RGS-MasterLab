"""Minor Fix G3 source guard; physical Windows Gate M remains separate."""
from pathlib import Path

chain = Path("ui/qml/DspChainSelector.qml").read_text(encoding="utf-8")
gain = Path("ui/qml/InputGainEditor.qml").read_text(encoding="utf-8")
smoke = Path("tests/ui_smoke/test_source_metadata_panel.cpp").read_text(encoding="utf-8")

def require(ok: bool, reason: str) -> None:
    if not ok:
        raise SystemExit("MINOR_FIX_G3_STATIC_UI_FAIL: " + reason)

require("implicitWidth: 180" in chain, "DSP Chain width was changed")
for px in (8, 28):
    for kind in ("minimumWidth", "preferredWidth", "maximumWidth"):
        require(f"Layout.{kind}: {px}" in chain, "LED/BYP/error reserve changed")
require("anchors.leftMargin: 7" in chain and
        "anchors.rightMargin: 4" in chain and "spacing: 2" in chain,
        "G3 narrow row margins missing")
require("font.pixelSize: 12" in chain and "elide: Text.ElideRight" in chain,
        "chain label typography/fallback changed")
require("FIX-UI-004: Parametric EQ title must not be elided" in smoke,
        "title native smoke missing")
require('objectName: "inputGainWheelHandler"' in gain, "wheel handler absent")
require("target: null" in gain and
        "enabled: gainSlider.enabled && !gainSlider.pressed" in gain,
        "wheel/drag isolation missing")
require("event.angleDelta.y" in gain and
        "Math.trunc(gainSlider.wheelRemainder / 120)" in gain,
        "detent accumulator missing")
require("root.viewModel.setGainDb(next)" in gain and
        "event.accepted = true" in gain,
        "wheel must use VM and stop outer scrolling")
require("sendGainWheel(120)" in smoke and
        "sendGainWheel(-120)" in smoke and
        "gainViewModel.undo();" in smoke,
        "native positive/negative wheel + undo checks absent")
print("MINOR_FIX_G3_STATIC_UI_GUARD_PASS__PHYSICAL_GATE_M_OPEN")
