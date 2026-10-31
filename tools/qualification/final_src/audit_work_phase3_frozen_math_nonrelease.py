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
from phase11_math_preflight import RATES


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
    return {
        "status": "FROZEN_DOMAIN_AND_ERROR_MODEL_AUDITED_NONRELEASE",
        "complete_band_domains": n,
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
