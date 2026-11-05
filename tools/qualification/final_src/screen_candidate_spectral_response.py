#!/usr/bin/env python3
"""Noncertifying, bounded FFT screen of offline RGSMSR1 CANDIDATE bytes.

This is NOT SRC_RESPONSE_ORACLE/1.0.0: the release oracle must use rigorous
MPFR DTFT / interval bounds and freeze Q1-Q6 results independently.
NumPy is an OFFLINE optional dependency, not a runtime/build dependency.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path

from phase11_math_preflight import Ratio, kernel_shape, audit_rgsmsr1_structure


def screen_asset(data: bytes, fin: int, fout: int,
                 expected_sha256: str, *, minimum_fft: int = 1 << 19) -> dict:
    try:
        import numpy as np
    except ImportError as e:
        raise RuntimeError("NumPy needed ONLY for offline noncertifying screen") from e
    ratio = Ratio.make(fin, fout)
    shape = kernel_shape(ratio)
    actual = audit_rgsmsr1_structure(data, shape)
    if actual != expected_sha256 or sha256(data).hexdigest() != expected_sha256:
        raise ValueError("candidate kernel checksum mismatch")
    if minimum_fft < 1 << 15 or minimum_fft & (minimum_fft - 1):
        raise ValueError("minimum FFT size must be a power of two >=32768")
    h = np.frombuffer(data, dtype="<f8", count=shape.N, offset=52)
    nfft = 1 << max(minimum_fft - 1, 4 * shape.N - 1).bit_length()
    frequency_step = (fin * shape.L) / nfft
    grid = np.arange(nfft // 2 + 1, dtype=np.float64) * frequency_step
    # The conceptual high-rate FIR has DC gain L. Divide by L to compare
    # normalized audio-band transfer and anti-alias rejection.
    response = abs(np.fft.rfft(h, n=nfft)) / shape.L
    passband = response[grid <= 0.48 * min(fin, fout)]
    stopband = response[grid >= 0.50 * min(fin, fout)]
    if passband.size < 2 or stopband.size < 2:
        raise ValueError("insufficient FFT samples")
    db = 20.0 * np.log10(np.maximum(passband, np.finfo(float).tiny))
    ripple_pp_db = float(np.max(db) - np.min(db))
    pass_max_abs_db = float(np.max(np.abs(db)))
    attenuation_db = float(-20.0 * np.log10(max(
        float(np.max(stopband)), float(np.finfo(float).tiny))))
    return dict(fin=fin, fout=fout, ratio=f"{shape.L}/{shape.M}",
                nfft=int(nfft), frequency_step_hz=frequency_step,
                ripple_peak_to_peak_db=ripple_pp_db,
                max_passband_error_db=pass_max_abs_db,
                sampled_stopband_attenuation_db=attenuation_db,
                sampled_pass=(ripple_pp_db <= 0.001
                              and pass_max_abs_db <= 0.001
                              and attenuation_db >= 140.0),
                caveat="SAMPLED_FFT_ONLY_NOT_CERTIFIED_DTFT_OR_RELEASE")


def screen_candidate_folder(folder: Path) -> dict:
    doc = json.loads((folder / "CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json").read_text())
    if doc["status"] != "CANDIDATE_NOT_RELEASE_QUALIFIED":
        raise ValueError("must consume only non-release candidate manifests")
    rows = []
    for entry in doc["assets"]:
        data = (folder / entry["filename"]).read_bytes()
        rows.append(screen_asset(data, entry["fin"], entry["fout"],
                                 entry["candidate_sha256"]))
    return dict(status="SAMPLED_FFT_SCREEN_ONLY_NOT_RELEASE_QUALIFIED",
                tested=len(rows), passed=sum(x["sampled_pass"] for x in rows),
                results=rows)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("candidate_folder", type=Path)
    args = p.parse_args()
    result = screen_candidate_folder(args.candidate_folder)
    print(json.dumps(result, indent=2, sort_keys=True))
    if result["passed"] != result["tested"]:
        raise SystemExit(1)
