#!/usr/bin/env python3
"""NONRELEASE genuine Phase11 36-path Q2 SUBSET: absolute frame invariance.

Must consume real all-14 integrity-audited RGSMSR1 assets, NOT synthetic FIRs.
Provides no core-cache/checkpoint, platform, runtime, release or FQ authority.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
from struct import pack, unpack_from

from phase11_math_preflight import Ratio, kernel_shape, mapped_interval, ContractError
from strict_scalar_polyphase_reference import strict_scalar_crop
from verify_candidate_family import audit_candidate_family

RATES = (44100, 48000, 88200, 96000, 176400, 192000)
SOURCE_Q1 = (0.25, -0.5, 0.125, 0.0, 0.5, -0.25, 0.375, -0.125)
TINY = float.fromhex("0x0.0000000000001p-1022")
SOURCE_EDGE = (0.0, -0.0, TINY, -TINY) + tuple(
    (i % 7 - 3) / 8 for i in range(4, 41))
PART_Q1 = (0, 1, 3, 5, 8)
PART_EDGE = (0, 1, 4, 11, 23, 29, 39, 41)
EXPECTED_MANIFEST_SHA = "0072fb8cf2fa5930c8200c67cac21d710724b0eb0ee1f2d73e9dbf9798938d37"


def qualify(assets: Path, vectors: Path) -> dict:
    integrity = audit_candidate_family(assets, require_complete=True)
    manifest = json.loads((assets / "CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json").read_text())
    refs = {(r["L"], r["M"]): r for r in manifest["assets"]}
    expected_raw = (vectors / "Q1_EXPECTED_MANIFEST_NONRELEASE.json").read_bytes()
    if sha256(expected_raw).hexdigest() != EXPECTED_MANIFEST_SHA:
        raise ContractError("Q2 rehearsal requires exact independent 36-Q1 manifest pin")
    oracle = json.loads(expected_raw)
    rows = {(r["fin"], r["fout"]): r for r in oracle.get("outputs", [])}
    if (oracle.get("path_count") != 36 or len(rows) != 36
        or oracle.get("product_admission_authorized") is not False):
        raise ContractError("not 36 independent nonrelease Q1 references")
    results = []
    for fin in RATES:
        for fout in RATES:
            ratio = Ratio.make(fin, fout)
            if fin == fout:
                taps, asset_digest = (), None
            else:
                entry = refs[(ratio.L, ratio.M)]
                data = (assets / entry["filename"]).read_bytes()
                asset_digest = sha256(data).hexdigest()
                if asset_digest != entry["candidate_sha256"]:
                    raise ContractError("Q2 real asset SHA differs from Q0")
                taps = unpack_from("<%dd" % kernel_shape(ratio).N, data, 52)
            ref = rows[(fin, fout)]
            baseline = strict_scalar_crop(SOURCE_Q1, ratio, taps)
            expected = (vectors / ref["expected_file"]).read_bytes()
            if (sha256(expected).hexdigest() != ref["interleaved_f64le_sha256"]
                or len(expected) != 16 * len(baseline)
                or any(pack("<d", x) != expected[16*i:16*i+8]
                       for i, x in enumerate(baseline))):
                raise ContractError("Q2 replay disagrees with independent Q1 oracle")
            for full, boundaries in (
                (SOURCE_Q1, PART_Q1), (SOURCE_EDGE, PART_EDGE)):
                whole = strict_scalar_crop(full, ratio, taps)
                segmented = []
                spans = []
                for lo, hi in zip(boundaries[:-1], boundaries[1:]):
                    segmented.extend(strict_scalar_crop(
                        full, ratio, taps, (lo, hi)))
                    spans.append(mapped_interval(lo, hi, ratio))
                if (b"".join(pack("<d", x) for x in whole)
                    != b"".join(pack("<d", x) for x in segmented)
                    or spans[0][0] != 0 or spans[-1][1] != len(whole)
                    or any(spans[i][1] != spans[i+1][0]
                           for i in range(len(spans)-1))):
                    raise ContractError("Q2 exact absolute-frame or chunk invariance failed")
                if fin == fout and (
                    b"".join(pack("<d", x) for x in whole)
                    != b"".join(pack("<d", x) for x in full)):
                    raise ContractError("Q2 identity lost signed-zero/subnormal bits")
            results.append({"fin": fin, "fout": fout,
                            "L": ratio.L, "M": ratio.M,
                            "real_candidate_sha256": asset_digest,
                            "status": "OFFLINE_EXACT_FRAME_CROP_AND_PARTITION_PASS"})
    if not integrity["complete"] or len(results) != 36:
        raise ContractError("Q2 expected complete 14 kernel and 36 pair inventory")
    return {
        "schema": "rgsml.phase11.offlineQ2-real36-absolute-crop/1",
        "status": "OFFLINE_Q2_SUBSET_36_PAIRS_PASS_NONRELEASE",
        "checked_pairs": 36, "checked_signal_cases": 72,
        "identity_cases": 6, "nonidentity_cases": 30,
        "signed_zero_subnormal_identity_bits": True,
        "absolute_frame_crop_bitwise_invariance": True,
        "core_cache_checkpoint_resume_verified": False,
        "full_q2_conformance_verified": False,
        "continuum_dtft_proven": False, "q1_q6_passed": False,
        "product_admission_authorized": False, "release_qualified": False,
        "paths": results,
    }


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--candidate-dir", type=Path, required=True)
    p.add_argument("--q1-dir", type=Path, required=True)
    p.add_argument("--report", type=Path, required=True)
    args = p.parse_args()
    if args.report.exists() or args.report.resolve().is_relative_to(
        args.candidate_dir.resolve()):
        raise ContractError("Q2 report must be a NEW path outside immutable assets")
    result = qualify(args.candidate_dir, args.q1_dir)
    args.report.write_text(json.dumps(result, sort_keys=True, indent=2) + "\n")
    print(result["status"], result["checked_pairs"], result["checked_signal_cases"])


if __name__ == "__main__":
    main()
