#!/usr/bin/env python3
"""M16-B2c: six-rate, hertz-defined synthetic evidence against genuine C++.

Independent exact-integer 4-phase reference; never imports production FIR.
All PCM is generated deterministically in RAM and piped locally to the
existing B2b production-linked C++ probe. Not an EBU test or Gate G pass.
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

# Source is the independently transcribed official mathematical numerators.
# This table is NOT loaded from the production source or compiled binary.
TAPS = (
    (14, 90, -161, 272, -487, 1125, 7964, -838, 390, -218, 122, -68),
    (-239, 240, -424, 730, -1364, 3810, 6388, -1641, 832, -477, 271, -155),
    (-155, 271, -477, 832, -1641, 6388, 3810, -1364, 730, -424, 240, -239),
    (-68, 122, -218, 390, -838, 7964, 1125, -487, 272, -161, 90, 14),
)
RATES = (44100, 48000, 88200, 96000, 176400, 192000)
DENOM = 8192 * (1 << 23)
CI417_SOURCE_SHA = "4152f1ba6bfc2c8037a3397631cd4f19f5e471c2"
CI417_RUN_ID = "38289765698"
CI417_EXE_SHA = "1ac9db1073790d97e8d6461dcd1c40528169aefd4baff7ec38cc39ea1327bed2"
AMPLITUDE_UNIT = 1 << 23


class TestFailure(ValueError):
    """Fail-closed qualification evidence exception."""


def exact_integer_peak(signal: list[int]) -> float:
    """Full 11-frame zero-EOS, signed FIR, absolute reduction; exact integers."""
    if not signal:
        raise TestFailure("Empty signal is forbidden")
    n = len(signal)
    greatest = 0
    for i in range(n + 11):
        for taps in TAPS:
            acc = 0
            for lag, tap in enumerate(taps):
                k = i - lag
                if 0 <= k < n:
                    acc += tap * signal[k]
            greatest = max(greatest, abs(acc))
    return greatest / DENOM


def create_cases(rate: int):
    """Rate-specific PCM with TRUE hertz-defined periods, not rate relabels."""
    n = rate // 20  # 50 ms in real-time at this rate.
    half = AMPLITUDE_UNIT // 2
    quarter = AMPLITUDE_UNIT // 4
    # Exact-integer sampled 1000-Hz bipolar square wave; no libm dependency.
    left = [half if ((2 * i * 1000) // rate) % 2 == 0 else -quarter
            for i in range(n)]
    right = [-quarter if ((2 * i * 1000 + rate // 3) // rate) % 2 == 0
             else half for i in range(n)]
    square = (left, right)
    # Uneven 997-Hz one-frame impulses. These sample positions genuinely
    # differ with the real rate (neither a resampled nor relabeled 48k vector).
    left = [-half if i == 0 or (i * 997) // rate != ((i - 1) * 997) // rate
            else 0 for i in range(n)]
    right = [quarter if i == 0
             or (i * 997 + rate // 4) // rate !=
             ((i - 1) * 997 + rate // 4) // rate else 0 for i in range(n)]
    pulses = (left, right)
    first = [0] * 13
    second = [0] * 13
    first[0] = AMPLITUDE_UNIT
    first[-1] = -2 * AMPLITUDE_UNIT
    second[0] = -AMPLITUDE_UNIT
    second[-1] = 3 * AMPLITUDE_UNIT
    zero = [0] * 13
    return (
        ("hz1000_square_stereo", square),
        ("hz997_pulse_train_stereo", pulses),
        ("hz1000_negated_metamorphic", ([-v for v in square[0]],
                                          [-v for v in square[1]])),
        ("hz997_swapped_metamorphic", (pulses[1], pulses[0])),
        ("boundary_signed_above_unity", (first, second)),
        ("exact_silence", (zero, zero)),
    )


def check_provenance(probe: Path, provenance: Path) -> str:
    if not probe.is_file() or not provenance.is_file():
        raise TestFailure("Probe or CI417 provenance JSON missing")
    obj = json.loads(provenance.read_text(encoding="utf-8-sig"))
    observed = hashlib.sha256(probe.read_bytes()).hexdigest()
    if (
        obj.get("source_commit_sha") != CI417_SOURCE_SHA
        or str(obj.get("workflow_run_id")) != CI417_RUN_ID
        or obj.get("production_gate_g") != "NOT_EXECUTED"
        or obj.get("test_scope") != "SYNTHETIC_ONLY__OFFICIAL_EBU_WAVS_NOT_EXECUTED"
        or observed != CI417_EXE_SHA
        or obj.get("probe_exe_sha256") != CI417_EXE_SHA
    ):
        raise TestFailure("Exact source/run/executable provenance mismatch")
    return observed


def run_cpp(probe: Path, rate: int, left: list[int], right: list[int]) -> dict:
    if len(left) != len(right):
        raise TestFailure("Channel length mismatch")
    # z / 2**23 is an EXACT binary64 dyadic, including finite >1 samples.
    # Anonymous pipe only, no source PCM, decoded audio, or derivative on disk.
    data = bytearray(len(left) * 16)
    for i, (l, r) in enumerate(zip(left, right)):
        struct.pack_into("<dd", data, i * 16, l / AMPLITUDE_UNIT,
                         r / AMPLITUDE_UNIT)
    proc = subprocess.run(
        [str(probe.resolve()), str(rate), str(len(left))],
        input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        check=False, timeout=120,
    )
    if proc.returncode != 0:
        raise TestFailure(f"C++ probe returned {proc.returncode} at {rate} Hz")
    try:
        return json.loads(proc.stdout.decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as exc:
        raise TestFailure("C++ probe did not return valid JSON") from exc


def compare(rate: int, kind: str, left: list[int], right: list[int],
            evidence: dict) -> dict:
    if (evidence.get("sample_rate_hz"), evidence.get("frames"),
            evidence.get("channel_count"), evidence.get("production_gate_g")) != (
            rate, len(left), 2, "NOT_EXECUTED"):
        raise TestFailure(f"{rate}/{kind}: misleading C++ metadata")
    p_left = exact_integer_peak(left)
    p_right = exact_integer_peak(right)
    p_global = max(p_left, p_right)
    for key, expected in (
        ("left_linear_peak", p_left),
        ("right_linear_peak", p_right),
        ("global_linear_peak", p_global),
    ):
        v = evidence.get(key)
        # Exact equality is justified for the bounded integer/dyadic fixtures.
        if type(v) not in (float, int) or not math.isfinite(v) or v != expected:
            raise TestFailure(f"{rate}/{kind}: {key} != independent integer oracle")
    if evidence.get("is_exact_silence") is not (p_global == 0.0):
        raise TestFailure(f"{rate}/{kind}: wrong silence state")
    for key, expected in (
        ("left_dbtp", p_left),
        ("right_dbtp", p_right),
        ("global_dbtp", p_global),
    ):
        observed = evidence.get(key)
        if expected == 0.0:
            if observed is not None:
                raise TestFailure(f"{rate}/{kind}: silent dB should be null")
        else:
            target = 20.0 * math.log10(expected)
            if type(observed) not in (float, int) or not math.isfinite(observed):
                raise TestFailure(f"{rate}/{kind}: invalid {key}")
            if abs(observed - target) > 0.01:
                raise TestFailure(f"{rate}/{kind}: RGS 0.01 dBTP bound failed")
    return {
        "rate_hz": rate, "family": kind, "frames": len(left),
        "left_peak": p_left, "right_peak": p_right,
        "global_dbtp": evidence.get("global_dbtp"),
        "exact_integer_oracle": "PASS", "qtp_0_01": "PASS",
        "metadata_silence": "PASS",
    }


def validate_metamorphic(results_by_kind: dict[str, dict], rate: int):
    pairs = (
        ("hz1000_square_stereo", "hz1000_negated_metamorphic", False),
        ("hz997_pulse_train_stereo", "hz997_swapped_metamorphic", True),
    )
    for parent, variant, swapped in pairs:
        a, b = results_by_kind[parent], results_by_kind[variant]
        if a["global_linear_peak"] != b["global_linear_peak"]:
            raise TestFailure(f"{rate}: metamorphic global mismatch")
        for ch in ("left", "right"):
            partner = "right" if ch == "left" and swapped else (
                "left" if ch == "right" and swapped else ch)
            if a[f"{ch}_linear_peak"] != b[f"{partner}_linear_peak"]:
                raise TestFailure(f"{rate}: metamorphic channel mismatch")


def internal_selftest():
    unit = AMPLITUDE_UNIT
    assert exact_integer_peak([0]) == 0.0
    assert exact_integer_peak([unit]) == 7964.0 / 8192.0
    assert exact_integer_peak([-unit]) == 7964.0 / 8192.0
    assert exact_integer_peak([unit, unit]) == 10198.0 / 8192.0
    assert exact_integer_peak([0, 0, 2 * unit]) == 2 * 7964.0 / 8192.0
    for hz in RATES:
        cases = create_cases(hz)
        assert len(cases) == 6
        assert len(cases[0][1][0]) == hz // 20
        assert len(cases[1][1][0]) == hz // 20
    return True


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--probe", type=Path, help="CI417 exact-head Windows probe EXE")
    ap.add_argument("--provenance", type=Path, help="CI417 build provenance JSON")
    args = ap.parse_args()
    try:
        internal_selftest()
        if args.selftest:
            print(json.dumps({"status": "B2C_PYTHON_INTEGER_SELFTEST_PASS",
                              "production_gate_g": "NOT_COMPLETE"}))
            return 0
        if args.probe is None or args.provenance is None:
            raise TestFailure("Must supply both --probe and --provenance")
        sha = check_provenance(args.probe, args.provenance)
        synthetic = []
        for rate in RATES:
            raw_by_family = {}
            for family, (l, r) in create_cases(rate):
                out = run_cpp(args.probe, rate, l, r)
                synthetic.append(compare(rate, family, l, r, out))
                raw_by_family[family] = out
            validate_metamorphic(raw_by_family, rate)
    except (TestFailure, OSError, ValueError, OverflowError,
            KeyError, subprocess.TimeoutExpired, AssertionError) as exc:
        print(json.dumps({"status": "FAIL_CLOSED",
                          "reason": str(exc),
                          "production_gate_g": "NOT_COMPLETE"}, indent=2),
              file=sys.stderr)
        return 1
    print(json.dumps({
        "status": "B2C_SYNTHETIC_SIX_RATE_CPP_COMPARISON_PASS",
        "production_gate_g": "NOT_COMPLETE",
        "source_build_sha": CI417_SOURCE_SHA,
        "probe_exe_sha256": sha,
        "scope": "RGS_SYNTHETIC_NOT_EBU",
        "sample_rates_hz": list(RATES),
        "cases": len(synthetic),
        "metamorphic_pairs": 12,
        "results": synthetic,
    }, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
