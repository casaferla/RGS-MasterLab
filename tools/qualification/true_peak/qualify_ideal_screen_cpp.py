#!/usr/bin/env python3
"""M16-B2e: high-precision ideal reconstruction SCREEN, never a certificate.

Use independent sinc interpolation of actual exact binary64 source samples.
Do NOT compare ideal peak with RGS 0.01-dB QTP: that tolerance belongs to
the frozen ITU 4x discrete estimator. Requires mpmath==1.3.0 (CI-only).
No EBU material and no audio file or decoded PCM derivative on disk.
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

import mpmath as mp

FIXTURES = (
    ("impulse", (1,), "1", 7964),
    ("dc_pair", (1, 1), "1.273239544735", 10198),
    ("alternating_pair", (1, -1), "1.092126763999", 8802),
    ("separated_signed", (1, 0, -1), "1.041769775566", 8451),
    ("alternating_four", (1, -1, 1, -1), "1.250589356949", 9410),
)
CI419_SHA = "76746fc75ec48d2ae7460660c640916a69c48bda"
CI419_RUN = "38303348879"
CI419_EXE_SHA = "9b51a2fadafdb4094a3850a51c8495f4d4d7e46696a7918190b17c2b3682b9c6"


class Failure(Exception):
    pass


def ideal_abs(samples, t):
    # sin(pi*u)/(pi*u), including the exact cardinal limit at integer u.
    return abs(mp.fsum(x * mp.sincpi(t - n)
                       for n, x in enumerate(samples)))


def sample_and_refine(samples, oversample, precision):
    # Finite [-16,N-1+16] discovery is NOT an infinite sinc-tail enclosure.
    with mp.workdps(precision):
        q = oversample
        invq = mp.mpf(1) / q
        best = mp.mpf(-1)
        best_tick = 0
        for tick in range(-16 * q, (len(samples) - 1 + 16) * q + 1):
            value = ideal_abs(samples, mp.mpf(tick) * invq)
            if value > best:
                best, best_tick = value, tick
        # Bracketed local maximum refinement of the best grid cell.
        # No global interval-cover certification is claimed.
        left = mp.mpf(best_tick - 1) * invq
        right = mp.mpf(best_tick + 1) * invq
        ratio = (mp.sqrt(5) - 1) / 2
        x = right - ratio * (right - left)
        z = left + ratio * (right - left)
        fx, fz = ideal_abs(samples, x), ideal_abs(samples, z)
        for _ in range(100):
            if fx < fz:
                left, x, fx = x, z, fz
                z = left + ratio * (right - left)
                fz = ideal_abs(samples, z)
            else:
                right, z, fz = z, x, fx
                x = right - ratio * (right - left)
                fx = ideal_abs(samples, x)
        return max(best, fx, fz), x if fx >= fz else z


def check_build(exe, provenance, sha, run, pinned_sha):
    if not exe.is_file() or not provenance.is_file():
        raise Failure("Missing exact-build probe/provenance")
    stamp = json.loads(provenance.read_text(encoding="utf-8-sig"))
    digest = hashlib.sha256(exe.read_bytes()).hexdigest()
    if (stamp.get("source_commit_sha") != sha
            or str(stamp.get("workflow_run_id")) != run
            or stamp.get("probe_exe_sha256") != digest
            or stamp.get("production_gate_g") != "NOT_EXECUTED"
            or (pinned_sha is not None and digest != pinned_sha)):
        raise Failure("Wrong source/run/probe identity")
    return digest


def production_peak(exe, samples):
    # Private anonymous stdin pipe: all PCM source data synthesized in RAM.
    pcm = b"".join(struct.pack("<dd", float(x), 0.0)
                   for x in samples)
    p = subprocess.run([str(exe.resolve()), "48000", str(len(samples))],
                       input=pcm, capture_output=True, check=False, timeout=30)
    if p.returncode != 0:
        raise Failure(f"C++ probe returned {p.returncode}")
    try:
        return json.loads(p.stdout.decode("utf-8"))
    except (UnicodeError, ValueError) as exc:
        raise Failure("C++ returned non-JSON or malformed result") from exc


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--provenance", type=Path)
    parser.add_argument("--expected-source-sha")
    parser.add_argument("--expected-run-id")
    args = parser.parse_args()
    try:
        if mp.sincpi(0) != 1 or mp.sincpi(1) != 0:
            raise Failure("Independent sinc cardinal identity failed")
        if args.selftest:
            print(json.dumps({"status": "B2E_SINC_IDEAL_SELFTEST_PASS",
                              "production_gate_g": "NOT_COMPLETE"}))
            return 0
        if args.probe is None or args.provenance is None:
            raise Failure("Probe and provenance mandatory")
        if bool(args.expected_source_sha) != bool(args.expected_run_id):
            raise Failure("Must provide exact source and run together")
        fresh = args.expected_source_sha is not None
        sha = args.expected_source_sha or CI419_SHA
        run = args.expected_run_id or CI419_RUN
        digest = check_build(args.probe, args.provenance, sha, run,
                             None if fresh else CI419_EXE_SHA)
        results = []
        for name, samples, ideal_ref, itu_numerator in FIXTURES:
            p32, _ = sample_and_refine(samples, 32, 80)
            p64, _ = sample_and_refine(samples, 64, 80)
            p128, time128 = sample_and_refine(samples, 128, 80)
            # Same refined location at 120 digits is a precision stability
            # check, NOT proof that no larger peak exists outside the grid.
            with mp.workdps(120):
                high_precision_sample = ideal_abs(samples, mp.mpf(time128))
            if abs(high_precision_sample - p128) > mp.mpf("1e-25"):
                raise Failure(f"{name}: 80/120-digit discrepancy")
            convergence_db = abs(20 * mp.log10(p64 / p128))
            if convergence_db > mp.mpf("0.0001"):
                raise Failure(f"{name}: 64x/128x convergence screen failed")
            if abs(p128 - mp.mpf(ideal_ref)) > mp.mpf("0.000002"):
                raise Failure(f"{name}: independently recorded Work estimate differs")
            if name == "impulse" and (p32 != 1 or p64 != 1 or p128 != 1):
                raise Failure("Analytic unit impulse sinc peak must be 1")
            expected_itu = itu_numerator / 8192.0
            out = production_peak(args.probe, samples)
            if (out.get("left_linear_peak") != expected_itu
                    or out.get("global_linear_peak") != expected_itu
                    or out.get("right_linear_peak") != 0.0
                    or out.get("right_dbtp") is not None
                    or out.get("production_gate_g") != "NOT_EXECUTED"):
                raise Failure(f"{name}: actual C++ does not match exact rational ITU peak")
            observed_db = out.get("global_dbtp")
            if (not isinstance(observed_db, (float, int))
                    or not math.isfinite(observed_db)
                    or abs(observed_db - 20 * math.log10(expected_itu)) > 0.01):
                raise Failure(f"{name}: QTP against ITU discrete oracle failed")
            results.append({
                "name": name,
                "ideal_128x_refined_estimate": float(p128),
                "ideal_64x_128x_dbtp_delta": float(convergence_db),
                "production_itu4x_peak": expected_itu,
                "itu_minus_ideal_dbtp_diagnostic":
                    float(20 * mp.log10(expected_itu / p128)),
                "ideal_tail_interval_certificate": "NOT_EXECUTED",
                "comparison_scope": "IDEAL_NOT_RGS_0_01_DBTP_REFERENCE",
                "screen": "PASS"
            })
    except (Failure, AssertionError, ValueError, TypeError, OSError,
            subprocess.TimeoutExpired) as exc:
        print(json.dumps({"status": "FAIL_CLOSED", "reason": str(exc),
                          "production_gate_g": "NOT_COMPLETE"}), file=sys.stderr)
        return 1
    print(json.dumps({
        "status": "B2E_IDEAL_RECONSTRUCTION_SCREEN_PASS",
        "production_gate_g": "NOT_COMPLETE",
        "source_build_sha": sha,
        "workflow_run_id": run,
        "probe_exe_sha256": digest,
        "scope": "SYNTHETIC_80DIGIT_32X_64X_128X_SCREEN_NOT_CERTIFIED",
        "cases": len(results),
        "results": results
    }, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
