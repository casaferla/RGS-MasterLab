#!/usr/bin/env python3
"""Static and arithmetic minimum-scale regression for shared M16/M17 rail.

This is a synthetic geometry check, not physical Qt raster/visual qualification.
"""
from pathlib import Path

lane = Path("ui/qml/TruePeakStaticLane.qml").read_text(encoding="utf-8")
meter = Path("ui/qml/TruePeakMeterEditor.qml").read_text(encoding="utf-8")

def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit("TRUE_PEAK_MIN_ZERO_FAIL: " + message)

require('model: [6, 0, -12, -24, -36, -48, -60]' in lane,
        "frozen physical meter-scale labels changed")
require('objectName: "truePeakDigitalZeroGuide"' in lane,
        "physical zero guide missing")
require("Math.round(5 + 6.0 / 66.0 * (parent.height - 10))" in lane,
        "digital zero guide geometry changed")
require("Math.round(5 + 6 / 66" in lane and
        "* (parent.height - 10)) - height / 2" in lane,
        "text zero is not tied to its physical digital-zero guide")
require("Math.max(15" not in lane,
        "zero tick has a hard-coded 15px displacement at minimum scale")
require("x: modelData === 6 && root.compactVertical" in lane
        and "parent.height < 110 ? 12 : 0" in lane,
        "compact +6 glyph not staggered from physical 0")
require("font.pixelSize: root.compactVertical" in lane
        and "parent.height < 110 ? 8 : 10" in lane,
        "compact physical scale typography not reduced")
require("compactVertical: true" in meter,
        "M16/M17 shared meter no longer opts into compact geometry")

for track_height in (75, 76, 90, 109, 110, 120, 270):
    guide = round(5 + 6 / 66 * (track_height - 10))
    zero_center = round(5 + 6 / 66 * (track_height - 10))
    require(guide == zero_center,
            f"physical zero diverges at {track_height}px")
    require(0 <= zero_center <= track_height,
            f"zero outside meter track at {track_height}px")
    if track_height < 110:
        require(12 >= 8, "compact +6/zero glyphs can overlap horizontally")
print("TRUE_PEAK_MIN_ZERO_STATIC_AND_SYNTHETIC_GEOMETRY_PASS; PHYSICAL_UI_GATE_OPEN")
