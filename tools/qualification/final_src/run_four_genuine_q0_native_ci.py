#!/usr/bin/env python3
"""Fail-closed Linux Focused NATIVE Qt execution of four genuine MPFR Q0 canaries.

This invokes real project rgsml_playback_src_tests, not standalone C++ or
Python-mocked test coverage. Four independently frozen Q0 asset+PCM pins
must match, four named Qt tests must each explicitly PASS, none may SKIP.
This verifies 4 ratios (not 14), and does NOT authorize product Final SRC
admission, full DTFT response, full Q1-Q6 or M18.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import re
import subprocess
import sys
import time

from phase11_math_preflight import ContractError

NATIVE_TESTS = (
    "phase11GenuineMpfrCanaryMatchesPythonIndependentScalarBits",
    "phase11GenuineMpfrOneToTwoDownsampleMatchesPythonScalarBits",
    "phase11GenuineMpfrFourfoldRatesMatchPythonScalarBits",
    "phase11GenuineQ0OneM17PassThenIndependentDeviceRate",
)
# Real source artifacts generated in THIS run, originally independently
# frozen and rechecked against Q0 Drive evidence. Not public product assets.
PINS = (
    ("two", "6fb38a7d62859283e4061a18d23f354a8308d5b22ff55623e8b1c9a245e8b00c",
     "d3346aa1d9b70507808d10c32450310869859a109c9d556c5a75482d880890cb"),
    ("one", "b97776527687180bea6dbf86a8da877c0bdaa528d305abe4b7beaf806ecaf48e",
     "384ba19d6559c081cd61d49ec8c6d13afdf3890cbb2c4da828b7b58bb2900374"),
    ("one_fourth", "3c5db068904a059fae54ab3a96ae96ec3963fc31e649f5f4de9ecf5b6d17a947",
     "54b5d7a2358439a926f53055d2d517f61d483ffbfda87aff7ae0ce56ea99a777"),
    ("four", "f2563518411bc6525889eaf82c43b194f88ceadd59b0750071c613d1071de095",
     "d2483af9f1bfd6516f806280632d78581ed24efb39640daf2c10c6f0495a7f25"),
)
NONPASS = re.compile(r"^\s*(?:FAIL|SKIP|XFAIL|XPASS)\s*:", re.MULTILINE)


def _require_pin(path: Path, digest: str) -> None:
    if not path.is_file() or sha256(path.read_bytes()).hexdigest() != digest:
        raise ContractError("genuine Q0 or independent PCM SHA mismatch: " + str(path))


def _native_binary(build_dir: Path) -> Path:
    if not build_dir.is_dir():
        raise ContractError("native build directory unavailable")
    candidates = [p for p in build_dir.rglob("rgsml_playback_src_tests")
                  if p.is_file() and p.stat().st_mode & 0o111]
    if len(candidates) != 1:
        raise ContractError("expected exactly one executable project-native Qt SRC test")
    return candidates[0]


def execute(*, build_dir: Path, two: Path, one: Path,
            fourfold_dir: Path, report: Path,
            timeout_seconds: int = 240) -> dict:
    if report.exists() or report.is_symlink() or not report.parent.is_dir():
        raise ContractError("native Q0 report must be a new file")
    if type(timeout_seconds) is not int or not 15 <= timeout_seconds <= 1200:
        raise ContractError("invalid native Q0 time budget")
    originals = (
        two, one, fourfold_dir / "ratio_L1_M4.bin",
        fourfold_dir / "ratio_L4_M1.bin",
    )
    for path, (_, h, pcm_h) in zip(originals, PINS):
        _require_pin(path, h)
        _require_pin(Path(str(path) + ".expected.f64le"), pcm_h)
    exe = _native_binary(build_dir)
    environment = __import__("os").environ.copy()
    # This driver launches QtTest DIRECTLY, bypassing CTest's explicit
    # ENVIRONMENT_MODIFICATION. The newly externalized frozen playback FIRs
    # must be provided in the exact same build-generated root. Do not run
    # MPFR canaries against an implicit system/user or network fallback.
    playback_root = build_dir.resolve() / "generated" / "rgsml" / "fir" / "playback"
    playback_names = (
        "playback_src_kernel_44100_48000.bin",
        "playback_src_kernel_48000_44100.bin",
    )
    if any(not (playback_root / name).is_file() for name in playback_names):
        raise ContractError("native Q0 canary frozen external playback FIR pack missing")
    environment.update({
        "RGSML_PLAYBACK_FIR_ROOT": str(playback_root),
        "RGSML_PHASE11_REAL_CANARY_PATH": str(two.resolve()),
        "RGSML_PHASE11_REAL_DOWNSAMPLE_PATH": str(one.resolve()),
        "RGSML_PHASE11_REAL_POW4_PATH": str(fourfold_dir.resolve()),
    })
    results = []
    started = time.monotonic()
    for test in NATIVE_TESTS:
        try:
            process = subprocess.run(
                [str(exe.resolve()), test, "-o", "-,txt"],
                env=environment, capture_output=True, text=True,
                encoding="utf-8", errors="replace",
                timeout=timeout_seconds, check=False)
        except subprocess.TimeoutExpired as exc:
            raise ContractError("native Qt Q0 canary timed out: " + test) from exc
        output = process.stdout + "\n" + process.stderr
        # Do not accept an unrelated PASS, a missing method or a silent SKIP.
        passed = re.findall(
            r"^PASS\s*:\s*[^\n]*::" + re.escape(test) +
            r"\s*\(\s*\)", output, re.MULTILINE)
        if process.returncode != 0 or len(passed) != 1 or NONPASS.search(output):
            tail = "\n".join(output.splitlines()[-12:])[:1600]
            raise ContractError("native genuine-Q0 test failed/skipped/missing "
                                + test + ":\n" + tail)
        results.append({"test": test, "status": "NATIVE_QT_PASS_UNSKIPPED"})
    receipt = {
        "schema": "rgsml.phase11.four-original-Q0-native-project-ci/1",
        "status": "FOUR_GENUINE_Q0_NATIVE_QT_TESTS_PASS_NONRELEASE",
        "native_binary_sha256": sha256(exe.read_bytes()).hexdigest(),
        "genuine_distinct_ratios": 4,
        "executed_named_native_qt_tests": len(results),
        "source_master_full_family_verified": False,
        "fourteen_asset_family_native_verified": False,
        "complete_q1": False,
        "full_continuum_dtft_proven": False,
        "q1_q6_passed": False,
        "product_admission_authorized": False,
        "release_qualified": False,
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "results": results,
    }
    report.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    return receipt


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build-dir", required=True, type=Path)
    p.add_argument("--two", required=True, type=Path)
    p.add_argument("--one", required=True, type=Path)
    p.add_argument("--fourfold-dir", required=True, type=Path)
    p.add_argument("--report", required=True, type=Path)
    a = p.parse_args()
    result = execute(build_dir=a.build_dir, two=a.two, one=a.one,
                     fourfold_dir=a.fourfold_dir, report=a.report)
    print(result["status"])


if __name__ == "__main__":
    main()
