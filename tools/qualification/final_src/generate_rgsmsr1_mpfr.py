#!/usr/bin/env python3
"""Offline Phase-11 MPFR interval/Ziv RGSMSR1 generator (never product runtime).

Needs a locally installed MPFR 4.x shared library. Fails closed if ANY
coefficient or the frozen I0 stopping predicate cannot be enclosed. This
is a qualification CANDIDATE until independently reviewed, reproduced,
and passed through the spectral/convolution/Q0-Q6 oracles.
"""
from __future__ import annotations

import argparse
import ctypes as ct
from ctypes.util import find_library
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import struct

from phase11_math_preflight import (
    HEADER, MAGIC, ContractError, Ratio, audit_rgsmsr1_structure,
    kernel_shape,
)

RNDN, RNDU, RNDD = 0, 2, 3


class ProofUnresolved(RuntimeError):
    pass


class MPFRStruct(ct.Structure):
    _fields_ = [
        ("prec", ct.c_long), ("sign", ct.c_int),
        ("exp", ct.c_long), ("limbs", ct.POINTER(ct.c_ulong)),
    ]


class Engine:
    def __init__(self, bits: int):
        self.bits = bits
        libname = find_library("mpfr")
        if not libname:
            raise ContractError("MPFR_NOT_INSTALLED: no coefficient fallback")
        self.lib = ct.CDLL(libname)
        ptr = ct.POINTER(MPFRStruct)
        specs = {
            "init2": (None, (ptr, ct.c_long)),
            "clear": (None, (ptr,)),
            "set_si": (ct.c_int, (ptr, ct.c_long, ct.c_int)),
            "set": (ct.c_int, (ptr, ptr, ct.c_int)),
            "add": (ct.c_int, (ptr, ptr, ptr, ct.c_int)),
            "sub": (ct.c_int, (ptr, ptr, ptr, ct.c_int)),
            "mul": (ct.c_int, (ptr, ptr, ptr, ct.c_int)),
            "div": (ct.c_int, (ptr, ptr, ptr, ct.c_int)),
            "div_ui": (ct.c_int, (ptr, ptr, ct.c_ulong, ct.c_int)),
            "mul_2si": (ct.c_int, (ptr, ptr, ct.c_long, ct.c_int)),
            "sqrt": (ct.c_int, (ptr, ptr, ct.c_int)),
            "sin": (ct.c_int, (ptr, ptr, ct.c_int)),
            "const_pi": (ct.c_int, (ptr, ct.c_int)),
            "cmp": (ct.c_int, (ptr, ptr)),
            "get_d": (ct.c_double, (ptr, ct.c_int)),
        }
        self.fn = {}
        for short, (result, arguments) in specs.items():
            f = getattr(self.lib, "mpfr_" + short)
            f.restype = result
            f.argtypes = list(arguments)
            self.fn[short] = f

    class Number:
        def __init__(self, engine: "Engine"):
            self.engine = engine
            self.value = MPFRStruct()
            engine.fn["init2"](ct.byref(self.value), engine.bits)

        def __del__(self):
            if getattr(self, "value", None) is not None:
                self.engine.fn["clear"](ct.byref(self.value))
                self.value = None

        @property
        def ref(self):
            return ct.byref(self.value)

    def number(self):
        return Engine.Number(self)

    def op(self, name, a, b=None, mode=RNDN):
        target = self.number()
        if b is None:
            self.fn[name](target.ref, a.ref, mode)
        else:
            self.fn[name](target.ref, a.ref, b.ref, mode)
        return target

    def setint(self, v: int, mode: int):
        if not -(1 << 62) <= v < (1 << 62):
            raise ContractError("MPFR interval numerator exceeds supported 64-bit range")
        result = self.number()
        self.fn["set_si"](result.ref, v, mode)
        return result

    def fraction(self, x: Fraction) -> "Interval":
        x = Fraction(x)
        if x.denominator >= (1 << 62):
            raise ContractError("MPFR rational denominator too large")
        lo = self.setint(x.numerator, RNDD)
        hi = self.setint(x.numerator, RNDU)
        self.fn["div_ui"](lo.ref, lo.ref, x.denominator, RNDD)
        self.fn["div_ui"](hi.ref, hi.ref, x.denominator, RNDU)
        return Interval(self, lo, hi)

    def pi(self) -> "Interval":
        lo, hi = self.number(), self.number()
        self.fn["const_pi"](lo.ref, RNDD)
        self.fn["const_pi"](hi.ref, RNDU)
        return Interval(self, lo, hi)


