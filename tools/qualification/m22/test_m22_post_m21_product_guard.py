#!/usr/bin/env python3
"""M22 post-M21 Product scope: allow verified-PRODUCT parity tests only.

This is deliberately NOT the old isolated M22 preproduction guard,
which rightfully rejected all changes to application integration.
It verifies that new PC-free Product evidence stays off main/M17/M18
and cannot silently touch any qualified DSP/FIR/source authority.
"""
from pathlib import Path
import subprocess

BASE = "1175e7588ca9273625e7c07225e2e3d22453d2e4"
allowed = (
    ".github/workflows/m22-preproduction.yml",
    ".github/workflows/m22-post-m21-integration.yml",
    "docs/architecture/M22_",
    "render/src/internal/m22_",
    "tools/qualification/m22/",
    "app/src/internal/m22_",
    # Explicit M19/M20/M21 Product owner binding on the M22 stack:
    # no broad app/analysis/CMake allowlist, no DSP/FIR/UI bypass.
    "app/src/internal/m21_product_owner_analysis.hpp",
    "app/src/internal/m21_product_publication_inbox.hpp",
    "docs/architecture/M21_POST_M20_REAL_PRODUCT_OWNER_CAPTURE_20261107.md",
    "tests/CMakeLists.txt",
    "tests/integration/app/test_project_session.cpp",
)
subprocess.run(["git", "merge-base", "--is-ancestor", BASE, "HEAD"], check=True)
changed = subprocess.check_output(
    ["git", "diff", "--name-only", f"{BASE}..HEAD"], text=True
).splitlines()
assert changed, "M22 post-M21 integration must have actual changes"
for path in changed:
    assert path.startswith(allowed), f"M22 changed protected predecessor: {path}"
for protected in (
    "app/src/main.cpp",
    "ui/qml/Main.qml",
    "analysis/src/true_peak_full_resource.cpp",
    "render/src/true_peak_limiter_terminal_candidate.cpp",
    "render/src/true_peak_limiter_terminal_check.cpp",
):
    assert protected not in changed, f"M22 changed frozen Product/DSP: {protected}"
header = Path("app/src/internal/m22_verified_master_preview_parity.hpp").read_text(
    encoding="utf-8"
)
assert "can_offer_verified_master" in header
assert "compensated_pcm.view()" in header
assert "full.subview(" in header
assert "capture_after_explicit_verified_consent" not in header
integration = Path("tests/integration/app/test_project_session.cpp").read_text(
    encoding="utf-8"
)
assert "m22_verified_master_preview_crop" in integration
assert "std::bit_cast<std::uint64_t>" in integration
print("M22_POST_M21_PRODUCT_SCOPE_GUARD_PASS_NON_FQ_NO_MAIN_MERGE")
