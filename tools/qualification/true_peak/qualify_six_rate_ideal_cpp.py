#!/usr/bin/env python3
"""M16 B2h: six-rate Hz-defined high-precision ideal sinc SCREEN vs real C++.

Independently generate real Hz tones at 80/120 decimal digits, round ONCE to
binary64 input, and assess those ACTUAL samples with two distinct oracles:
* high-precision >=64x sinc interpolation of one actual source channel,
  finite-window diagnostic only (not a globally certified ideal peak);
* independently transcribed exact-dyadic ITU FIR numerators to verify
  left/right/global C++ against the RGS 0.01-dBTP criterion.
Never apply the RGS 0.01-dBTP predicate to mathematical ideal peaks.
No official EBU assets, audio files, PCM files, or production code used.
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

# Both independent helpers are test-only: neither imports production taps,
# production FIR/reduction code, or the production output DTO.
from qualify_ideal_screen_cpp import ideal_abs, sample_and_refine, check_build
from qualify_six_rate_cpp import RATES, TAPS

CI422_SHA = "0684483c2912641356539e4bb1c7d8c8992e183f"
CI422_RUN = "38407061252"
CI422_EXE_SHA = "ab58e98813fe08ef76d6c5d9662bfaee021cdd12055dd6122ff8398eef76baa7"
FRAMES = 24
ITU_DIVISOR = 8192
LEFT_HZ = 12345
RIGHT_HZ = 6789


class Failure(ValueError):
    pass


def generate_pcm(hz: int, precision: int) -> tuple[tuple[float, ...], tuple[float, ...]]:
    """True-Hz stereo PCM. High-precision sine then ONE binary64 rounding."""
    with mp.workdps(precision):
        rate = mp.mpf(hz)
        phi_l = mp.mpf(1) / 7
        phi_r = mp.mpf(2) / 11
        left = tuple(float(mp.mpf(5)/8 * mp.sin(
            2 * mp.pi * LEFT_HZ * (mp.mpf(i) + phi_l) / rate))
                     for i in range(FRAMES))
        right = tuple(float(mp.mpf(3)/8 * mp.cos(
            2 * mp.pi * RIGHT_HZ * (mp.mpf(i) - phi_r) / rate))
                      for i in range(FRAMES))
    return left, right


def exact_dyadic_itu_peak(samples: tuple[float, ...]) -> mp.mpf:
    """Separate mpf FIR: exact dyadic input, signed original lag numerators.

    Unlike B2e's ideal sinc oracle, this independently evaluates the
    normative frozen discrete ITU filter, with 11 exact zero-EOS frames.
    It DOES NOT read coefficients or code from the C++ production runtime.
    """
    greatest = mp.mpf(0)
    n = len(samples)
    for frame in range(n + 11):
        for phase in TAPS:
            products = [
                mp.mpf(tap) * mp.mpf(samples[frame - lag])
                for lag, tap in enumerate(phase) if 0 <= frame - lag < n
            ]
            value = abs(mp.fsum(products) / ITU_DIVISOR)
            if value > greatest:
                greatest = value
    return greatest


def run_real_cpp(probe: Path, hz: int,
                 left: tuple[float, ...], right: tuple[float, ...]) -> dict:
    payload = b"".join(struct.pack("<dd", l, r)
                       for l, r in zip(left, right))
    completed = subprocess.run(
        [str(probe.resolve()), str(hz), str(len(left))],
        input=payload, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        check=False, timeout=120,
    )
    if completed.returncode != 0:
        raise Failure(f"{hz}: production C++ failed exit={completed.returncode}")
    try:
        out = json.loads(completed.stdout.decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as exc:
        raise Failure(f"{hz}: invalid C++ JSON") from exc
    return out


def compare_cpp(out: dict, hz: int,
                left_itu: mp.mpf, right_itu: mp.mpf) -> dict:
    if (out.get("sample_rate_hz"), out.get("frames"),
            out.get("channel_count"), out.get("production_gate_g")) != (
            hz, FRAMES, 2, "NOT_EXECUTED"):
        raise Failure(f"{hz}: incorrect C++ rate/frame/qualification metadata")
    expected = (left_itu, right_itu, max(left_itu, right_itu))
    numeric = ("left_linear_peak", "right_linear_peak", "global_linear_peak")
    decibel = ("left_dbtp", "right_dbtp", "global_dbtp")
    deviations = []
    for key, db_key, target in zip(numeric, decibel, expected):
        value = out.get(key)
        db = out.get(db_key)
        if (type(value) not in (int, float) or not math.isfinite(value)
                or value <= 0 or type(db) not in (int, float)
                or not math.isfinite(db)):
            raise Failure(f"{hz}: invalid {key}/{db_key}")
        with mp.workdps(80):
            rational_db = 20 * mp.log10(target)
            linear_error_db = abs(20 * mp.log10(mp.mpf(value) / target))
            db_error_db = abs(mp.mpf(db) - rational_db)
            if linear_error_db > mp.mpf("0.01") or db_error_db > mp.mpf("0.01"):
                raise Failure(f"{hz}: frozen RGS 0.01 dBTP vs ITU oracle FAILED")
            deviations.append(float(max(linear_error_db, db_error_db)))
    if out.get("is_exact_silence") is not False:
        raise Failure(f"{hz}: nonzero stereo tone reported as silence")
    return {
        "independent_discrete_itu_left_linear": float(left_itu),
        "independent_discrete_itu_right_linear": float(right_itu),
        "independent_discrete_itu_global_linear": float(expected[2]),
        "rgs_qtp_0_01_vs_discrete_itu": "PASS",
        "max_discrete_itu_dbtp_deviation": max(deviations),
    }


def internal_selftest() -> None:
    if len(RATES) != 6 or len(set(RATES)) != 6 or len(TAPS) != 4:
        raise Failure("Canonical six rate or independently transcribed FIR missing")
    if mp.sincpi(0) != 1 or mp.sincpi(1) != 0:
        raise Failure("Independent cardinal sinc identity broken")
    with mp.workdps(100):
        if exact_dyadic_itu_peak((1.0,)) != mp.mpf(7964) / 8192:
            raise Failure("Exact ITU unit impulse selftest failed")
        if exact_dyadic_itu_peak((1.0, 1.0)) != mp.mpf(10198) / 8192:
            raise Failure("Exact ITU adjacent-pair selftest failed")
        for hz in RATES:
            a, b = generate_pcm(hz, 80)
            aa, bb = generate_pcm(hz, 120)
            if (a, b) != (aa, bb) or len(a) != FRAMES or len(b) != FRAMES:
                raise Failure(f"{hz}: binary64 PCM high-precision stability failed")
            if not all(math.isfinite(x) for x in a + b):
                raise Failure(f"{hz}: invalid source PCM synthesis")
            if (max(map(abs, a)) <= 0 or max(map(abs, b)) <= 0):
                raise Failure(f"{hz}: unexpectedly silent generator")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--probe", type=Path)
    ap.add_argument("--provenance", type=Path)
    ap.add_argument("--expected-source-sha")
    ap.add_argument("--expected-run-id")
    args = ap.parse_args()
    try:
        internal_selftest()
        if args.selftest:
            print(json.dumps({
                "status": "B2H_HZ_GENERATOR_AND_EXACT_ITU_SELFTEST_PASS",
                "production_gate_g": "NOT_COMPLETE"}))
            return 0
        if args.probe is None or args.provenance is None:
            raise Failure("Exact-head probe/provenance mandatory")
        if bool(args.expected_source_sha) != bool(args.expected_run_id):
            raise Failure("Source SHA and workflow run must be supplied together")
        new_ci = args.expected_source_sha is not None
        head = args.expected_source_sha or CI422_SHA
        run_id = args.expected_run_id or CI422_RUN
        sha = check_build(args.probe, args.provenance, head, run_id,
                          None if new_ci else CI422_EXE_SHA)
        raw_provenance = json.loads(args.provenance.read_text(encoding="utf-8-sig"))
        if (raw_provenance.get("production_gate_g") != "NOT_EXECUTED"
                or raw_provenance.get("test_scope") !=
                   "SYNTHETIC_ONLY__OFFICIAL_EBU_WAVS_NOT_EXECUTED"):
            raise Failure("Misleading CI probe qualification/asset claim")

        rows = []
        hashes = set()
        for hz in RATES:
            left, right = generate_pcm(hz, 80)
            pcm_hash = hashlib.sha256(b"".join(
                struct.pack("<dd", l, r) for l, r in zip(left, right))).hexdigest()
            if pcm_hash in hashes:
                raise Failure("Rate-labelled duplicate PCM fixture detected")
            hashes.add(pcm_hash)
            with mp.workdps(100):
                expected_left = exact_dyadic_itu_peak(left)
                expected_right = exact_dyadic_itu_peak(right)
            observed = run_real_cpp(args.probe, hz, left, right)
            normative = compare_cpp(observed, hz, expected_left, expected_right)

            # The ideal oracle is intentionally on the LEFT source tone only;
            # both stereo channels are independently checked against ITU.
            ideal64, _ = sample_and_refine(left, 64, 80)
            ideal128, time128 = sample_and_refine(left, 128, 80)
            with mp.workdps(120):
                point120 = ideal_abs(left, mp.mpf(time128))
            with mp.workdps(80):
                convergence_db = abs(20 * mp.log10(ideal64 / ideal128))
                precision_delta = abs(point120 - ideal128)
                if convergence_db > mp.mpf("0.0001"):
                    raise Failure(f"{hz}: 64x/128x ideal SCREEN convergence failed")
                if precision_delta > mp.mpf("1e-25"):
                    raise Failure(f"{hz}: independent 80/120-digit check failed")
                diff_itu_ideal_db = 20 * mp.log10(expected_left / ideal128)
            rows.append({
                "rate_hz": hz,
                "frames": FRAMES,
                "hz_left": LEFT_HZ,
                "hz_right": RIGHT_HZ,
                "pcm64_interleaved_sha256": pcm_hash,
                "rate_specific_input_rounded_at_80_and_120_digits": "PASS",
                **normative,
                "ideal_left_refined_128x": float(ideal128),
                "ideal_left_64x_128x_dbtp_difference": float(convergence_db),
                "itu_minus_ideal_left_dbtp_diagnostic": float(diff_itu_ideal_db),
                "ideal_tail_interval_certificate": "NOT_EXECUTED",
                "qualification_status": "SUPPORTED_UNQUALIFIED",
                "screen": "PASS"
            })
        if len(rows) != 6 or len(hashes) != 6:
            raise Failure("Not all six physical-rate fixtures qualified as a screen")
    except (Failure, ValueError, OSError, TypeError, OverflowError,
            subprocess.TimeoutExpired, AssertionError) as exc:
        print(json.dumps({"status": "FAIL_CLOSED", "reason": str(exc),
                          "production_gate_g": "NOT_COMPLETE"}), file=sys.stderr)
        return 1
    print(json.dumps({
        "status": "B2H_SIX_RATE_HIGH_RES_IDEAL_SCREEN_AND_DISCRETE_CPP_PASS",
        "production_gate_g": "NOT_COMPLETE",
        "release_qualification": "NOT_AUTHORIZED",
        "scope": "SIX_REAL_HERTZ_RATES_80_DIGIT_64X_128X_FINITE_SINC_SCREEN",
        "full_global_continuous_ideal_certificate": "NOT_EXECUTED",
        "official_ebu_15_23_status": "NOT_EXECUTED__PRIVATE_48KHZ_EVIDENCE_IS_SEPARATE",
        "source_build_sha": head,
        "workflow_run_id": run_id,
        "probe_exe_sha256": sha,
        "rates": list(RATES),
        "cases": len(rows),
        "results": rows
    }, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
