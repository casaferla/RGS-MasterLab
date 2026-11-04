#!/usr/bin/env python3
"""M22 isolation check: never activate code in production or touch predecessor files."""
from pathlib import Path
import subprocess

BASE = "0a65990091f8ddc641888df114069d5e7db75fc4"
allowed = (
    "render/src/internal/m22_",
    "tools/qualification/m22/",
    "docs/architecture/M22_PARALLEL_PREPRODUCTION_BRANCH_CONTRACT_20261104.md",
    ".github/workflows/m22-preproduction.yml",
)
changed = subprocess.check_output(
    ["git", "diff", "--name-only", BASE + "..HEAD"], text=True
).splitlines()
assert changed, "M22 private changes missing"
for path in changed:
    assert path.startswith(allowed), f"M22 modifies a protected surface: {path}"
for p in ("app/src/main.cpp", "ui/qml/Main.qml", "render/CMakeLists.txt",
          "tests/CMakeLists.txt"):
    content = Path(p).read_text(encoding="utf-8")
    assert "m22_request_plan_candidate" not in content, f"M22 product activation: {p}"
print("M22_PRIVATE_SCOPE_GUARD_PASS_NOT_PRODUCTION_OR_GATE")
