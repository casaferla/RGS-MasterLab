#!/usr/bin/env python3
"""Emit chunked PNG CI evidence for the isolated R1 artistic qualification.

No network calls, no repository writes, no user data: only the empty-shell UI
screenshots created by the diagnostic --rgsml-skinpack-art-smoke invocation.
The chunks allow an independent review without modifying a shared CI workflow.
"""
import base64
import hashlib
from pathlib import Path
import sys

def main():
    if len(sys.argv) != 3 or sys.argv[2] not in ("1184", "1440"):
        raise SystemExit("usage: emit_art_screenshot.py FILE 1184|1440")
    path, label = Path(sys.argv[1]), sys.argv[2]
    payload = path.read_bytes()
    if not payload.startswith(b"\x89PNG\r\n\x1a\n") or len(payload) > 4_000_000:
        raise SystemExit("invalid or unexpectedly large image")
    digest = hashlib.sha256(payload).hexdigest()
    encoded = base64.b64encode(payload).decode("ascii")
    print(f"RGS_ART_SCREENSHOT_{label}_BEGIN size={len(payload)} sha256={digest}")
    for start in range(0, len(encoded), 3000):
        print(f"RGS_ART_SCREENSHOT_{label}_CHUNK {encoded[start:start + 3000]}")
    print(f"RGS_ART_SCREENSHOT_{label}_END")
    print(f"RGS_ART_SCREENSHOT_{label}_STORED path={path} sha256={digest}")

if __name__ == "__main__":
    main()
