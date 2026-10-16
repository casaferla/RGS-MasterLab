#!/usr/bin/env python3
"""Static, no-dependencies contract checks for the *candidate* SkinPack workflow.
Run: python tools/skinpack/ci/check_experimental_workflow.py
These are NOT YAML parser or live Actions build tests.
"""
from pathlib import Path

p = Path(__file__).with_name("skinpack-experimental.yml")
text = p.read_text(encoding="utf-8")
expected = "experimental/skinpack-r1-r4-unqualified-20261016"
assert text.startswith("# RGS SkinPack EXPERIMENTAL ONLY")
assert text.count("\non:\n") == 1
assert "\non:\n  push:\n    branches:\n      - " + expected + "\n    paths:\n      - skinpack/ci/run-request.txt\n" in text
assert text.count("      - skinpack/ci/run-request.txt") == 1
assert "\npermissions:\n  contents: read\n" in text
assert "persist-credentials: false" in text
assert "runs-on: windows-2022" in text
assert "timeout-minutes: 50" in text
assert "rgsml_app" not in text or "qt_r1_probe.ps1" in text
assert "Qt 6.8.3" in text and "win64_msvc2022_64" in text
for banned in ("workflow_dispatch:", "pull_request:", "repository_dispatch:", "schedule:", "secrets.", "contents: write", "id-token: write", "uses: actions/create-release", "branches:\n      - main"):
    assert banned not in text, banned
assert "tools/skinpack/qt_r1_probe.ps1" in text
print("SKINPACK_EXPERIMENTAL_WORKFLOW_STATIC_GATES=PASS")
