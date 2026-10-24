#!/usr/bin/env python3
"""M16-B2d: exact-build extreme binary64 / error-path production-C++ probe.

No EBU material; no audio files or decoded PCM written or uploaded.
This is a deliberately bounded, fail-closed supplement, NOT Gate G closure.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys

BASE_SOURCE_SHA = "e432872371d1e39bf92a8275213a5b1357740875"
BASE_RUN_ID = "38300690868"
BASE_PROBE_SHA = "9d9d10fd01bbc791a8bc283015c18c9b6e1c4f5014bedf0434b4284e94b6614f"
MINSUB = float.fromhex("0x0.0000000000001p-1022")
MINNORMAL = float.fromhex("0x1p-1022")
DBL_MAX = float.fromhex("0x1.fffffffffffffp+1023")
IMPULSE_NUM = 7964.0 / 8192.0
# Independently recorded M16 pre-audit real-build fixture observations,
# NOT coefficients dynamically loaded from or computed by the production bank.
EXPECTED_MINNORMAL_P = float.fromhex("0x0.f8e0000000000p-1022")
EXPECTED_MAX_P = float.fromhex("0x1.f1bffffffffffp+1023")
PHASE1_NUMERATORS = (-239, 240, -424, 730, -1364, 3810,
                     6388, -1641, 832, -477, 271, -155)


class Failure(Exception):
    pass


def provenance_ok(probe: Path, stamp: Path, head: str, run: str,
                  pinned_probe_sha: str | None) -> str:
    if not probe.is_file() or not stamp.is_file():
        raise Failure("Missing probe/provenance")
    obj = json.loads(stamp.read_text(encoding="utf-8-sig"))
    actual = hashlib.sha256(probe.read_bytes()).hexdigest()
    if (obj.get("source_commit_sha") != head
            or str(obj.get("workflow_run_id")) != run
            or obj.get("probe_exe_sha256") != actual
            or obj.get("production_gate_g") != "NOT_EXECUTED"
            or obj.get("test_scope") !=
               "SYNTHETIC_ONLY__OFFICIAL_EBU_WAVS_NOT_EXECUTED"
            or (pinned_probe_sha is not None and
                actual != pinned_probe_sha)):
        raise Failure("Exact-head build provenance/hash mismatch")
    return actual


def invoke(probe: Path, left: list[float], right: list[float],
           rate: int = 48000, extend: bytes = b"") -> subprocess.CompletedProcess:
    if len(left) != len(right):
        raise Failure("Unequal channels")
    buf = bytearray(16 * len(left))
    for i, (l, r) in enumerate(zip(left, right)):
        struct.pack_into("<dd", buf, 16 * i, l, r)
    return subprocess.run(
        [str(probe.resolve()), str(rate), str(len(left))],
        input=bytes(buf) + extend, capture_output=True,
        check=False, timeout=45,
    )


def accepted_peak(result: subprocess.CompletedProcess, expected: float,
                  *, label: str, silent_right: bool = True) -> None:
    if result.returncode != 0:
        raise Failure(f"{label}: expected success; exit={result.returncode}")
    try:
        obj = json.loads(result.stdout.decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as exc:
        raise Failure(f"{label}: invalid C++ JSON") from exc
    for name in ("left_linear_peak", "global_linear_peak"):
        actual = obj.get(name)
        if not isinstance(actual, (float, int)) or actual != expected:
            raise Failure(f"{label}: {name}: {actual!r} != {expected!r}")
    if silent_right and (obj.get("right_linear_peak") != 0.0
                         or obj.get("right_dbtp") is not None):
        raise Failure(f"{label}: right silence contract violated")
    if (obj.get("production_gate_g") != "NOT_EXECUTED"
            or obj.get("is_exact_silence") is True):
        raise Failure(f"{label}: incorrect truthfulness/silence")
    dbtp = obj.get("global_dbtp")
    reference = 20.0 * math.log10(expected)
    if not isinstance(dbtp, (int, float)) or not math.isfinite(dbtp):
        raise Failure(f"{label}: missing/nonfinite dBTP")
    if abs(dbtp - reference) > 0.01:
        raise Failure(f"{label}: dBTP not within frozen RGS 0.01 criterion")


def rejected(result: subprocess.CompletedProcess, label: str) -> None:
    if (result.returncode == 0 or result.stdout.strip()
            or b"B2B_PRODUCTION_PROBE_FAILED" not in result.stderr):
        raise Failure(f"{label}: failure not categorical/clean "
                      f"(exit={result.returncode})")


def selftest() -> None:
    assert MINSUB > 0.0 and MINNORMAL > MINSUB
    assert math.isfinite(DBL_MAX)
    assert EXPECTED_MINNORMAL_P < MINNORMAL
    assert math.isfinite(EXPECTED_MAX_P) and EXPECTED_MAX_P < DBL_MAX
    assert len(PHASE1_NUMERATORS) == 12
    assert sum(abs(i) for i in PHASE1_NUMERATORS) == 16571
    assert 20.0 * math.log10(IMPULSE_NUM) < 0.0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--selftest", action="store_true")
    p.add_argument("--probe", type=Path)
    p.add_argument("--provenance", type=Path)
    p.add_argument("--expected-source-sha")
    p.add_argument("--expected-run-id")
    a = p.parse_args()
    try:
        selftest()
        if a.selftest:
            print(json.dumps({"status": "B2D_EXTREME_ORACLE_SELFTEST_PASS",
                              "production_gate_g": "NOT_COMPLETE"}))
            return 0
        if a.probe is None or a.provenance is None:
            raise Failure("Probe and provenance are mandatory")
        if bool(a.expected_source_sha) != bool(a.expected_run_id):
            raise Failure("Source SHA and run ID must be supplied together")
        fresh = a.expected_source_sha is not None
        head = a.expected_source_sha or BASE_SOURCE_SHA
        run = a.expected_run_id or BASE_RUN_ID
        digest = provenance_ok(a.probe, a.provenance, head, run,
                               None if fresh else BASE_PROBE_SHA)
        counts = {"tiny": 0, "large": 0, "nonfinite": 0,
                  "overflow": 0, "framing": 0, "unsupported": 0}
        for x, expected, name in (
            (MINSUB, MINSUB, "minsub"),
            (-MINSUB, MINSUB, "negative_minsub"),
            (MINNORMAL, EXPECTED_MINNORMAL_P, "minnormal"),
            (-MINNORMAL, EXPECTED_MINNORMAL_P, "negative_minnormal"),
        ):
            accepted_peak(invoke(a.probe, [x], [0.0]), expected, label=name)
            counts["tiny"] += 1
        for x, name in ((DBL_MAX, "dbl_max"), (-DBL_MAX, "negative_dbl_max")):
            accepted_peak(invoke(a.probe, [x], [0.0]), EXPECTED_MAX_P,
                          label=name)
            counts["large"] += 1
        # The sum of abs phase-1 tap numerators /8192 exceeds 1.
        # Each multiplication remains finite, but accumulation must overflow,
        # and fail rather than clamp or silently select another maximum.
        over = [0.0] * 12
        for lag, coeff in enumerate(PHASE1_NUMERATORS):
            over[11 - lag] = DBL_MAX if coeff > 0 else -DBL_MAX
        rejected(invoke(a.probe, over, [0.0] * 12), "overflow_signed")
        counts["overflow"] += 1
        # First/middle/last after valid large peaks on left and right.
        for bad, code in ((math.nan, "nan"),
                          (math.inf, "posinf"), (-math.inf, "neginf")):
            for ch in range(2):
                for pos in (0, 2, 4):
                    left = [2.0, -1.0, 0.125, 0.375, -0.25]
                    right = [0.5, -0.5, 0.25, -0.125, 0.125]
                    (left if ch == 0 else right)[pos] = bad
                    rejected(invoke(a.probe, left, right),
                             f"{code}/channel={ch}/position={pos}")
                    counts["nonfinite"] += 1
        rejected(invoke(a.probe, [1.0], [0.0], extend=b"\x00"),
                 "extra_framing_byte")
        counts["framing"] += 1
        rejected(invoke(a.probe, [1.0], [0.0], rate=32000),
                 "unsupported_32000hz")
        counts["unsupported"] += 1
        if counts != {"tiny": 4, "large": 2, "nonfinite": 18,
                      "overflow": 1, "framing": 1, "unsupported": 1}:
            raise Failure("Unexpected fixture count")
    except (Failure, OSError, ValueError, OverflowError, TypeError,
            AssertionError, subprocess.TimeoutExpired) as exc:
        print(json.dumps({"status": "FAIL_CLOSED",
                          "reason": str(exc),
                          "production_gate_g": "NOT_COMPLETE"}, indent=2),
              file=sys.stderr)
        return 1
    print(json.dumps({
        "status": "B2D_EXTREME_FP_CPP_COMPARISON_PASS",
        "scope": "RGS_SYNTHETIC_NOT_EBU",
        "source_build_sha": head,
        "workflow_run_id": run,
        "probe_exe_sha256": digest,
        "counts": counts,
        "cases": sum(counts.values()),
        "production_gate_g": "NOT_COMPLETE",
    }, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
