#!/usr/bin/env python3
"""REAL frozen SRC Q0: rigorous second-order MPFR DTFT continuum SUBBANDS.

Proof is for every real frequency in an EXPLICIT subinterval, not grid
samples. An incomplete/failed window is UNRESOLVED, never a release claim.
Whole-band certification and Q1-Q6 are independent uncompleted gates.
"""
from __future__ import annotations
import argparse
from fractions import Fraction
from hashlib import sha256
import json
from pathlib import Path

from exact_dyadic_zero_stuff_oracle import read_locked_kernel
from generate_rgsmsr1_mpfr import Engine, RNDD, RNDU
from mpfr_dtft_interval_preflight import (
    _mpfr_dyadic, _validate_dtft_input, PASS_TOL, STOP_TOL)
from phase11_math_preflight import ContractError, Ratio, pi_bounds


def second_moment(taps):
    d = (len(taps) - 1) // 2
    return sum((u*u*abs(taps[d+u]) for u in range(1, d+1)),
               Fraction(0))


def enclose_second_order(ratio, taps, left, right, *, bits=256):
    """Public entry: compute the exact second moment, no caller trust."""
    _validate_dtft_input(ratio, taps, bits)
    return _enclose_second_order_validated(
        ratio, taps, left, right,
        moment2=second_moment(taps), bits=bits)


def _enclose_second_order_validated(ratio, taps, left, right, *,
                                    moment2, bits=256):
    """Sound interval A(f) using exact rational second moment and MPFR slope."""
    _validate_dtft_input(ratio, taps, bits)
    left, right = Fraction(left), Fraction(right)
    high_rate = ratio.fin * ratio.L
    if left < 0 or right < left or right > Fraction(high_rate, 2):
        raise ContractError("second-order box outside high-grid Nyquist")
    center, radius = (left+right)/2, (right-left)/2
    e = Engine(bits)
    pi = e.pi()
    two = e.fraction(Fraction(2))
    d = (len(taps)-1)//2
    val = _mpfr_dyadic(e, taps[d])
    deriv = e.fraction(Fraction(0))
    for u in range(1, d+1):
        tap = taps[d+u]
        if not tap:
            continue
        cycles = (center*u/high_rate) % 1
        if cycles > Fraction(1, 2):
            cycles -= 1
        phase = two*pi*e.fraction(cycles)
        term = two*_mpfr_dyadic(e, tap)
        val = val + term*(phase+pi/two).sin()
        deriv = deriv-e.fraction(Fraction(u))*term*phase.sin()
    val = val/e.fraction(Fraction(ratio.L))
    deriv = deriv*two*pi/e.fraction(Fraction(ratio.L*high_rate))
    def bounds(x):
        return (Fraction.from_float(e.fn["get_d"](x.lo.ref, RNDD)),
                Fraction.from_float(e.fn["get_d"](x.hi.ref, RNDU)))
    val_lo, val_hi = bounds(val)
    d_lo, d_hi = bounds(deriv)
    pi_upper = pi_bounds()[1]
    sup_second = (8*pi_upper*pi_upper*moment2 /
                  (ratio.L*high_rate*high_rate))
    max_variation = (max(abs(d_lo), abs(d_hi))*radius +
                     sup_second*radius*radius/2)
    return val_lo-max_variation, val_hi+max_variation


def prove_window(ratio, taps, start, end, *,
                 kind, max_boxes=128, bits=256):
    _validate_dtft_input(ratio, taps, bits)
    if (kind not in ("passband","stopband")
        or type(max_boxes) is not int or max_boxes < 1
        or max_boxes > 1_000_000):
        raise ContractError("second-order proof kind/budget invalid")
    if (Fraction(start) < 0 or Fraction(end) < Fraction(start)
        or Fraction(end) > Fraction(ratio.fin * ratio.L, 2)):
        raise ContractError("second-order proof interval invalid")
    moment2 = second_moment(taps)
    stack = [(Fraction(start), Fraction(end))]
    visited = 0
    proven = 0
    while stack:
        if visited >= max_boxes:
            return dict(status="UNRESOLVED", visited_boxes=visited,
                        proven_boxes=proven, remaining_boxes=len(stack),
                        release_qualified=False)
        lo, hi = stack.pop()
        low, high = _enclose_second_order_validated(
            ratio, taps, lo, hi, moment2=moment2, bits=bits)
        visited += 1
        ok = ((low >= 1-PASS_TOL and high <= 1+PASS_TOL)
              if kind == "passband" else
              (low >= -STOP_TOL and high <= STOP_TOL))
        if ok:
            proven += 1
            continue
        if lo == hi:
            return dict(status="UNRESOLVED", visited_boxes=visited,
                        proven_boxes=proven, remaining_boxes=len(stack)+1,
                        release_qualified=False)
        mid = (lo+hi)/2
        stack.extend(((mid, hi), (lo, mid)))
    return dict(status="CONTINUUM_SUBBAND_PROVEN_NONRELEASE",
                visited_boxes=visited, proven_boxes=proven,
                remaining_boxes=0, release_qualified=False)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("asset", type=Path)
    p.add_argument("--fin", type=int, required=True)
    p.add_argument("--fout", type=int, required=True)
    p.add_argument("--pinned-sha256", required=True)
    p.add_argument("--lo", type=Fraction, required=True)
    p.add_argument("--hi", type=Fraction, required=True)
    p.add_argument("--kind", choices=("passband","stopband"), required=True)
    p.add_argument("--max-boxes", type=int, default=128)
    p.add_argument("--report", type=Path, required=True)
    a = p.parse_args()
    ratio = Ratio.make(a.fin, a.fout)
    if a.fin == a.fout:
        raise ContractError("identity must never load FIR")
    if a.report.exists() or a.report.resolve().is_relative_to(
            a.asset.resolve().parent):
        raise ContractError("only NEW external nonrelease reports allowed")
    if ((a.kind == "passband" and
         (a.lo < 0 or a.hi > Fraction(12*min(a.fin,a.fout), 25)))
        or (a.kind == "stopband" and
            (a.lo < Fraction(min(a.fin,a.fout),2)
             or a.hi > Fraction(a.fin*ratio.L,2)))):
        raise ContractError("outside frozen passband/stopband")
    data = a.asset.read_bytes()
    if sha256(data).hexdigest() != a.pinned_sha256:
        raise ContractError("not the independently pinned genuine Q0 asset")
    _, taps = read_locked_kernel(data, ratio, a.pinned_sha256)
    result = prove_window(
        ratio, taps, a.lo, a.hi,
        kind=a.kind, max_boxes=a.max_boxes)
    result.update(
        schema="rgsml.phase11.mpfr-2ndorder-true-continuum-subband/1",
        fin=a.fin, fout=a.fout, kind=a.kind,
        lo_hz=str(a.lo), hi_hz=str(a.hi),
        candidate_sha256=a.pinned_sha256,
        full_passband_and_stopband_proven=False, q1_q6_passed=False,
        product_admission_authorized=False, release_qualified=False)
    a.report.write_text(json.dumps(result,sort_keys=True,indent=2)+"\n")
    print(json.dumps(result,sort_keys=True))


if __name__ == "__main__":
    main()
