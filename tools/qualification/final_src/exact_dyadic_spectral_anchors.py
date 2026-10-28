#!/usr/bin/env python3
"""Provable TWO-POINT RGSMSR1 frequency-domain screen (NON-RELEASE).

Unlike sampled FFTs, both endpoint transfer values are evaluated as EXACT
rational sums of the binary64 tap bits, including all 14 reduced ratios.
These two frequencies alone do NOT prove any frequency INTERVAL or meet
the required independent MPFR continuous DTFT/Q1–Q6 release oracle.
"""
from __future__ import annotations

from fractions import Fraction
from hashlib import sha256

from exact_dyadic_zero_stuff_oracle import read_locked_kernel
from phase11_math_preflight import Ratio, ContractError


def screen_exact_spectral_anchors(asset_bytes: bytes, fin: int,
                                  fout: int) -> dict:
    ratio = Ratio.make(fin, fout)
    if fin == fout:
        raise ContractError("identity is bit transparent, no kernel")
    digest = sha256(asset_bytes).hexdigest()
    shape, taps = read_locked_kernel(asset_bytes, ratio, digest)
    # The high-grid FIR's normalized H(0) = sum(h[j])/L,
    # H(pi) = sum((-1)^j*h[j])/L. Coefficients are EXACT IEEE dyadics.
    dc = sum(taps, Fraction(0)) / ratio.L
    nyquist = sum(((-tap if j & 1 else tap)
                   for j, tap in enumerate(taps)), Fraction(0)) / ratio.L
    # Deliberately strong rational negative screens, not a surrogate for
    # the frozen whole-passband 0.001dB or whole-stopband -140dB proof.
    dc_pass = abs(dc - 1) <= Fraction(1, 1_000_000)
    nyquist_pass = abs(nyquist) <= Fraction(1, 10_000_000)
    return dict(
        status="EXACT_DYADIC_TWO_POINT_SCREEN_NOT_CERTIFIED_DTFT",
        fin=fin, fout=fout, L=shape.L, M=shape.M, taps=shape.N,
        candidate_sha256=digest,
        dc_gain_exact_numerator=dc.numerator,
        dc_gain_exact_denominator=dc.denominator,
        high_grid_nyquist_exact_numerator=nyquist.numerator,
        high_grid_nyquist_exact_denominator=nyquist.denominator,
        dc_within_strong_rational_screen=dc_pass,
        high_grid_nyquist_within_strong_rational_screen=nyquist_pass,
        full_frequency_band_proven=False,
        release_qualified=False,
    )
