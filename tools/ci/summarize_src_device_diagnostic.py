#!/usr/bin/env python3
"""Only bounded QtTest SRC assertions and progress markers; never a whole log."""
from __future__ import annotations
import argparse
from pathlib import Path
import re
p=argparse.ArgumentParser()
p.add_argument("--log",required=True)
p.add_argument("--output",required=True)
p.add_argument("--exit-code",type=int,required=True)
a=p.parse_args()
log=Path(a.log)
lines=log.read_text(encoding="utf-8",errors="replace").splitlines() if log.is_file() else []
kept=[]
last_case=""
for line in lines:
    s=line.strip()
    if "PlaybackSampleRateAdapterTest::" in s:
        matched=re.search(r"(PlaybackSampleRateAdapterTest::[^()]*)\(",s)
        if matched:
            last_case=matched.group(1)
    if any(t in s for t in ("FAIL!", "QFATAL", "QWARN", "DEVICE_SRC_192_TO_48_STEREO", "Totals:", "********* Finished", "ASSERTION")):
        kept.append(s[:350])
out=["RGS SRC WIN RELEASE TEST — BOUNDED FAIL-ONLY DIAGNOSTIC",
     "No full CTest/GitHub logs or source PCM in this artifact.",
     f"Exit code: {a.exit_code}",
     f"Last observable QtTest method: {last_case or 'NOT EMITTED'}",
     f"QtTest log created: {bool(lines)}",
     f"Diagnostic lines: {min(len(kept),16)}"]
out.extend(kept[:16])
Path(a.output).write_text("\n".join(out)[:3800]+"\n",encoding="utf-8")
print("SRC_WIN_BOUNDED_DIAGNOSTIC_CREATED")
