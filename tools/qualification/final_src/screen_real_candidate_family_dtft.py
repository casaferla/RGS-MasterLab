#!/usr/bin/env python3
"""Offline all-14 REAL candidate DTFT continuum screening (NON-RELEASE).

This CLI consumes already generated RGSMSR1 assets in a complete immutable
candidate folder. It verifies all bytes, SHA-256 and 14-ratio metadata BEFORE
computing ANY interval proof. Actual MPFR boxes use exact binary64 taps and
never trust a generated manifest as independently frozen release provenance.
Finite box budget exhaustion is UNRESOLVED, never PASS. No artifact is issued
inside the audited folder, no runtime component imports this offline module,
and this is not Q1-Q6/product admission or release certification.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Callable

from mpfr_dtft_interval_preflight import screen_candidate_asset
from phase11_math_preflight import ContractError
from verify_candidate_family import audit_candidate_family


def attempt_candidate_family(
    folder: Path, *, max_boxes: int = 128,
    require_complete: bool = True,
    screener: Callable[..., dict] = screen_candidate_asset,
) -> dict:
    if type(max_boxes) is not int or not 1 <= max_boxes <= 1_000_000:
        raise ContractError("MPFR DTFT bounded box budget required")
    integrity = audit_candidate_family(folder, require_complete=require_complete)
    document = json.loads(
        (folder / "CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json").read_text(
            encoding="utf-8"))
    rows = sorted(document["assets"], key=lambda x: (x["L"], x["M"]))
    results = []
    for entry in rows:
        # The independent file/folder audit above guarantees exact bytes and
        # manifest SHA, but the manifest itself is NOT a trusted release lock.
        outcome = screener(
            (folder / entry["filename"]).read_bytes(),
            entry["fin"], entry["fout"], entry["candidate_sha256"],
            max_boxes=max_boxes)
        if (not isinstance(outcome, dict)
            or outcome.get("candidate_sha256") != entry["candidate_sha256"]
            or outcome.get("fin") != entry["fin"]
            or outcome.get("fout") != entry["fout"]
            or outcome.get("status") !=
                "MPFR_DTFT_INTERVAL_ATTEMPT_NOT_RELEASE_QUALIFIED"
            or outcome.get("release_qualified") is not False
            or outcome.get("product_admission_authorized") is not False):
            raise ContractError("DTFT per-ratio response/provenance malformed")
        for kind in ("passband", "stopband"):
            band = outcome.get(kind)
            if (not isinstance(band, dict)
                or band.get("status") not in (
                    "UNRESOLVED", "CONTINUUM_BOUND_PROVEN_NONRELEASE")
                or band.get("release_qualified") is not False
                or type(band.get("visited_boxes")) is not int
                or not 1 <= band["visited_boxes"] <= max_boxes
                or type(band.get("remaining_boxes")) is not int
                or band["remaining_boxes"] < 0):
                raise ContractError("DTFT proof budget/status malformed")
        closed = all(outcome[kind]["status"] ==
                     "CONTINUUM_BOUND_PROVEN_NONRELEASE"
                     for kind in ("passband", "stopband"))
        if outcome.get("continuous_bounds_closed") is not closed:
            raise ContractError("DTFT claimed continuum proof without both bands")
        results.append(outcome)
    all_closed = bool(integrity["complete"]) and all(
        row["continuous_bounds_closed"] for row in results)
    return {
        "status": ("CANDIDATE_DTFT_CONTINUUM_BOUNDS_CLOSED_NOT_RELEASE"
                   if all_closed else
                   "CANDIDATE_DTFT_CONTINUUM_SCREEN_UNRESOLVED"),
        "ratios_screened": len(results),
        "complete_14_ratio_family": bool(integrity["complete"]),
        "continuum_bounds_closed": all_closed,
        "kernel_release_provenance_independently_frozen": False,
        "q1_q6_passed": False,
        "product_admission_authorized": False,
        "release_qualified": False,
        "assets": results,
    }


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("candidate_folder", type=Path)
    p.add_argument("--max-boxes", type=int, default=128,
                   help="per-band, per-ratio rigorous box budget; UNRESOLVED on exhaustion")
    p.add_argument("--report", type=Path, required=True,
                   help="new JSON report path OUTSIDE candidate folder (never a release manifest)")
    a = p.parse_args()
    if a.report.exists() or a.report.parent == a.candidate_folder:
        raise ContractError("report must be a new path outside the candidate folder")
    outcome = attempt_candidate_family(a.candidate_folder, max_boxes=a.max_boxes)
    a.report.parent.mkdir(parents=True, exist_ok=True)
    # No partial report on interrupted proof. Avoid overwriting preexisting
    # reports; qualified release hashes/certificates are never emitted.
    if a.report.exists():
        raise ContractError("refusing to overwrite an existing report")
    a.report.write_text(json.dumps(outcome, sort_keys=True, indent=2)+"\n",
                        encoding="utf-8")
    if not outcome["continuum_bounds_closed"]:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
