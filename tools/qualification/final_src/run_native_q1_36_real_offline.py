#!/usr/bin/env python3
"""Run EXACT 36-real-Q1-vector test against an existing native Qt C++ binary.

Requires the two permanently archived private genuine nonrelease ZIPs from
RGS ContentBox, extracted by a SHA-locked audit. Skips, missing fixtures,
Qt missing test, malformed hashes, or nonzero exit are HARD FAILURES.
Does not certify Q1-Q6 or authorize production SRC/limiter release.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import time

from phase11_math_preflight import ContractError
from prepare_real_q1_36_native_rehearsal import prepare

TEST_NAME = "phase11External14RealCandidatesAll36OfflineNativeRehearsal"
EXACT_PASS = re.compile(
    r"^PASS\s+:\s+[^\n]*::" + re.escape(TEST_NAME) + r"\s*\(\s*\)",
    re.MULTILINE,
)


def _sha_stream(path: Path) -> str:
    h = sha256()
    with path.open("rb") as inp:
        for chunk in iter(lambda: inp.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def execute(exe: Path, candidate_zip: Path, oracle_zip: Path,
            workdir: Path, report: Path, *, timeout_seconds=900) -> dict:
    if not exe.is_file() or not os.access(exe, os.X_OK):
        raise ContractError("native C++ test executable is unavailable")
    if (report.exists() or report.is_symlink() or
        not report.parent.is_dir() or
        report.resolve().is_relative_to(workdir.resolve())):
        raise ContractError("report must be new and outside immutable fixture cache")
    if not 30 <= timeout_seconds <= 1800:
        raise ContractError("native qualification timeout outside safe budget")
    fixture = prepare(candidate_zip, oracle_zip, workdir)
    if (fixture.get("real_kernels") != 14 or
        fixture.get("expected_paths") != 36 or
        fixture.get("product_admission_authorized") is not False):
        raise ContractError("native rehearsal does not have exact Q0+Q1 authority")
    env = os.environ.copy()
    env["RGSML_PHASE11_REAL_14_Q0_DIR"] = fixture["assets_dir"]
    env["RGSML_PHASE11_REAL_36_Q1_DIR"] = fixture["oracle_dir"]
    started = time.monotonic()
    try:
        result = subprocess.run(
            [str(exe.resolve()), TEST_NAME, "-o", "-,txt"],
            env=env, capture_output=True, text=True, encoding="utf-8",
            errors="replace", timeout=timeout_seconds, check=False)
    except subprocess.TimeoutExpired as exc:
        raise ContractError("genuine native-Q1 rehearsal timed out; no PASS") from exc
    combined = result.stdout + "\n" + result.stderr
    if (result.returncode != 0 or not EXACT_PASS.search(combined) or
        re.search(r"^\s*(FAIL|SKIP|XPASS|XFAIL)\s*:", combined, re.MULTILINE)):
        # Bounded errors only; never dump full CI logs or raw audio.
        tail = "\n".join(combined.splitlines()[-16:])[:3000]
        raise ContractError("Q1 native test missing, failed or skipped:\n" + tail)
    proof = {
        "schema": "rgsml.phase11.native-q1-36-real-run-rehearsal/1",
        "status": "ALL_36_REAL_Q1_NATIVE_VECTORS_MATCH_NONRELEASE",
        "native_test": TEST_NAME,
        "platform": platform.platform(),
        "native_executable_sha256": _sha_stream(exe),
        "frozen_candidate_zip_sha256": _sha_stream(candidate_zip),
        "frozen_q1_vectors_zip_sha256": _sha_stream(oracle_zip),
        "real_kernels": 14,
        "source_master_pairs_verified": 36,
        "native_test_not_skipped": True,
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "q1_full_conformance": False,
        "q2_q6_complete": False,
        "full_continuum_dtft_proven": False,
        "release_qualified": False,
        "product_admission_authorized": False,
    }
    report.write_text(json.dumps(proof, sort_keys=True, indent=2) + "\n")
    return proof


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--native-exe", type=Path, required=True)
    p.add_argument("--candidate-zip", type=Path, required=True)
    p.add_argument("--q1-reference-zip", type=Path, required=True)
    p.add_argument("--new-fixture-dir", type=Path, required=True)
    p.add_argument("--report", type=Path, required=True)
    p.add_argument("--timeout-seconds", type=int, default=900)
    a = p.parse_args()
    print(json.dumps(execute(
        a.native_exe, a.candidate_zip, a.q1_reference_zip,
        a.new_fixture_dir, a.report, timeout_seconds=a.timeout_seconds),
        sort_keys=True))


if __name__ == "__main__":
    main()
