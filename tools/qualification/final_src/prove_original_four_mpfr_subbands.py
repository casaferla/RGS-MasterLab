#!/usr/bin/env python3
"""A genuine FOUR-kernel, EIGHT continuous MPFR DTFT SUBBAND proof, NONRELEASE.

This is not a proof of either COMPLETE passband or stopband. It uses the
four original CI Q0 canaries; each must be real frozen binary64 and match an
independently pinned SHA. All evidence is generated from scratch on runner.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path

from dtft_second_order_rigorous import prove_window
from exact_dyadic_zero_stuff_oracle import read_locked_kernel
from phase11_math_preflight import ContractError, Ratio


def certify_four_subbands(two: Path, one: Path, four: Path) -> dict:
    entries = (
        (48000, 96000, two,
         "6fb38a7d62859283e4061a18d23f354a8308d5b22ff55623e8b1c9a245e8b00c"),
        (88200, 44100, one,
         "b97776527687180bea6dbf86a8da877c0bdaa528d305abe4b7beaf806ecaf48e"),
        (176400, 44100, four / "ratio_L1_M4.bin",
         "3c5db068904a059fae54ab3a96ae96ec3963fc31e649f5f4de9ecf5b6d17a947"),
        (44100, 176400, four / "ratio_L4_M1.bin",
         "f2563518411bc6525889eaf82c43b194f88ceadd59b0750071c613d1071de095"),
    )
    observed = []
    for fin, fout, file, pinned in entries:
        data = file.read_bytes()
        if sha256(data).hexdigest() != pinned:
            raise ContractError("non-original or tampered genuine Q0 canary")
        ratio = Ratio.make(fin, fout)
        _, taps = read_locked_kernel(data, ratio, pinned)
        for kind, low, high in (
            ("passband", 3000, 3001),
            ("stopband", 34000, 34001),
        ):
            if (kind == "passband" and high > 12 * min(fin,fout) / 25
                or kind == "stopband" and low < min(fin,fout)/2):
                raise ContractError("outside frozen band")
            result = prove_window(
                ratio, taps, low, high, kind=kind, max_boxes=256, bits=256)
            if (result["status"] != "CONTINUUM_SUBBAND_PROVEN_NONRELEASE"
                or result["remaining_boxes"] != 0
                or result["release_qualified"] is not False):
                raise ContractError("real 1-Hz continuum subband UNRESOLVED")
            observed.append(dict(
                fin=fin, fout=fout, L=ratio.L, M=ratio.M,
                candidate_sha256=pinned, kind=kind,
                lo_hz=low, hi_hz=high, **result))
    if len(observed) != 8:
        raise ContractError("missing authentic four-kernel proof")
    return dict(
        schema="rgsml.phase11.mpfr-original4-true-continuum-8subbands/1",
        status="EIGHT_GENUINE_CONTINUUM_SUBBANDS_PROVEN_NOT_RELEASE",
        proved_subbands=len(observed),
        distinct_real_q0_kernels=4,
        entire_passband_stopband_proved=False,
        fourteen_ratio_continuum_proved=False,
        q1_q6_passed=False,
        product_admission_authorized=False,
        release_qualified=False,
        results=observed)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--two", type=Path, required=True)
    p.add_argument("--one", type=Path, required=True)
    p.add_argument("--fourfold-dir", type=Path, required=True)
    p.add_argument("--report", type=Path, required=True)
    a = p.parse_args()
    if a.report.exists() or a.report.is_symlink():
        raise ContractError("continuum evidence may only write a new report")
    report = certify_four_subbands(a.two, a.one, a.fourfold_dir)
    a.report.write_text(json.dumps(report, sort_keys=True, indent=2)+"\n")
    print(report["status"])


if __name__ == "__main__":
    main()
