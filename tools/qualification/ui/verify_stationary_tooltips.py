#!/usr/bin/env python3
"""Bounded static contract for consistent stationary-hover hints across RGS QML.

This only verifies source wiring. Physical interaction remains a manual UI gate.
"""
from __future__ import annotations

from pathlib import Path
import re
import sys


def verify() -> int:
    root = Path(__file__).resolve().parents[3]
    files = sorted((root / "ui" / "qml").glob("*.qml"))
    helper = root / "ui" / "qml" / "StudioTooltipDwell.qml"
    cmake = (root / "ui" / "CMakeLists.txt").read_text(encoding="utf-8")
    errors: list[str] = []

    if "qml/StudioTooltipDwell.qml" not in cmake:
        errors.append("Common stationary hint component absent from QML module")
    source = helper.read_text(encoding="utf-8")
    for required in ("interval: 1000", "onPointChanged:", "onHoveredChanged:",
                     "onInteractingChanged:", "stationaryTimer.stop()"):
        if required not in source:
            errors.append(f"Common hint component missing {required}")

    count = 0
    for path in files:
        data = path.read_text(encoding="utf-8")
        hints = re.findall(r"^\s*ToolTip\.visible:[^\n]*", data, re.MULTILINE)
        delays = re.findall(r"^\s*ToolTip\.delay:[^\n]*", data, re.MULTILINE)
        labels = re.findall(r"^\s*ToolTip\.text:", data, re.MULTILINE)
        if not hints and not delays and not labels:
            continue
        count += len(hints)
        if len(hints) != len(delays) or len(hints) != len(labels):
            errors.append(f"{path.name}: mismatch in tooltip text/visible/delay")
        for hint in hints:
            if not re.search(r"\b[A-Za-z_]\w*\.ready\b", hint):
                errors.append(f"{path.name}: tooltip without stationary-ready gate: {hint.strip()}")
        for delay in delays:
            if not re.search(r"ToolTip\.delay:\s*0\s*$", delay):
                errors.append(f"{path.name}: attached tooltip has an independent delay: {delay.strip()}")
        if "ToolTip.show(" in data:
            errors.append(f"{path.name}: unmanaged imperative tooltip invocation")

    if count < 39:
        errors.append(f"Only {count} managed hints; expected at least 39 in this product UI")

    for error in errors:
        print("STATIONARY_TOOLTIP_POLICY_FAIL:", error, file=sys.stderr)
    if errors:
        return 1
    print(f"STATIONARY_TOOLTIP_POLICY_PASS: {count} QML tooltips share 1000 ms motion-reset policy")
    return 0


if __name__ == "__main__":
    raise SystemExit(verify())
