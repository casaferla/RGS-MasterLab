#!/usr/bin/env python3
"""Bounded M18 Windows QtTest crash/FAIL diagnostic (never a whole job log)."""
from __future__ import annotations
import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument("--log", required=True)
parser.add_argument("--output", required=True)
parser.add_argument("--exit-code", type=int, required=True)
args = parser.parse_args()
file = Path(args.log)
lines = file.read_text(encoding="utf-8", errors="replace").splitlines() if file.is_file() else []
test = "M18CandidateDeliveryValidationTest"
last_method = ""
selected: list[str] = []
for line in lines:
    stripped = line.strip()
    found = re.search(r"M18CandidateDeliveryValidationTest::([a-zA-Z_][a-zA-Z_0-9]*)\(", stripped)
    if found:
        last_method = found.group(1)
    if any(token in stripped for token in (
        "FAIL!", "QFATAL", "ASSERTION", "Unknown test function",
        "Totals:", "Cannot load", "0xc000", "Fatal error", "QWARN")):
        selected.append(stripped[:340])
summary = [
    "RGS M18 WINDOWS QTTEST — BOUNDED DIRECT DIAGNOSTIC",
    "NO FULL CTEST/JOB LOG OR PERSONAL PCM; ONLY EXIT/METHOD/ASSERTIONS.",
    f"EXIT_CODE={args.exit_code}",
    f"RAW_LOG_PRESENT={bool(lines)}",
    f"LAST_METHOD={last_method or 'NOT_EMITTED'}",
    f"FILTERED_LINES={min(len(selected), 14)}",
    *selected[:14],
]
Path(args.output).write_text("\n".join(summary)[:3900] + "\n", encoding="utf-8")
print("M18_WINDOWS_QTEST_BOUNDED_DIAGNOSTIC_WRITTEN")
