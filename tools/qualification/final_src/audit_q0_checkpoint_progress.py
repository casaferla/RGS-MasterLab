#!/usr/bin/env python3
"""Read-only progress audit of REAL Phase-11 MPFR/Ziv Q0 checkpoints.

Reuses the exact production-Q0 checkpoint reader and four independently
frozen *nonrelease* CI baselines. Does NOT invoke an MPFR generator, run
an interval DTFT proof, edit a checkpoint, or grant M17 product admission.
No wall-time ETA is invented from kernel sizes or CI runner timings.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path

from build_candidate_manifest import canonical_pair_for_ratio
from build_resumable_real_candidates import _read_checkpoint
from import_audited_ci_q0_checkpoints import _pinned_candidates
from phase11_math_preflight import ContractError, Ratio, kernel_shape


def audit_progress(workdir: Path, *, checked_reader=_read_checkpoint,
                   pinned_reader=_pinned_candidates) -> dict:
    pairs = canonical_pair_for_ratio()
    if workdir.is_symlink() or (workdir.exists() and not workdir.is_dir()):
        raise ContractError("Q0 progress checkpoint root is not a real directory")
    pinned = {(x["L"], x["M"]): x for x in pinned_reader()}
    if set(pinned) != {(2, 1), (1, 2), (1, 4), (4, 1)}:
        raise ContractError("Q0 progress expected four original authentic CI pins")
    permitted = {f"ratio_L{L}_M{M}" for L, M in pairs}
    actual = ({x.name for x in workdir.iterdir()} if workdir.exists() else set())
    transient = sorted(x for x in actual if
                       x.startswith(".incomplete-ratio-")
                       or x.startswith(".ci-q0-"))
    unexpected = actual - permitted - set(transient)
    if unexpected:
        raise ContractError("Q0 progress contains unexpected checkpoint entries")
    ready = []
    missing = []
    for (L, M), (fin, fout) in pairs.items():
        path = workdir / f"ratio_L{L}_M{M}"
        shape = kernel_shape(Ratio.make(fin, fout))
        if not path.exists() and not path.is_symlink():
            missing.append({"L": L, "M": M, "fin": fin, "fout": fout,
                            "coefficient_count": shape.N,
                            "expected_bytes": shape.frozen_bytes})
            continue
        # A present directory is never counted without full binary64,
        # canonical rate, exact receipt, REAL backend and SHA verification.
        data, bits = checked_reader(workdir, fin, fout)
        digest = sha256(data).hexdigest()
        if len(data) != shape.frozen_bytes:
            raise ContractError("Q0 progress accepted impossible byte count")
        if (L, M) in pinned:
            pin = pinned[(L, M)]
            if (digest != pin["sha"] or bits != pin["bits"]
                or list((fin, fout)) != pin["pair"]):
                raise ContractError("Q0 original real CI coefficient pin mismatch")
        ready.append({"L": L, "M": M, "fin": fin, "fout": fout,
                      "coefficient_count": shape.N,
                      "bytes": len(data), "precision_bits": bits,
                      "sha256": digest,
                      "pinned_original_ci_q0": (L, M) in pinned})
    if len(ready) + len(missing) != 14:
        raise ContractError("Q0 checkpoint cardinality violated")
    return {
        "schema": "rgsml.phase11.real-q0-checkpoint-progress/1",
        "status": ("ALL_14_REAL_CHECKPOINTS_VALIDATED_NOT_RELEASE"
                   if not missing and not transient else
                   "PARTIAL_OR_INCOMPLETE_REAL_CHECKPOINTS_NOT_RELEASE"),
        "checkpoint_root_present": workdir.is_dir(),
        "validated_real_checkpoint_count": len(ready),
        "remaining_missing_ratio_count": len(missing),
        "validated_pinned_original_ci_count":
            sum(row["pinned_original_ci_q0"] for row in ready),
        "total_expected_distinct_ratios": 14,
        "outstanding_incomplete_temporary_entries": len(transient),
        "expected_total_rgsmsr1_bytes":
            sum(kernel_shape(Ratio.make(fin, fout)).frozen_bytes
                for fin, fout in pairs.values()),
        "verified_rgsmsr1_bytes": sum(row["bytes"] for row in ready),
        "time_to_complete": None,
        "continuum_dtft_proven": False,
        "q1_q6_passed": False,
        "release_qualified": False,
        "product_admission_authorized": False,
        "validated": ready,
        "missing": missing,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkpoint-workdir", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(audit_progress(args.checkpoint_workdir),
                     sort_keys=True, indent=2))


if __name__ == "__main__":
    main()
