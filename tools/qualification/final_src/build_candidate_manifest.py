#!/usr/bin/env python3
"""Offline all-ratio Phase-11 RGSMSR1 CANDIDATE asset assembler.

Two reproductions by the SAME directed MPFR/Ziv generator are checked.
This is a Q0 production worklist, NOT independent oracle/Q1-Q6 qualification.
All assets and candidate manifest publish together atomically or not at all.
Product runtime MUST NOT consume any result from this utility as a release
manifest without separately approved independent coefficient and response gates.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import struct
import tempfile
from typing import Callable

from generate_rgsmsr1_mpfr import generate
from phase11_math_preflight import (
    RATES, Ratio, ContractError, audit_rgsmsr1_structure,
    kernel_shape, plan,
)


def canonical_pair_for_ratio() -> dict[tuple[int, int], tuple[int, int]]:
    result = {}
    for fin in RATES:
        for fout in RATES:
            if fin != fout:
                ratio = Ratio.make(fin, fout)
                result.setdefault((ratio.L, ratio.M), (fin, fout))
    if len(result) != 14:
        raise ContractError("Phase-11 distinct reduced ratio inventory changed")
    return dict(sorted(result.items()))


def assemble_candidate(
    destination: Path,
    *,
    generator: Callable[[int, int, int], tuple[bytes, int]] | None = None,
    max_precision: int = 1024,
    pairs: dict[tuple[int, int], tuple[int, int]] | None = None,
    independent_screen: Callable[[bytes, int, int], dict] | None = None,
    dyadic_screen: Callable[[bytes, int, int], dict] | None = None,
    sampled_fft_screen: Callable[[bytes, int, int, str], dict] | None = None,
    exact_anchor_screen: Callable[[bytes, int, int], dict] | None = None,
) -> dict:
    """Publish all requested candidate ratios as one atomic folder.

    The default is the complete frozen 14-ratio family. Tests may supply a
    SUBSET fixture; a subset is explicitly and permanently nonqualifying.
    Only an absent destination may be written: do not overwrite prior assets.
    """
    if max_precision < 256:
        raise ContractError("MPFR precision budget below 256")
    pairmap = canonical_pair_for_ratio() if pairs is None else dict(pairs)
    allowed = canonical_pair_for_ratio()
    if not pairmap or any(allowed.get(key) != pair for key, pair in pairmap.items()):
        raise ContractError("unrecognized reduced ratio or source/target pair")
    if destination.exists():
        raise ContractError("candidate destination already exists; never overwrite")
    destination.parent.mkdir(parents=True, exist_ok=True)
    backend = generator or (lambda fin, fout, bits: generate(fin, fout, max_precision=bits))
    entries = []
    # Temporary sibling is on same filesystem, enabling atomic final rename.
    with tempfile.TemporaryDirectory(prefix=".rgsmsr1-unpublished-",
                                     dir=destination.parent) as temp_name:
        temp = Path(temp_name)
        for (L, M), (fin, fout) in sorted(pairmap.items()):
            ratio = Ratio.make(fin, fout)
            shape = kernel_shape(ratio)
            data, first_bits = backend(fin, fout, max_precision)
            again, second_bits = backend(fin, fout, max_precision)
            if first_bits != second_bits or data != again:
                raise ContractError("ZIV_REPRODUCTION_MISMATCH: no candidate publication")
            digest = audit_rgsmsr1_structure(data, shape)
            if digest != sha256(data).hexdigest():
                raise ContractError("candidate checksum mismatch")
            if independent_screen is not None:
                # Independent mpmath at 120/160 dps is a reproducibility
                # SCREEN, not an interval proof or release checksum authority.
                screen = independent_screen(data, fin, fout)
                if not (screen.get("independent_precision_stable") is True
                        and screen.get("candidate_bytes_match_both") is True
                        and screen.get("release_qualified") is False):
                    raise ContractError(
                        "INDEPENDENT_NUMERICAL_SCREEN_FAILED: no candidate publication")
            if dyadic_screen is not None:
                # Exact dyadic PCM arithmetic of ACTUAL generated binary64
                # taps, independent j-ascending conceptual zero-stuffed grid.
                # Does not prove continuous spectrum or release readiness.
                dyadic = dyadic_screen(data, fin, fout)
                if not (dyadic.get("exact_dyadic_crop_invariant") is True
                        and dyadic.get("release_qualified") is False):
                    raise ContractError(
                        "EXACT_DYADIC_SCREEN_FAILED: no candidate publication")
            fft_evidence = None
            if sampled_fft_screen is not None:
                # Offline, optional dense-grid FFT is a NEGATIVE SCREEN:
                # failure rejects the candidate atomically, success still
                # does not prove the continuum MPFR DTFT or release Q1–Q6.
                fft_evidence = sampled_fft_screen(data, fin, fout, digest)
                if not (fft_evidence.get("sampled_pass") is True
                        and fft_evidence.get("caveat")
                            == "SAMPLED_FFT_ONLY_NOT_CERTIFIED_DTFT_OR_RELEASE"):
                    raise ContractError(
                        "SAMPLED_FFT_SCREEN_FAILED: no candidate publication")
            anchor_evidence = None
            if exact_anchor_screen is not None:
                # Only H(0) and high-grid H(pi) are EXACT dyadic here;
                # no full passband/stopband continuum can be certified.
                anchor_evidence = exact_anchor_screen(data, fin, fout)
                if not (anchor_evidence.get("dc_within_strong_rational_screen") is True
                        and anchor_evidence.get(
                            "high_grid_nyquist_within_strong_rational_screen") is True
                        and anchor_evidence.get("full_frequency_band_proven") is False
                        and anchor_evidence.get("release_qualified") is False):
                    raise ContractError(
                        "EXACT_TWO_POINT_SPECTRAL_SCREEN_FAILED: no candidate publication")
            name = f"RGSMSR1_L{L}_M{M}.bin"
            (temp / name).write_bytes(data)
            entries.append(dict(
                fin=fin, fout=fout, L=L, M=M, T=shape.T, N=shape.N,
                bytes=len(data), candidate_sha256=digest,
                precision_bits=first_bits, filename=name,
                status="SAME_GENERATOR_DOUBLE_REPRODUCED_NOT_RELEASE_QUALIFIED",
                independent_mpmath_screen_passed=(independent_screen is not None),
                exact_dyadic_screen_passed=(dyadic_screen is not None),
                sampled_fft_screen_passed=(sampled_fft_screen is not None),
                exact_two_point_screen_passed=(exact_anchor_screen is not None),
                sampled_fft_noncertifying_metrics=(
                    {key: fft_evidence[key] for key in (
                        "nfft", "frequency_step_hz",
                        "ripple_peak_to_peak_db", "max_passband_error_db",
                        "sampled_stopband_attenuation_db")
                     if key in fft_evidence}
                    if fft_evidence is not None else None),
            ))
        complete = set(pairmap) == set(allowed)
        manifest = dict(
            schema="rgsml.phase11.RGSMSR1-candidate-manifest/1",
            status="CANDIDATE_NOT_RELEASE_QUALIFIED",
            complete_ratio_family=complete,
            independent_generator_verified=False,
            independent_mpmath_screen_complete=(independent_screen is not None and complete),
            exact_dyadic_screen_complete=(dyadic_screen is not None and complete),
            sampled_fft_screen_complete=(sampled_fft_screen is not None and complete),
            exact_two_point_screen_complete=(exact_anchor_screen is not None and complete),
            mpfr_dtft_oracle_passed=False,
            q1_q6_passed=False,
            product_admission_authorized=False,
            expected_ratio_count=14, observed_ratio_count=len(entries),
            total_asset_bytes=sum(row["bytes"] for row in entries),
            assets=entries,
        )
        (temp / "CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True)+"\n",
            encoding="utf-8",
        )
        temp.rename(destination)
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True,
                        help="new empty candidate path; never a product asset directory")
    parser.add_argument("--max-precision", type=int, default=1024)
    parser.add_argument("--screen-independent-mpmath", action="store_true",
                        help="Require byte-identical independent 120/160-dps screen on ALL 14 real assets; still NOT release qualified")
    parser.add_argument("--screen-exact-dyadic", action="store_true",
                        help="Real 14-asset exact dyadic finite-track/crop screen; NOT MPFR Q1 or release qualification")
    parser.add_argument("--screen-sampled-fft", action="store_true",
                        help="Reject candidates failing an optional sampled FFT passband/stopband screen for all 14 ratios; NOT certified MPFR DTFT")
    parser.add_argument("--screen-exact-spectral-anchors", action="store_true",
                        help="Exact rational H(0), H(pi) negative screen on 14 real assets; not continuous DTFT release proof")
    args = parser.parse_args()
    checker = None
    if args.screen_independent_mpmath:
        from independent_mpmath_crosscheck import compare_candidate
        checker = compare_candidate
    dyadic = None
    if args.screen_exact_dyadic:
        from exact_dyadic_zero_stuff_oracle import screen_real_candidate_dyadic
        dyadic = screen_real_candidate_dyadic
    fft = None
    if args.screen_sampled_fft:
        from screen_candidate_spectral_response import screen_asset
        fft = screen_asset
    anchor = None
    if args.screen_exact_spectral_anchors:
        from exact_dyadic_spectral_anchors import screen_exact_spectral_anchors
        anchor = screen_exact_spectral_anchors
    manifest = assemble_candidate(args.out, max_precision=args.max_precision,
                                  independent_screen=checker,
                                  dyadic_screen=dyadic,
                                  sampled_fft_screen=fft,
                                  exact_anchor_screen=anchor)
    print(json.dumps({
        "status": manifest["status"],
        "ratios": manifest["observed_ratio_count"],
        "total_bytes": manifest["total_asset_bytes"],
        "independent_qualification": False,
        "independent_mpmath_screen_complete": manifest["independent_mpmath_screen_complete"],
        "exact_dyadic_screen_complete": manifest["exact_dyadic_screen_complete"],
        "sampled_fft_screen_complete": manifest["sampled_fft_screen_complete"],
        "exact_two_point_screen_complete": manifest["exact_two_point_screen_complete"],
    }, sort_keys=True))


if __name__ == "__main__":
    main()
