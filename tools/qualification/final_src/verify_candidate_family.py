#!/usr/bin/env python3
"""Fail-closed OFFLINE integrity audit of a Phase-11 candidate folder.

Never upgrades candidate bytes/hashes to an independently approved release
manifest. Does NOT prove Kaiser coefficients, spectral response, Q1–Q6,
MPFR/DTFT or product SRC admission.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path

from build_candidate_manifest import canonical_pair_for_ratio
from phase11_math_preflight import (
    ContractError, Ratio, audit_rgsmsr1_structure, kernel_shape,
)


def audit_candidate_family(folder: Path, *, require_complete: bool = True) -> dict:
    document = json.loads(
        (folder / "CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json").read_text(
            encoding="utf-8"))
    if not isinstance(document, dict):
        raise ContractError("candidate manifest must be an object")
    if (document.get("schema") != "rgsml.phase11.RGSMSR1-candidate-manifest/1"
        or document.get("status") != "CANDIDATE_NOT_RELEASE_QUALIFIED"):
        raise ContractError("candidate manifest has wrong schema/status")
    for key in ("independent_generator_verified", "mpfr_dtft_oracle_passed",
                "q1_q6_passed", "product_admission_authorized"):
        if document.get(key) is not False:
            raise ContractError("candidate must not claim certification: "+key)
    expected = canonical_pair_for_ratio()
    rows = document.get("assets")
    if not isinstance(rows, list) or not rows or len(rows) > 14:
        raise ContractError("candidate family size invalid")
    accepted = set()
    observed_names = {"CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json"}
    bytes_total = 0
    for row in rows:
        if not isinstance(row, dict):
            raise ContractError("invalid candidate asset record")
        L, M = row.get("L"), row.get("M")
        if type(L) is not int or type(M) is not int or (L, M) in accepted:
            raise ContractError("candidate ratio invalid/duplicated")
        canonical = expected.get((L, M))
        if canonical is None or (row.get("fin"), row.get("fout")) != canonical:
            raise ContractError("candidate ratio has noncanonical rate pair")
        ratio = Ratio.make(*canonical)
        shape = kernel_shape(ratio)
        file_name = f"RGSMSR1_L{L}_M{M}.bin"
        if (row.get("filename") != file_name
            or row.get("N") != shape.N or row.get("T") != shape.T
            or row.get("bytes") != shape.frozen_bytes
            or row.get("status") !=
                "SAME_GENERATOR_DOUBLE_REPRODUCED_NOT_RELEASE_QUALIFIED"):
            raise ContractError("candidate geometry, filename or status invalid")
        digest = row.get("candidate_sha256")
        if (not isinstance(digest, str) or len(digest) != 64
            or any(char not in "0123456789abcdef" for char in digest)):
            raise ContractError("candidate SHA not canonical lower-hex")
        payload = (folder / file_name).read_bytes()
        if (audit_rgsmsr1_structure(payload, shape) != digest
            or sha256(payload).hexdigest() != digest):
            raise ContractError("candidate asset binary checksum mismatch")
        bytes_total += len(payload)
        observed_names.add(file_name)
        accepted.add((L, M))
    real_names = {f.name for f in folder.iterdir()}
    if real_names != observed_names:
        raise ContractError("candidate folder missing or unexpected files")
    complete = accepted == set(expected)
    if (document.get("complete_ratio_family") is not complete
        or document.get("observed_ratio_count") != len(rows)
        or document.get("expected_ratio_count") != 14
        or document.get("total_asset_bytes") != bytes_total):
        raise ContractError("candidate family completeness or bytes metadata mismatch")
    if require_complete and not complete:
        raise ContractError("all 14 ratios required for complete candidate audit")
    for key, per_entry_key in (
        ("independent_mpmath_screen_complete", "independent_mpmath_screen_passed"),
        ("exact_dyadic_screen_complete", "exact_dyadic_screen_passed"),
        ("sampled_fft_screen_complete", "sampled_fft_screen_passed"),
        ("exact_two_point_screen_complete", "exact_two_point_screen_passed"),
    ):
        flags = [row.get(per_entry_key) is True for row in rows]
        # Never trust a manifest that claims a whole-family screen if any
        # constituent ratio is absent/unscreened.
        if document.get(key) is not (complete and all(flags)):
            raise ContractError("candidate screen completeness misrepresented: "+key)
    return dict(status="CANDIDATE_INTEGRITY_ONLY_NOT_RELEASE_QUALIFIED",
                ratios=len(rows), complete=complete, bytes=bytes_total,
                product_admission_authorized=False)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("folder", type=Path)
    p.add_argument("--allow-subset-fixture", action="store_true",
                   help="test-only: permit a subset, never a complete 14-asset claim")
    a = p.parse_args()
    print(json.dumps(
        audit_candidate_family(
            a.folder, require_complete=not a.allow_subset_fixture),
        sort_keys=True))
