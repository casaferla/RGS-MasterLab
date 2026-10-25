"""Scoped G3 Qt Quick UI source guard; Windows physical Gate M remains open."""
from pathlib import Path

chain = Path("ui/qml/DspChainSelector.qml").read_text(encoding="utf-8")
gain = Path("ui/qml/InputGainEditor.qml").read_text(encoding="utf-8")
smoke = Path("tests/ui_smoke/test_source_metadata_panel.cpp").read_text(encoding="utf-8")
monitor = Path("ui/qml/MonitorEditor.qml").read_text(encoding="utf-8")
icons = Path("ui/qml/StudioIcon.qml").read_text(encoding="utf-8")
adapters = Path("app/src/dsp_module_adapter.hpp").read_text(encoding="utf-8")


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise SystemExit("MINOR_FIX_G3_STATIC_UI_FAIL: " + reason)


require("implicitWidth: 180" in chain, "DSP Chain width changed")
# PO 2026-10-25 supersedes the former always-reserved 28px BYP badge.
# Configuration LED and red error slot stay at 8px; bypass is now an
# independent dark/muted row skin with readable status text and no reflow.
for kind in ("minimumWidth", "preferredWidth", "maximumWidth"):
    require(f"Layout.{kind}: 8" in chain,
            "LED/error reservation changed")
    require(f"Layout.{kind}: 28" not in chain,
            "obsolete BYP badge still reserving 28px")
require('dspChainBypassBadge_' not in chain,
        "old BYP badge is still instantiated")
require("isBypassed ? 0.28 : 1.0" in chain and
        '"Bypass · " + state' in chain,
        "bypass must be visible by muted row and explicit wording")
require('objectName: "dspChainUnifiedScroll"' in chain and
        "finalizationDock" not in chain and
        "terminalRepeater" not in chain,
        "one unified scroll list required: no anchored FINALIZATION")
require("contentWidth: width" in chain and
        "ScrollBar.horizontal.policy: ScrollBar.AlwaysOff" in chain and
        "width: chainScrollView.width" in chain,
        "scrollbar must overlay without stealing row/title width")
require("anchors.leftMargin: 7" in chain and
        "anchors.rightMargin: 4" in chain and "spacing: 2" in chain,
        "compact row margin contract changed")
require("font.pixelSize: 12" in chain and
        "elide: Text.ElideRight" in chain,
        "title typography/fallback changed")
# Module shape is stable (16px) and family hue is fixed despite bypass.
# This is independently enforced alongside native gain/EQ render assertions.
require('objectName: "dspChainModuleIcon_"' in chain and
        "Layout.minimumWidth: 16" in chain and
        "Layout.maximumWidth: 16" in chain,
        "fixed 16px module icon reservation missing")
for key,kind in (
        ("INPUT_GAIN","module-gain"),
        ("PARAMETRIC_EQ","module-eq"),
        ("COMPRESSOR","module-compressor"),
        ("STEREO_MS","module-stereo"),
        ("TRUE_PEAK_METER","module-peak-meter"),
        ("TRUE_PEAK_LIMITER","module-limiter"),
        ("MASTER_MONITOR","module-headphones")):
    require(f'case "{key}": return "{kind}"' in chain,
            f"missing icon mapping for {key}")
    require(f'kind === "{kind}"' in icons,
            f"unimplemented icon drawing for {key}")
require('return "#6683A1"' in chain and
        'return "#C4774A"' in chain and
        'return "#718092"' in chain,
        "analysis/dynamics/utility icon family accents have drifted")
require('QStringLiteral("Master Audition")' in adapters and
        'text: "Master Audition"' in monitor,
        "PO-approved Master Audition UI name missing")
require('QCOMPARE(chainGainIcon->width(),16.0)' in smoke or
        'QCOMPARE(chainGainIcon->width(), 16.0)' in smoke,
        "native fixed-width icon smoke assertion missing")
require("FIX-UI-004: Parametric EQ title must not be elided" in smoke,
        "native title non-elision test missing")
# The MONITOR family layout is deliberately NOT responsive in column count.
# At the Windows minimum the user must see the same spatial grammar, never
# MASTER and DEVICE magically move into a different vertical hierarchy.
require("columns: 2" in monitor and
        "columns: root.width" not in monitor,
        "MONITOR MASTER / DEVICE must never reflow from two columns to one")
require('objectName: "monitorMasterCard"' in monitor and
        'objectName: "monitorDeviceCard"' in monitor and
        'objectName: "monitorTransportWell"' in monitor,
        "MONITOR family wells and transport panel are missing")

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
