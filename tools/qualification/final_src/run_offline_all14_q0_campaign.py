#!/usr/bin/env python3
"""ONE resumable, real all-14 Phase-11 MPFR/Ziv Q0 offline campaign.

Consumes a previously SHA+provenance-audited four-candidate ZIP, generates
ONLY the remaining ten ratios (bounded independent MPFR processes), validates
all 14 exact binary64 kernels with independently written numerical negative
screens and publishes the complete nonrelease candidate family atomically.

No release KernelManifest, Q1-Q6, continuous DTFT, actual production admission
or Windows FQ authority is created by this offline command. If any phase
fails, already verified checkpoints remain reusable and the final family
is not published. Do not run it inside the ordinary per-PR Focused CI.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Callable

from build_resumable_real_candidates import stage_all, finalize
from import_audited_ci_q0_checkpoints import import_pinned_seed_bundle
from phase11_math_preflight import ContractError
from verify_candidate_family import audit_candidate_family


def run_campaign(
    seed_zip: Path, workdir: Path, candidate_folder: Path,
    *, workers: int = 2, max_precision: int = 1024,
    seed_importer: Callable = import_pinned_seed_bundle,
    generator: Callable = stage_all,
    publisher: Callable = finalize,
    verifier: Callable = audit_candidate_family,
) -> dict:
    if type(workers) is not int or not 1 <= workers <= 4:
        raise ContractError("real all-14 offline worker count must be 1..4")
    if type(max_precision) is not int or not 256 <= max_precision <= 4096:
        raise ContractError("real all-14 MPFR precision must be 256..4096")
    if candidate_folder.exists():
        raise ContractError("refuse overwrite of an existing candidate folder")
    # A successful publication MUST be distinct from the checkpoint tree.
    if (candidate_folder.resolve().is_relative_to(workdir.resolve())
        or workdir.resolve().is_relative_to(candidate_folder.resolve())):
        raise ContractError("checkpoint and candidate folders must be disjoint")
    seeded = seed_importer(workdir, seed_zip)
    if (seeded.get("ratios") != 4
        or seeded.get("source_ci_artifact_pins_verified") is not True
        or seeded.get("release_qualified") is not False):
        raise ContractError("four real Q0 provenance seeds not verified")
    staged = generator(
        workdir, workers=workers, max_precision=max_precision,
        progress=lambda message: print(message, flush=True))
    if (staged.get("ratios") != 14
        or staged.get("status") !=
            "14_DOUBLE_RUN_CANDIDATES_CHECKPOINTED_NOT_RELEASE"
        or staged.get("product_admission_authorized") is not False):
        raise ContractError("all fourteen genuine double-run checkpoints required")
    # No synthetic generator, partial pair map or unverified asset.
    from independent_mpmath_crosscheck import compare_candidate
    from exact_dyadic_zero_stuff_oracle import screen_real_candidate_dyadic
    from screen_candidate_spectral_response import screen_asset
    from exact_dyadic_spectral_anchors import screen_exact_spectral_anchors
    manifest = publisher(
        workdir, candidate_folder, max_precision=max_precision,
        independent_screen=compare_candidate,
        dyadic_screen=screen_real_candidate_dyadic,
        sampled_fft_screen=screen_asset,
        exact_anchor_screen=screen_exact_spectral_anchors)
    if (manifest.get("observed_ratio_count") != 14
        or manifest.get("complete_ratio_family") is not True
        or manifest.get("independent_mpmath_screen_complete") is not True
        or manifest.get("exact_dyadic_screen_complete") is not True
        or manifest.get("sampled_fft_screen_complete") is not True
        or manifest.get("exact_two_point_screen_complete") is not True
        or manifest.get("q1_q6_passed") is not False
        or manifest.get("product_admission_authorized") is not False
        or manifest.get("status") != "CANDIDATE_NOT_RELEASE_QUALIFIED"):
        raise ContractError("complete 14-asset Q0 numerical negative screen unresolved")
    audit = verifier(candidate_folder, require_complete=True)
    if (audit.get("complete") is not True
        or audit.get("status") not in
            ("CANDIDATE_FAMILY_INTEGRITY_PASS_NOT_RELEASE",)):
        raise ContractError("all-14 candidate folder independently failed integrity audit")
    return {
        "status": "FOUR_SEEDS_PLUS_TEN_GENUINE_MPFR_Q0_SCREENS_COMPLETE_NONRELEASE",
        "seeded_ratio_count": 4, "genuinely_generated_remaining_ratios": 10,
        "total_distinct_ratios": 14,
        "ratio_pair_mappings": 30,
        "independent_generator_interval_proven": False,
        "continuous_dtft_proven": False,
        "q1_q6_passed": False,
        "release_qualified": False,
        "product_admission_authorized": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed-checkpoints-zip", type=Path, required=True)
    parser.add_argument("--checkpoint-workdir", type=Path, required=True)
    parser.add_argument("--candidate-folder", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--max-precision", type=int, default=1024)
    a = parser.parse_args()
    result = run_campaign(
        a.seed_checkpoints_zip, a.checkpoint_workdir, a.candidate_folder,
        workers=a.workers, max_precision=a.max_precision)
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
