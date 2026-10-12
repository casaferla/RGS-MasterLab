#!/usr/bin/env python3
"""B2b private EBU v5 15-23 production-C++ comparison. © EBU.

Inputs must be authentic and used for internal technical R&D only.
All nine official WAVs are independently checked with the B2a preflight
before the C++ executable receives any decoded PCM64 over a private pipe.
This script NEVER promotes official Gate G or any product status.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import struct
import subprocess
import sys

import verify_ebu_3341_v5 as b2a


class QualificationError(b2a.GatePreflightError):
    pass


def run_case(exe: Path, case: dict, raw: bytes, manifest: dict) -> dict:
    left, right, pcm_digest = b2a.pcm24_stereo(raw, case["frames"])
    if pcm_digest != case["pcm64_sha256"]:
        raise QualificationError(f"Case {case['case']}: PCM digest changed")

    # PCM in anonymous OS pipe only; do not export redistributed derivatives.
    binary = bytearray()
    for l, r in zip(left, right):
        binary.extend(struct.pack("<dd", l / (1 << 23), r / (1 << 23)))
    try:
        run = subprocess.run(
            [str(exe.resolve()), "48000", str(case["frames"])],
            input=bytes(binary), stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, check=False, timeout=120,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise QualificationError(f"Case {case['case']}: C++ probe unavailable") from exc
    if run.returncode != 0:
        raise QualificationError(f"Case {case['case']}: C++ probe exit={run.returncode}")
    try:
        measured = json.loads(run.stdout.decode("utf-8"))
    except (ValueError, UnicodeDecodeError) as exc:
        raise QualificationError(f"Case {case['case']}: C++ JSON invalid") from exc
    if measured.get("production_gate_g") != "NOT_EXECUTED":
        raise QualificationError("C++ probe invented Gate G status")
    if (measured.get("sample_rate_hz"), measured.get("frames"),
            measured.get("channel_count"), measured.get("is_exact_silence")) != (
            48000, case["frames"], 2, False):
        raise QualificationError(f"Case {case['case']}: C++ metadata mismatch")

    div = manifest["exact_peak_denominator"]
    expected = {
        "left_linear_peak": case["left_numerator"] / div,
        "right_linear_peak": case["right_numerator"] / div,
        "global_linear_peak": max(case["left_numerator"],
                                  case["right_numerator"]) / div,
    }
    for name, correct in expected.items():
        actual = measured.get(name)
        # Exact equality is justified by the Appendix B dyadic PCM24 proof.
        if type(actual) not in (int, float) or not math.isfinite(actual) or actual != correct:
            raise QualificationError(f"Case {case['case']}: C++ {name} != integer oracle")

    for key, peak in (("left_dbtp", "left_linear_peak"),
                      ("right_dbtp", "right_linear_peak"),
                      ("global_dbtp", "global_linear_peak")):
        actual = measured.get(key)
        target = 20.0 * math.log10(expected[peak])
        if type(actual) not in (int, float) or not math.isfinite(actual) or abs(actual - target) > 0.01:
            raise QualificationError(f"Case {case['case']}: RGS QTP failed ({key})")

    nominal = case["expected_ebu_dbtp"]
    lo = nominal + manifest["official_tolerance_dbtp"]["low_offset"]
    hi = nominal + manifest["official_tolerance_dbtp"]["high_offset"]
    if not lo <= measured["global_dbtp"] <= hi:
        raise QualificationError(f"Case {case['case']}: EBU interval failed")

    return {"case": case["case"], "frames": case["frames"],
            "cpp_left_peak": measured["left_linear_peak"],
            "cpp_right_peak": measured["right_linear_peak"],
            "cpp_dbtp": measured["global_dbtp"],
            "pcm24_exact_oracle": "PASS", "rgs_qtp_0_01": "PASS",
            "ebu_asymmetric_tolerance": "PASS"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--archive", type=Path)
    source.add_argument("--directory", type=Path)
    args = parser.parse_args()
    try:
        manifest = json.loads(Path(__file__).with_name(
            "ebu_3341_cases_15_23_manifest.json").read_text(encoding="utf-8"))
        if (manifest.get("status") !=
                "PREPRODUCTION_PROVENANCE_ONLY__NOT_PRODUCTION_GATE_G"
                or [x["case"] for x in manifest["cases"]] != list(range(15, 24))):
            raise QualificationError("Untrusted manifest identity/scope")
        if not args.probe.is_file():
            raise QualificationError("C++ probe executable absent")
        originals = (b2a.select_archive(args.archive, manifest) if args.archive
                     else b2a.select_directory(args.directory, manifest))
        # Fail atomically if ANY authentic preflight case is missing/incorrect.
        for c in manifest["cases"]:
            b2a.verify(c, originals[c["case"]], manifest)
        results = [run_case(args.probe, c, originals[c["case"]], manifest)
                   for c in manifest["cases"]]
    except (QualificationError, b2a.GatePreflightError, OSError, ValueError,
            KeyError, OverflowError, TypeError) as exc:
        print(json.dumps({"status": "FAIL_CLOSED", "reason": str(exc),
                          "production_gate_g": "NOT_PASSED"}, indent=2),
              file=sys.stderr)
        return 1
    print(json.dumps({"status": "B2B_OFFICIAL_48KHZ_CPP_COMPARISON_PASS",
                      "production_gate_g": "NOT_COMPLETE",
                      "copyright": "© EBU", "results": results}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