class Interval:
    def __init__(self, engine: Engine, lo: Engine.Number, hi: Engine.Number):
        self.e = engine
        self.lo, self.hi = lo, hi
        if engine.fn["cmp"](lo.ref, hi.ref) > 0:
            raise ContractError("invalid interval enclosure")

    def cmp(self, other: "Interval") -> tuple[int, int]:
        f = self.e.fn["cmp"]
        return f(self.lo.ref, other.hi.ref), f(self.hi.ref, other.lo.ref)

    def __add__(self, other: "Interval") -> "Interval":
        f = self.e
        return Interval(f, f.op("add", self.lo, other.lo, RNDD),
                        f.op("add", self.hi, other.hi, RNDU))

    def __sub__(self, other: "Interval") -> "Interval":
        f = self.e
        return Interval(f, f.op("sub", self.lo, other.hi, RNDD),
                        f.op("sub", self.hi, other.lo, RNDU))

    def _four(self, other: "Interval", operation: str) -> "Interval":
        f = self.e
        zero = f.fraction(Fraction(0))
        if operation == "div":
            # Divisor interval must exclude exactly zero.
            if other.cmp(zero)[0] <= 0 and other.cmp(zero)[1] >= 0:
                raise ProofUnresolved("interval division contains zero")
        pairs = [(a, b) for a in (self.lo, self.hi)
                 for b in (other.lo, other.hi)]
        lows = [f.op(operation, a, b, RNDD) for a, b in pairs]
        highs = [f.op(operation, a, b, RNDU) for a, b in pairs]
        cmp = f.fn["cmp"]
        low = lows[0]
        high = highs[0]
        for x in lows[1:]:
            if cmp(x.ref, low.ref) < 0:
                low = x
        for x in highs[1:]:
            if cmp(x.ref, high.ref) > 0:
                high = x
        return Interval(f, low, high)

    def __mul__(self, other: "Interval") -> "Interval":
        return self._four(other, "mul")

    def __truediv__(self, other: "Interval") -> "Interval":
        return self._four(other, "div")

    def sqrt(self) -> "Interval":
        zero = self.e.fraction(Fraction(0))
        if self.e.fn["cmp"](self.lo.ref, zero.lo.ref) < 0:
            raise ProofUnresolved("sqrt input interval crosses negative")
        return Interval(self.e, self.e.op("sqrt", self.lo, mode=RNDD),
                        self.e.op("sqrt", self.hi, mode=RNDU))

    def sin(self) -> "Interval":
        # Rigorous for ANY phase interval without relying on monotonicity:
        # sin is globally 1-Lipschitz, so sin([a,b]) belongs to
        # [round_down(sin(a))-(b-a), round_up(sin(a))+(b-a)].
        e = self.e
        width_hi = e.op("sub", self.hi, self.lo, RNDU)
        low_sin = e.op("sin", self.lo, mode=RNDD)
        high_sin = e.op("sin", self.lo, mode=RNDU)
        return Interval(e, e.op("sub", low_sin, width_hi, RNDD),
                        e.op("add", high_sin, width_hi, RNDU))

    def shift2(self, amount: int) -> "Interval":
        f = self.e.fn["mul_2si"]
        lo, hi = self.e.number(), self.e.number()
        f(lo.ref, self.lo.ref, amount, RNDD)
        f(hi.ref, self.hi.ref, amount, RNDU)
        return Interval(self.e, lo, hi)

    def double_rounding_cell(self) -> bytes:
        # Outward enclosures + monotonic correctly rounded mpfr_get_d:
        # equal RN-even endpoint results prove the entire real enclosure
        # belongs to one binary64 rounding cell (including signed-zero bits).
        get = self.e.fn["get_d"]
        a = struct.pack("<d", get(self.lo.ref, RNDN))
        b = struct.pack("<d", get(self.hi.ref, RNDN))
        if a != b:
            raise ProofUnresolved("coefficient spans binary64 rounding boundary")
        return a


