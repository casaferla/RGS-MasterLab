#!/usr/bin/env python3
"""B3: fail-closed production CMake scope, preserving the ORIGINAL B2i blob lock.

No B2i verifier, B2i evidence matrix, numerical FIR, algorithm authority,
release status or numeric dBTP computation may be changed to pass B3.
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
LOCKED_CMAKE_BLOB_SHA = "dfca065ab8b1fb7488b044d3b81bc5d5dda78a97"
DECLARATION = '''# B3: add the non-qualifying public DTO adapter OUTSIDE the cryptographically
# locked M16 numerical target definition. The frozen analysis/CMakeLists.txt
# and its strict-FP source list are preserved byte-for-byte by the B2i guard.
target_sources(rgsml_analysis PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/analysis/src/true_peak_measurement.cpp")'''

def require(ok: bool, reason: str) -> None:
    if not ok:
        raise RuntimeError(reason)

def git_blob(path: str) -> str:
    run = subprocess.run(
        ["git", "hash-object", "--", path], cwd=ROOT,
        text=True, capture_output=True, check=False,
    )
    require(run.returncode == 0, "Cannot check protected hash: " + path)
    return run.stdout.strip()

def main() -> int:
    try:
        require(git_blob("analysis/CMakeLists.txt") == LOCKED_CMAKE_BLOB_SHA,
                "B2i protected analysis/CMakeLists.txt MUST NOT change")
        root = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        require(root.count(DECLARATION) == 1,
                "Exactly one B3 adapter target_sources binding required")
        require(root.index("add_subdirectory(analysis)") <
                root.index(DECLARATION) < root.index("add_subdirectory(render)"),
                "B3 adapter must be appended after ANALYSIS target creation")
        internal = (ROOT / "analysis/src/true_peak_measurement.cpp").read_text(
            encoding="utf-8")
        header = (ROOT / "analysis/include/rgsml/analysis/true_peak_measurement.hpp").read_text(
            encoding="utf-8")
        require(internal.count("measure_unqualified_true_peak_full_resource(resource)") == 1,
                "B3 must reuse exactly one frozen internal numeric measurement")
        require("qualification_status_ =" not in internal,
                "No runtime status promotion in B3")
        require("TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED" in header,
                "B3 must fail closed on Gate G")
        require("true_peak_measurement.cpp" not in
                (ROOT / "analysis/CMakeLists.txt").read_text(encoding="utf-8"),
                "Adapter must not enter locked M16 CMake scope")
    except (RuntimeError, OSError) as e:
        print("M16_B3_CMAKE_SOURCE_BOUNDARY_FAIL: " + str(e), file=sys.stderr)
        return 1
    print("M16_B3_CMAKE_SOURCE_BOUNDARY_PASS__B2I_LOCK_INTACT__GATE_G_OPEN")
    return 0

if __name__ == "__main__":
    sys.exit(main())
