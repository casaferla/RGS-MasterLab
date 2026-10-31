#!/usr/bin/env python3
"""Independent frozen-geometry and exact-rational Work Phase3 Q0 receipt review.

PRIVATE ZIP supplied by the operator; this is read-only and NONRELEASE.
The pinned archive integrity audit runs first. This separate source
reconstructs the 28 canonical frequency domains and declared IEEE/FFT
error inequalities. It is NOT an independent re-execution of 219128
spectral cells or certified MPFR coefficient generation, nor SRC release.
"""
from __future__ import annotations

from fractions import Fraction as F
from math import gcd
from pathlib import Path
from zipfile import ZipFile
import argparse
import json

from audit_work_phase3_handoff_nonrelease import (
    audit, AuditError, parsed, require,
)
from phase11_math_preflight import RATES, Ratio, kernel_shape


def canonical_ratio_pairs():
    pairs = {}
    for source in RATES:
        for master in RATES:
            if source == master:
                continue
            common = gcd(source, master)
            key = (master // common, source // common)
            pairs.setdefault(key, (source, master))
    require(len(pairs) == 14, "frozen SOURCE/MASTER ratio count changed")
    require(sum(1 for a in RATES for b in RATES if a != b) == 30,
            "frozen nonidentity route count changed")
    return pairs


def _exact_integer(value, label):
    require(type(value) is str and len(value) <= 30
            and value.isascii() and value.isdecimal(),
            label + ": canonical nonnegative integer required")
    return int(value)


def verify_frozen_band_domains(bands):
    """Reconstruct Hz intervals from six rates; never trust ZIP endpoints."""
    pairs = canonical_ratio_pairs()
    require(type(bands) is list and len(bands) == 28,
            "28 frozen spectral bands required")
    seen = set()
    for band in bands:
        require(type(band) is dict, "nonobject spectral band")
        L, M, kind = band.get("L"), band.get("M"), band.get("kind")
        require(type(L) is int and type(M) is int and (L, M) in pairs
                and kind in ("passband", "stopband"),
                "invalid ratio or spectral band kind")
        require((L, M, kind) not in seen, "duplicate band")
        source, master = pairs[(L, M)]
        nyquist = min(source, master) // 2
        # The frozen passband is 96% of the SMALLER Nyquist, not 96%
        # of the source Nyquist; the full stopband runs to L*Fin/2.
        expected = ((0, F(12, 25) * min(source, master))
                    if kind == "passband"
                    else (nyquist, F(source * L, 2)))
        actual = (_exact_integer(band.get("lo_hz"), "band lower"),
                  _exact_integer(band.get("hi_hz"), "band upper"))
        require((F(actual[0]), F(actual[1])) == expected
                and actual[0] < actual[1],
                "frozen spectral domain endpoint differs from ratio contract")
        require(band.get("status") == "VERIFIED_NONRELEASE",
                "band claimed a release or incomplete state")
        seen.add((L, M, kind))
    require(seen == {(L, M, kind) for L, M in pairs
                     for kind in ("passband", "stopband")},
            "missing canonical spectral interval")
    return 28


def _fraction_field(model, key):
    value = model.get(key)
    require(type(value) is str and 0 < len(value) < 10000,
            key + ": missing or oversized exact rational")
    try:
        return F(value)
    except (ValueError, ZeroDivisionError) as e:
        raise AuditError(key + ": invalid rational") from e


def verify_exact_covered_error_model(model):
    """Recompute the frozen inequalities, then check recorded exact values."""
    require(type(model) is dict and
            model.get("status") ==
                "MATHEMATICAL_ERROR_MODEL_REDERIVED_PASS_UNDER_CHECKED_IEEE_ASSUMPTIONS"
            and model.get("runtime_guard_gap_found") is True
            and model.get("release_qualified") is False,
            "nonrelease IEEE/error-model status mismatch")
    u, eta = F(1, 2**53), F(1, 2**38)
    g2 = 2 * u / (1 - 2 * u)
    safe = (1 + 16 * u)**256 - 1
    local = eta + 2*g2*(1+eta) + u*(1+eta+2*g2*(1+eta))
    c = 4*eta + 64*u
    q = 22*c / (1-22*c)
    gamma81 = 81*u / (1-81*u)
    total = q*(1+gamma81)+gamma81
    actual = {
        "twiddle_upper": safe, "twiddle_limit": eta,
        "butterfly_local_upper": local, "used_c": c,
        "accumulated_fft_and_input_upper": total,
        "used_envelope": F(1, 2**29),
        "underflow_bound": F(2**69, 2**1075),
        "used_underflow": F(1, 2**900),
    }
    for key, expected in actual.items():
        require(_fraction_field(model, key) == expected,
                "authentic Phase3 model differs from independently computed "+key)
    require(safe < eta and local < c and total < F(1, 2**29)
            and F(2**69, 2**1075) < F(1, 2**900)
            and F(1, 1-gamma81) < 1 + F(1, 2**40),
            "frozen sufficient error inequalities fail")
    require(type(model.get("underflow_real_operation_count_bound")) is int
            and model["underflow_real_operation_count_bound"] == 2**34
            and type(model.get("underflow_propagation_bound")) is int
            and model["underflow_propagation_bound"] == 2**35,
            "underflow operation/propagation count incorrect")
    return True


def verify_independent_rounding_receipt_shapes(rows, receipts):
    """Cross-check all 28 signed-off *claims* against frozen Q0 geometry.

    Receipts are externally SHA-pinned by the parent package audit. This
    function checks internal shape/metadata contradictions, not the MPFR
    proof execution or correctness of original proprietary FIR bytes.
    """
    pairs = canonical_ratio_pairs()
    require(type(rows) is list and len(rows) == 28
            and type(receipts) is dict and len(receipts) == 28,
            "exact 28 original coefficient receipts required")
    seen = set()
    kernels = {}
    producer_sources = set()
    helpers = set()
    for row in rows:
        require(type(row) is dict, "invalid reproduction summary")
        L, M, bits = row.get("L"), row.get("M"), row.get("bits")
        require(type(L) is int and type(M) is int
                and type(bits) is int and (L, M) in pairs
                and bits in (256, 512) and (L, M, bits) not in seen,
                "noncanonical or duplicate coefficient precision entry")
        seen.add((L, M, bits))
        source, master = pairs[(L, M)]
        shape = kernel_shape(Ratio.make(source, master))
        require(row.get("coefficient_count") == shape.N
                and type(row.get("coefficient_count")) is int,
                "reproduced FIR coefficient count differs from frozen shape")
        key = (L, M, bits)
        receipt = receipts.get(key)
        require(type(receipt) is dict and
                (receipt.get("L"), receipt.get("M"), receipt.get("bits")) == key
                and receipt.get("shape_formula_exact_pi_interval_verified") is True
                and receipt.get("header_independent_regeneration") is True
                and receipt.get("original_header_reused_and_independently_parsed") is False
                and receipt.get("all_binary64_rounding_cells_proven") is True
                and receipt.get("payload_bitwise_equal") is True
                and receipt.get("release_qualified") is False,
                "coefficient receipt shape/RN assertions not satisfied")
        digest = row.get("asset_sha256")
        require(type(digest) is str and len(digest) == 64
                and all(c in "0123456789abcdef" for c in digest)
                and receipt.get("asset_sha256") == digest
                and receipt.get("expected_sha256") == digest
                and ((L, M) not in kernels or kernels[(L, M)] == digest),
                "binary64 256/512 coefficient payload SHA discrepancy")
        kernels[(L, M)] = digest
        expected_note = (
            f"DIRECTED_COEFFICIENT_ROUNDING_CELLS_PROVEN {shape.N} "
            f"BITS {bits} MPFR "
        )
        require(type(receipt.get("transcript")) is str
                and receipt["transcript"].startswith(expected_note),
                "wrong geometry or precision in directed rounding receipt")
        producer_sources.add(receipt.get("source_sha256"))
        helpers.add(receipt.get("arithmetic_helper_sha256"))
    require(seen == {(L, M, b) for L, M in pairs for b in (256, 512)}
            and len(kernels) == 14
            and len(producer_sources) == len(helpers) == 1,
            "incomplete cross-precision or inconsistent generator provenance")
    return 28


def verify_phase3_q1_q2_subset_receipts(document, kernels):
    """Check all 72 claims against canonical 36 path/2-recipe geometry.

    A receipt SHA is *not* equivalent to executing Qt or comparing PCM.
    This does not confer Q1/Q2 release authority on the current product.
    """
    require(type(document) is dict
            and document.get("cases") == 72
            and document.get("source_master_pairs") == 36
            and document.get("mpfr_vs_dyadic_exact_cases") == 72
            and document.get("scalar_partition_and_crop_cases") == 72
            and document.get("identity_special_bit_cases") == 6
            and document.get("full_Q1_gate") == "OPEN"
            and document.get("full_Q2_gate") == "OPEN"
            and document.get("release_qualified") is False
            and document.get("product_admission_authorized") is False,
            "Phase3 Q1/Q2 report must remain a complete NONRELEASE subset")
    rows = document.get("results")
    require(type(rows) is list and len(rows) == 72,
            "72 source/master/recipe receipts required")
    seen = set()
    for row in rows:
        require(type(row) is dict, "invalid Q1/Q2 receipt row")
        fin, fout, recipe = row.get("fin"), row.get("fout"), row.get("case")
        require(type(fin) is int and type(fout) is int
                and fin in RATES and fout in RATES
                and recipe in ("short_17", "one_frame"),
                "invalid SOURCE/MASTER/recipe")
        key = fin, fout, recipe
        require(key not in seen, "duplicate Phase3 SRC recipe")
        seen.add(key)
        source_frames = 17 if recipe == "short_17" else 1
        frames = (source_frames * fout + fin - 1) // fin
        digest = row.get("expected_sha256")
        require(row.get("frames") == frames
                and type(row.get("frames")) is int
                and type(digest) is str and len(digest) == 64
                and all(c in "0123456789abcdef" for c in digest),
                "wrong SRC output frame count or independent vector digest")
        ratio = (fout//gcd(fin, fout), fin//gcd(fin, fout))
        expected_kernel = None if fin == fout else kernels.get(ratio)
        require((fin == fout or expected_kernel is not None)
                and row.get("kernel_sha256") == expected_kernel,
                "Phase3 vector uses an unpinned Q0 coefficient ratio")
        require(all(row.get(name) is True for name in (
                    "mpfr_zero_stuffed_rounding_cells_proven",
                    "exact_dyadic_independent_bitwise_match",
                    "strict_scalar_absolute_partition_bitwise_equal",
                    "strict_scalar_crop_bitwise_equal"))
                and row.get("canonical_qtp_profile_applied") is False
                and row.get("actual_product_qt_executed") is False
                and row.get("release_qualified") is False,
                "Phase3 subset claims unexecuted product Qt or misses exact oracle")
        require(row.get("transcript") ==
                f"FULL_ZERO_STUFF_CONVOLUTION_ROUNDING_PROVEN "
                f"{frames} MPFR_BITS 256\n",
                "Phase3 result transcript is not the exact test geometry")
        error = row.get("strict_scalar_max_abs_error_vs_exact")
        require(type(error) is str and 0 < len(error) < 200
                and F(error) >= 0,
                "Phase3 exact dyadic error is invalid")
    require(seen == {(a, b, c) for a in RATES for b in RATES
                     for c in ("short_17", "one_frame")},
            "incomplete Phase3 36x2 subset matrix")
    return 72


def review(pinned_phase3_zip: Path):
    # Existing original-external-SHA audit also rejects unsafe archives,
    # duplicate JSON keys, forged release flags and unpinned sources.
    result = audit(pinned_phase3_zip.read_bytes())
    with ZipFile(pinned_phase3_zip) as z:
        band_doc = parsed(z.read(
            "phase3/IMPORTED_Q0_CANONICAL_PREPARATION_NONRELEASE.json"))
        err_doc = parsed(z.read("phase3/FORMAL_ERROR_REVIEW.json"))
        n = verify_frozen_band_domains(band_doc["bands"])
        verify_exact_covered_error_model(err_doc)
        reproduction = parsed(z.read(
            "phase3/INDEPENDENT_COEFFICIENT_AUDIT.json"))
        summaries = reproduction["reproductions"]
        receipts = {}
        for (L, M) in canonical_ratio_pairs():
            for bits in (256, 512):
                filename = (f"phase3/coefficient_reproduction/"
                            f"L{L}_M{M}_{bits}.json")
                receipts[(L, M, bits)] = parsed(z.read(filename))
        validated_rounding = verify_independent_rounding_receipt_shapes(
            summaries, receipts)
        kernels = {
            (row["L"], row["M"]): row["asset_sha256"]
            for row in summaries
        }
        subset = parsed(z.read("phase3/Q1_Q2_NEW_OFFLINE_SUBSET.json"))
        validated_q1_q2 = verify_phase3_q1_q2_subset_receipts(subset, kernels)
    return {
        "status": "FROZEN_DOMAIN_AND_ERROR_MODEL_AUDITED_NONRELEASE",
        "complete_band_domains": n,
        "reproduction_receipt_shapes": validated_rounding,
        "q1_q2_independent_subset_receipts": validated_q1_q2,
        "canonical_ratio_count": len(canonical_ratio_pairs()),
        "pinned_inventory": result["result"],
        "independent_spectral_cell_replay_executed": False,
        "canonical_q0_approved": False,
        "product_admission_authorized": False,
        "release_qualified": False,
    }


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("pinned_phase3_zip", type=Path)
    a = p.parse_args()
    print(json.dumps(review(a.pinned_phase3_zip), sort_keys=True))