def i0_interval(x: Interval) -> Interval:
    e = x.e
    one = e.fraction(Fraction(1))
    four = e.fraction(Fraction(4))
    y = x * x / four
    total = one
    term = one
    for r in range(1, 320):
        term = term * y / e.fraction(Fraction(r * r))
        total = total + term
        threshold = total.shift2(-300)
        cmp = e.fn["cmp"]
        # Frozen stop predicate: include term(r), stop at its FIRST true
        # value, certified independently of rounded midpoint comparisons.
        if cmp(term.hi.ref, threshold.lo.ref) <= 0:
            next_ratio = y / e.fraction(Fraction((r + 1) ** 2))
            if cmp(next_ratio.hi.ref, one.lo.ref) >= 0:
                raise ProofUnresolved("I0 tail ratio not bounded below one")
            tail = term * next_ratio / (one - next_ratio)
            return Interval(e, total.lo,
                            e.op("add", total.hi, tail.hi, RNDU))
        if cmp(term.lo.ref, threshold.hi.ref) <= 0:
            raise ProofUnresolved("I0 first-stop predicate indeterminate")
    raise ProofUnresolved("I0 proof did not reach frozen termination")


def sinc_ratio_interval(e: Engine, pi: Interval, u: int,
                        L: int, M: int) -> Interval:
    """Frozen sinc with mathematically exact nonzero integer-argument zeros.

    2*fc*u = 49*u / (50*max(L,M)) is a rational, not a rounded phase.
    sin(pi*k) is EXACTLY zero for integer k. Without this special case
    a directed MPFR sine enclosure can straddle zero at every precision;
    a spurious tiny signed coefficient must never be emitted instead.
    This is algebraically the frozen sinc formula, not a filter redesign.
    """
    if u == 0:
        return e.fraction(Fraction(1))
    z = Fraction(49 * u, 50 * max(L, M))
    if z.denominator == 1:
        return e.fraction(Fraction(0))
    phase = pi * e.fraction(z)
    return phase.sin() / phase


def coefficient_bytes_at_precision(L: int, M: int, T: int,
                                   bits: int) -> bytes:
    e = Engine(bits)
    N, D = L * T + 1, L * T // 2
    beta = e.fraction(Fraction(778563, 50000))
    fc = e.fraction(Fraction(49, 100 * max(L, M)))
    pi = e.pi()
    one, two = e.fraction(Fraction(1)), e.fraction(Fraction(2))
    denominator = i0_interval(beta)
    raw = []
    for j in range(D + 1):
        u = j - D
        normalized = e.fraction(Fraction(u, D))
        inside = one - normalized * normalized
        window = i0_interval(beta * inside.sqrt()) / denominator
        sinc = sinc_ratio_interval(e, pi, u, L, M)
        raw.append((two * fc) * sinc * window)
    # Frozen normalization: add in STRICTLY ascending prototype j order.
    acc = e.fraction(Fraction(0))
    for j in range(N):
        acc = acc + raw[j if j <= D else N - 1 - j]
    scale = e.fraction(Fraction(L)) / acc
    unique = [(item * scale).double_rounding_cell() for item in raw]
    payload = b"".join(unique + unique[-2::-1])
    if len(payload) != 8 * N:
        raise ContractError("MPFR coefficient count mismatch")
    header = HEADER.pack(MAGIC, 1, 1, L, M, T, 0, N, N, len(payload))
    return header + payload


def generate(fin: int, fout: int, max_precision: int = 1024) -> tuple[bytes, int]:
    ratio = Ratio.make(fin, fout)
    shape = kernel_shape(ratio)
    precision = 256
    while precision <= max_precision:
        try:
            data = coefficient_bytes_at_precision(
                shape.L, shape.M, shape.T, precision)
            audit_rgsmsr1_structure(data, shape)
            return data, precision
        except ProofUnresolved:
            precision *= 2
    raise ContractError("ZIV_PROOF_NOT_CLOSED: no provisional bytes issued")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--fin", type=int, required=True)
    p.add_argument("--fout", type=int, required=True)
    p.add_argument("--max-precision", type=int, default=1024)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    if a.max_precision < 256:
        raise ContractError("proof budget below frozen 256-bit initial precision")
    data, bits = generate(a.fin, a.fout, a.max_precision)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    # No partial output on proof failure; one atomic final asset write.
    temporary = a.output.with_name(a.output.name + ".unpublished.tmp")
    try:
        temporary.write_bytes(data)
        temporary.replace(a.output)
    finally:
        temporary.unlink(missing_ok=True)
    ratio = Ratio.make(a.fin, a.fout)
    print(json.dumps({
        "status": "OFFLINE_MPFR_GENERATOR_CANDIDATE_NOT_Q1_Q6_QUALIFIED",
        "L": ratio.L, "M": ratio.M, "precision_bits": bits,
        "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
        "source": str(a.output),
    }, sort_keys=True))


if __name__ == "__main__":
    main()
