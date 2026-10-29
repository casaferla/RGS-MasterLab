#!/usr/bin/env python3
"""Import FOUR previously audited REAL MPFR/Ziv CI Q0 candidates as checkpoints.

This is an OFFLINE workstation bootstrap, not a release gate. It NEVER
generates coefficients, uses only independently pinned GitHub CI artifacts,
and refuses mismatched ZIPs, provenance or binary64 geometry. The resulting
four REAL same-generator-double-run checkpoints are read by the EXISTING
resumable all-14 generator, which must still compute the other ten ratios.
Independent continuum DTFT, Q1-Q6, full SHA release approval, and all
production SRC admissions remain entirely separate and REQUIRED.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import tempfile
import zipfile

from build_candidate_manifest import canonical_pair_for_ratio
from build_resumable_real_candidates import (
    BACKEND, RECEIPT, RECIPE, _read_checkpoint, _slot,
)
from phase11_math_preflight import ContractError, Ratio, audit_rgsmsr1_structure, kernel_shape

HERE = Path(__file__).resolve().parent


def _read_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ContractError("Q0 CI baseline JSON must be an object")
    return value


def _pinned_candidates() -> tuple[dict, ...]:
    two = _read_json(HERE / "Q0_REAL_2TO1_CANARY_PIN_NOT_FOR_RELEASE.json")
    one = _read_json(HERE / "Q0_REAL_1TO2_CANARY_PIN_NOT_FOR_RELEASE.json")
    four = _read_json(HERE / "Q0_REAL_FOURFOLD_CANARY_PINS_NOT_FOR_RELEASE.json")
    if two.get("status") != "Q0_BITWISE_REPRODUCIBILITY_PIN_NOT_RELEASE" or (
        two.get("ratio_L"), two.get("ratio_M")) != (2, 1):
        raise ContractError("2/1 prior Q0 pin invalid")
    if one.get("status") != "Q0_BITWISE_REPRODUCIBILITY_PIN_NOT_RELEASE" or (
        one.get("ratio_L"), one.get("ratio_M")) != (1, 2):
        raise ContractError("1/2 prior Q0 pin invalid")
    if (four.get("status") != "CROSS_RUN_REAL_MPFR_CANDIDATES_NOT_FOR_RELEASE"
        or len(four.get("baselines", [])) != 2
        or four.get("independently_inspected_actual_artifact") is not True):
        raise ContractError("fourfold prior Q0 pins invalid")
    pairs = [
        dict(L=2, M=1, pair=two["canonical_generation_pair_hz"],
             sha=two["baseline_candidate_sha256"],
             size=two["baseline_asset_bytes"], N=two["baseline_coefficient_count"],
             bits=two["baseline_precision_bits"],
             run=two["source_github_run_id"],
             pr=two["source_pr_sha"],
             filename="rgsml-phase11-real-mpfr-48to96.bin",
             archive_key="two"),
        dict(L=1, M=2, pair=one["canonical_generation_pair_hz"],
             sha=one["baseline_candidate_sha256"],
             size=one["baseline_asset_bytes"], N=one["baseline_coefficient_count"],
             bits=one["baseline_precision_bits"],
             run=one["source_github_run_id"],
             pr=one["source_pr_sha"],
             filename="rgsml-phase11-real-mpfr-88kto44k.bin",
             archive_key="one"),
    ]
    for item in four["baselines"]:
        L, M = item["L"], item["M"]
        pairs.append(dict(
            L=L, M=M, pair=item["canonical_generation_pair_hz"],
            sha=item["sha256"], size=item["bytes"],
            N=item["coefficient_count"], bits=item["precision_bits"],
            run=four["source_github_run_id"], pr=four["source_pr_sha"],
            filename=f"ratio_L{L}_M{M}.bin", archive_key="four"))
    if {(p["L"], p["M"]) for p in pairs} != {
            (1, 2), (2, 1), (1, 4), (4, 1)}:
        raise ContractError("Q0 bootstrap must contain exactly four distinct ratios")
    for p in pairs:
        pair = canonical_pair_for_ratio().get((p["L"], p["M"]))
        if list(pair or ()) != p["pair"]:
            raise ContractError("pinned rate alias not canonical")
        shape = kernel_shape(Ratio.make(*pair))
        if (p["N"] != shape.N or p["size"] != shape.frozen_bytes
            or type(p["bits"]) is not int
            or p["bits"] not in (256, 512, 1024)
            or type(p["run"]) is not int or p["run"] <= 0
            or not isinstance(p["sha"], str) or len(p["sha"]) != 64):
            raise ContractError("pinned Q0 geometry/precision/provenance invalid")
    return tuple(pairs)


def _validated_zip_payload(path: Path, p: dict) -> bytes:
    if not path.is_file() or path.is_symlink():
        raise ContractError("Q0 original CI ZIP missing or symlink")
    try:
        with zipfile.ZipFile(path) as z:
            expected = {p["filename"], p["filename"] + ".expected.f64le",
                        p["filename"] + ".receipt.json"}
            if set(z.namelist()) != expected:
                raise ContractError("Q0 CI ZIP has missing or unexpected files")
            for name in expected:
                item = z.getinfo(name)
                if item.file_size > 8 * 1024 * 1024:
                    raise ContractError("Q0 CI ZIP entry exceeds bounded size")
            data = z.read(p["filename"])
            receipt = json.loads(z.read(p["filename"] + ".receipt.json"))
            expected_pcm = z.read(p["filename"] + ".expected.f64le")
    except (OSError, zipfile.BadZipFile, ValueError, KeyError) as error:
        raise ContractError("Q0 CI ZIP could not be securely audited") from error
    if not isinstance(receipt, dict):
        raise ContractError("Q0 CI receipt must be a JSON object")
    L, M = p["L"], p["M"]
    ratio_field = ("kernel_ratio" if (L, M) == (2, 1) else "ratio")
    mp_field = ("independent_mpmath_both_precisions_matched" if (L, M) == (2, 1)
                else "independent_mpmath_two_precisions_match")
    if (receipt.get("coefficient_sha256") != p["sha"]
        or receipt.get("coefficient_count") != p["N"]
        or receipt.get("payload_bytes") != p["size"]
        or receipt.get("rounding_precision_bits") != p["bits"]
        or receipt.get("canonical_generation_pair_hz") != p["pair"]
        or receipt.get(ratio_field) != {"L": L, "M": M}
        or str(receipt.get("github_run_id")) != str(p["run"])
        or receipt.get("git_pr_head_sha") != p["pr"]
        or receipt.get("same_generator_reproductions") != 2
        or receipt.get(mp_field) is not True
        or receipt.get("release_qualified") is not False
        or receipt.get("q1_q6_passed") is not False
        or receipt.get("product_admission_authorized") is not False):
        raise ContractError("Q0 CI receipt provenance or proof fields mismatch")
    if (L, M) != (2, 1):
        if (receipt.get("exact_dyadic_crop_invariant") is not True
            or not all(screen.get("sampled_pass") is True
                       and screen.get("nfft") == 1 << 20
                       for screen in receipt.get("sampled_fft_negative_screens", []))
            or len(receipt.get("sampled_fft_negative_screens", [])) != 2):
            raise ContractError("Q0 CI independent numeric screens missing")
    if (len(data) != p["size"] or sha256(data).hexdigest() != p["sha"]
        or len(expected_pcm) != (64 if (L, M) == (2, 1)
                                else 32 if (L, M) == (1, 2)
                                else 16 if (L, M) == (1, 4)
                                else 256)):
        raise ContractError("Q0 CI candidate bytes or expected PCM invalid")
    shape = kernel_shape(Ratio.make(*p["pair"]))
    if audit_rgsmsr1_structure(data, shape) != p["sha"]:
        raise ContractError("Q0 CI candidate binary64 geometry invalid")
    return data


def import_pinned_ci_q0(workdir: Path, two_zip: Path, one_zip: Path,
                        four_zip: Path) -> dict:
    """Pre-audit ALL four artifacts before publishing any checkpoint."""
    if workdir.is_symlink():
        raise ContractError("Q0 checkpoint workdir must not be a symlink")
    archives = {"two": two_zip, "one": one_zip, "four": four_zip}
    pairs = _pinned_candidates()
    ready = [(p, _validated_zip_payload(archives[p["archive_key"]], p))
             for p in pairs]
    workdir.mkdir(parents=True, exist_ok=True)
    for p, data in ready:
        slot = _slot(workdir, p["L"], p["M"])
        if slot.exists() or slot.is_symlink():
            prior, bits = _read_checkpoint(
                workdir, *p["pair"], allow_test_generator=False)
            if prior != data or bits != p["bits"]:
                raise ContractError("existing Q0 checkpoint conflicts with CI pin")
    with tempfile.TemporaryDirectory(
        prefix=".ci-q0-four-import-unpublished-", dir=workdir) as temp:
        root = Path(temp)
        for p, data in ready:
            if _slot(workdir, p["L"], p["M"]).exists():
                continue
            slot = root / f"ratio_L{p['L']}_M{p['M']}"
            slot.mkdir()
            shape = kernel_shape(Ratio.make(*p["pair"]))
            receipt = {
                "recipe": RECIPE, "backend": BACKEND,
                "fin": p["pair"][0], "fout": p["pair"][1],
                "L": p["L"], "M": p["M"], "T": shape.T, "N": shape.N,
                "bytes": len(data), "sha256": p["sha"],
                "precision_bits": p["bits"],
                "same_generator_double_run": True,
                "independent_generator_verified": False,
                "mpfr_dtft_oracle_passed": False,
                "q1_q6_passed": False, "release_qualified": False,
                "product_admission_authorized": False,
                "origin": "INDEPENDENTLY_AUDITED_CI_Q0_ARTIFACT",
                "origin_github_run_id": p["run"],
                "origin_pr_sha": p["pr"],
                "origin_verification": "ORIGINAL_CI_Q0_CHECKS_NOT_RELEASE",
            }
            (slot / f"RGSMSR1_L{p['L']}_M{p['M']}.bin").write_bytes(data)
            (slot / RECEIPT).write_text(
                json.dumps(receipt, sort_keys=True, indent=2) + "\n",
                encoding="utf-8")
        for p, _ in ready:
            slot = _slot(workdir, p["L"], p["M"])
            if slot.exists():
                continue
            (root / slot.name).rename(slot)
    for p, data in ready:
        checked, bits = _read_checkpoint(workdir, *p["pair"])
        if checked != data or bits != p["bits"]:
            raise ContractError("imported REAL Q0 checkpoint verification failed")
    return {
        "status": "FOUR_REAL_CI_Q0_CHECKPOINTS_IMPORTED_NOT_FOR_RELEASE",
        "ratios": 4, "remaining_genuine_ratio_candidates": 10,
        "source_ci_artifact_pins_verified": True,
        "release_qualified": False, "product_admission_authorized": False,
        "assets": [{"L": p["L"], "M": p["M"], "sha256": p["sha"]}
                   for p, _ in ready],
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--workdir", type=Path, required=True)
    ap.add_argument("--real-2to1-zip", type=Path, required=True)
    ap.add_argument("--real-1to2-zip", type=Path, required=True)
    ap.add_argument("--real-fourfold-zip", type=Path, required=True)
    args = ap.parse_args()
    print(json.dumps(import_pinned_ci_q0(
        args.workdir, args.real_2to1_zip, args.real_1to2_zip,
        args.real_fourfold_zip), sort_keys=True))


if __name__ == "__main__":
    main()
