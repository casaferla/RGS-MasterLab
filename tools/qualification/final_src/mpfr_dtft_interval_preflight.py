#!/usr/bin/env python3
"""Conservative continuous-frequency MPFR DTFT interval preflight (NON-RELEASE).

Evaluates independently loaded, exact binary64 FIR taps (not generator math).
A proven box covers EVERY frequency in that box; an unresolved box never
qualifies anything. Conservative rational acceptance bounds are sufficient,
not necessary, for the frozen 0.001 dB peak-to-peak passband and -140 dB
stopband requirements. Q1-Q6 and product admission remain separate gates.
"""
from __future__ import annotations

from fractions import Fraction
from hashlib import sha256

from exact_dyadic_zero_stuff_oracle import read_locked_kernel
from generate_rgsmsr1_mpfr import Engine, RNDN, RNDD, RNDU
from phase11_math_preflight import ContractError, Ratio, pi_bounds

# |H-1| <= 1/30000 implies <0.00058 dB total passband ripple.
# |H| <= 1/10^7 implies at least 140 dB stopband attenuation.
PASS_TOL = Fraction(1, 30000)
STOP_TOL = Fraction(1, 10_000_000)


def _mpfr_dyadic(e: Engine, x: Fraction):
    """Exact binary64 dyadic loading, including tiny subnormal coefficients."""
    x = Fraction(x)
    d = x.denominator
    if d & (d - 1) or abs(x.numerator) >= (1 << 62):
        raise ContractError("DTFT requires bounded exact binary64 dyadic tap")
    value = e.fraction(Fraction(x.numerator))
    return value.shift2(-(d.bit_length() - 1))


def enclose_dtft_box(ratio: Ratio, taps: tuple[Fraction, ...],
                     lo_hz: Fraction, hi_hz: Fraction,
                     *, bits: int = 256) -> tuple[Fraction, Fraction]:
    """Rigorous interval for REAL zero-phase H(f), for all f in [lo,hi].

    Symmetry removes the linear-phase factor. Center value is enclosed using
    MPFR directed intervals and exact binary64 tap values. A rigorous global
    first-derivative (Lipschitz) bound encloses the entire frequency box.
    It is deliberately conservative and may need many subdivisions.
    """
    lo_hz, hi_hz = Fraction(lo_hz), Fraction(hi_hz)
    if (lo_hz < 0 or hi_hz < lo_hz
        or hi_hz > Fraction(ratio.fin * ratio.L, 2)):
        raise ContractError("DTFT frequency box outside high-grid Nyquist")
    if len(taps) < 3 or len(taps) % 2 == 0 or tuple(taps) != tuple(reversed(taps)):
        raise ContractError("DTFT requires a finite symmetric odd-length FIR")
    if bits < 256 or bits > 4096:
        raise ContractError("DTFT precision budget outside allowed range")
    D = (len(taps) - 1) // 2
    center = (lo_hz + hi_hz) / 2
    radius = (hi_hz - lo_hz) / 2
    e = Engine(bits)
    two = e.fraction(Fraction(2))
    pi = e.pi()
    total = _mpfr_dyadic(e, taps[D])
    moment = Fraction(0)
    for u in range(1, D + 1):
        tap = taps[D + u]
        if not tap:
            continue
        moment += u * abs(tap)
        # Frequency is rational. Remove exact full cycles BEFORE multiplying
        # by MPFR pi, avoiding giant unreduced trigonometric arguments.
        cycles = (center * u / (ratio.fin * ratio.L)) % 1
        if cycles > Fraction(1, 2):
            cycles -= 1
        phase = pi * e.fraction(2 * cycles)
        cosine = (phase + pi / two).sin()
        total = total + two * _mpfr_dyadic(e, tap) * cosine
    response = total / e.fraction(Fraction(ratio.L))
    # mpfr_get_d with directed rounding encloses each MPFR endpoint as an
    # EXACT IEEE dyadic; conversion to Fraction therefore loses no information.
    low = Fraction.from_float(e.fn["get_d"](response.lo.ref, RNDD))
    high = Fraction.from_float(e.fn["get_d"](response.hi.ref, RNDU))
    pi_upper = pi_bounds()[1]
    max_delta = Fraction(4) * pi_upper * moment * radius / (
        ratio.fin * ratio.L * ratio.L)
    return low - max_delta, high + max_delta


def prove_band(ratio: Ratio, taps: tuple[Fraction, ...],
               start_hz: Fraction, end_hz: Fraction, *,
               kind: str, max_boxes: int = 32,
               bits: int = 256) -> dict:
    """Attempt full continuum coverage. NEVER issue PASS for incomplete boxes."""
    if kind not in ("passband", "stopband") or not 1 <= max_boxes <= 1_000_000:
        raise ContractError("DTFT proof kind or explicit box budget invalid")
    stack = [(Fraction(start_hz), Fraction(end_hz))]
    visited = 0
    while stack:
        if visited == max_boxes:
            return dict(status="UNRESOLVED", visited_boxes=visited,
                        remaining_boxes=len(stack),
                        release_qualified=False)
        lo, hi = stack.pop()
        low, high = enclose_dtft_box(ratio, taps, lo, hi, bits=bits)
        visited += 1
        proven = ((low >= 1 - PASS_TOL and high <= 1 + PASS_TOL)
                  if kind == "passband" else
                  (low >= -STOP_TOL and high <= STOP_TOL))
        if proven:
            continue
        if lo == hi:
            return dict(status="UNRESOLVED", visited_boxes=visited,
                        remaining_boxes=len(stack) + 1,
                        release_qualified=False)
        midpoint = (lo + hi) / 2
        stack.append((midpoint, hi))
        stack.append((lo, midpoint))
    return dict(status="CONTINUUM_BOUND_PROVEN_NONRELEASE",
                visited_boxes=visited, remaining_boxes=0,
                release_qualified=False)


def screen_candidate_asset(data: bytes, fin: int, fout: int,
                           expected_sha256: str, *, max_boxes: int = 32) -> dict:
    """Offline per-asset continuum proof ATTEMPT with required explicit hash.

    A self-derived digest is not an independent kernel release lockfile.
    This stage cannot promote product support, even if its bounds close.
    """
    ratio = Ratio.make(fin, fout)
    if fin == fout:
        raise ContractError("identity path must not load FIR candidate")
    shape, taps = read_locked_kernel(data, ratio, expected_sha256)
    common = min(fin, fout)
    passband = prove_band(ratio, taps, Fraction(0),
                          Fraction(12 * common, 25),
                          kind="passband", max_boxes=max_boxes)
    stopband = prove_band(
        ratio, taps, Fraction(common, 2),
        Fraction(fin * ratio.L, 2), kind="stopband",
        max_boxes=max_boxes)
    return dict(
        status="MPFR_DTFT_INTERVAL_ATTEMPT_NOT_RELEASE_QUALIFIED",
        fin=fin, fout=fout, L=shape.L, M=shape.M,
        candidate_sha256=sha256(data).hexdigest(),
        passband=passband, stopband=stopband,
        continuous_bounds_closed=(
            passband["status"] == "CONTINUUM_BOUND_PROVEN_NONRELEASE" and
            stopband["status"] == "CONTINUUM_BOUND_PROVEN_NONRELEASE"),
        independent_coefficient_generator_verified=False,
        q1_q6_passed=False, product_admission_authorized=False,
        release_qualified=False)
