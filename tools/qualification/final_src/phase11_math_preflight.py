#!/usr/bin/env python3
"""Phase-11 Final SRC Q0 mathematical preflight; NOT a coefficient generator.

This module is an exact-integer/rational reference for the FROZEN Phase-11
contracts (including the Final SRC Integrity Micro-Delta). It deliberately
does not substitute for the missing MPFR/Ziv coefficient-proof generator,
spectral response oracle, or Q1-Q6 release qualification.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from fractions import Fraction
from hashlib import sha256
import json
from math import gcd, isfinite
from pathlib import Path
import struct

RATES = (44100, 48000, 88200, 96000, 176400, 192000)
MIN_I128 = -(1 << 127)
MAX_I128 = (1 << 127) - 1
MIN_I64 = -(1 << 63)
MAX_I64 = (1 << 63) - 1
HEADER = struct.Struct("<8sHHIIIIQQQ")
MAGIC = b"RGSMSR1\x00"
ALGORITHM = "rgsml.final-output-src.polyphase-fir/1.0.0"
KERNEL = "rgsml.src.kernel.kaiser-sinc/1.0.0"
BOUNDARY = "rgsml.src.boundary.even-reflect/1.0.0"
FRAME_MAP = "rgsml.src.frame-map.half-open-ceil/1.0.0"


class ContractError(ValueError):
    """Fail closed rather than emit incomplete, truncated or wrapped data."""


def check_int(value: int, minimum: int, maximum: int, what: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        raise ContractError(f"{what}: not an integer")
    if not minimum <= value <= maximum:
        raise ContractError(f"{what}: integer overflow")
    return value


def ceil_div_signed(a: int, b: int) -> int:
    if not isinstance(b, int) or b <= 0:
        raise ContractError("positive divisor required")
    return -((-a) // b)


def floor_div_signed(a: int, b: int) -> int:
    if not isinstance(b, int) or b <= 0:
        raise ContractError("positive divisor required")
    return a // b


def floor_mod(a: int, b: int) -> int:
    if not isinstance(b, int) or b <= 0:
        raise ContractError("positive divisor required")
    return a - b * floor_div_signed(a, b)


def next_even(x: int) -> int:
    check_int(x, 0, MAX_I128, "nextEven operand")
    return check_int(x + (x % 2), 0, MAX_I128, "nextEven result")


def atan_bounds(unit_denominator: int, terms: int = 28) -> tuple[Fraction, Fraction]:
    """Certified alternating-series enclosure; all arithmetic is rational."""
    if unit_denominator <= 1 or terms <= 0:
        raise ContractError("atan enclosure arguments")
    x = Fraction(1, unit_denominator)
    partial = Fraction(0)
    for r in range(terms):
        partial += (-1 if r & 1 else 1) * x ** (2 * r + 1) / (2 * r + 1)
    next_term = (-1 if terms & 1 else 1) * x ** (2 * terms + 1) / (2 * terms + 1)
    return min(partial, partial + next_term), max(partial, partial + next_term)


def pi_bounds() -> tuple[Fraction, Fraction]:
    # Machin: pi = 16 atan(1/5) - 4 atan(1/239), with certified remainders.
    lo5, hi5 = atan_bounds(5)
    lo239, hi239 = atan_bounds(239)
    return 16 * lo5 - 4 * hi239, 16 * hi5 - 4 * lo239


def ceil_rational(x: Fraction) -> int:
    return ceil_div_signed(x.numerator, x.denominator)


@dataclass(frozen=True)
class Ratio:
    fin: int
    fout: int
    L: int
    M: int

    @classmethod
    def make(cls, fin: int, fout: int) -> "Ratio":
        if fin not in RATES or fout not in RATES:
            raise ContractError("UNSUPPORTED_RATE_PAIR_V1")
        divisor = gcd(fin, fout)
        return cls(fin, fout, fout // divisor, fin // divisor)


@dataclass(frozen=True)
class KernelShape:
    L: int
    M: int
    T: int
    N: int
    D: int

    @property
    def frozen_bytes(self) -> int:
        return HEADER.size + 8 * self.N


def kernel_shape(ratio: Ratio, pi: tuple[Fraction, Fraction] | None = None) -> KernelShape:
    if ratio.fin == ratio.fout:
        raise ContractError("identity has no kernel asset")
    pi_lo, pi_hi = pi if pi is not None else pi_bounds()
    if pi_lo <= 0 or pi_lo > pi_hi:
        raise ContractError("invalid pi enclosure")
    # Exact rational part of frozen orderEstimateInput:
    # 142 / ((457/200)*2*pi*(1/50)*min(1,L/M))
    # = 710000*M/(457*pi*min(L,M)).
    numerator = Fraction(710000 * ratio.M, 457 * min(ratio.L, ratio.M))
    estimate_lo = numerator / pi_hi
    estimate_hi = numerator / pi_lo
    lower_t = ceil_rational(estimate_lo)
    upper_t = ceil_rational(estimate_hi)
    if lower_t != upper_t:
        raise ContractError("kernel order not proven by pi enclosure")
    t = next_even(max(512, upper_t))
    n = check_int(ratio.L * t + 1, 1, MAX_I64, "prototype N")
    if n % 2 != 1:
        raise ContractError("prototype must have odd length")
    return KernelShape(ratio.L, ratio.M, t, n, (n - 1) // 2)


def mapped_frame(origin: int, ratio: Ratio) -> int:
    check_int(origin, MIN_I64, MAX_I64, "source origin")
    product = check_int(origin * ratio.L, MIN_I128, MAX_I128, "FrameMap product")
    return check_int(ceil_div_signed(product, ratio.M), MIN_I64, MAX_I64,
                     "output origin")


def mapped_interval(start: int, end: int, ratio: Ratio) -> tuple[int, int]:
    if end < start:
        raise ContractError("reversed half-open source interval")
    return mapped_frame(start, ratio), mapped_frame(end, ratio)


def even_reflect(n: int, source_frames: int) -> int:
    check_int(source_frames, 1, MAX_I64, "source frame count")
    if source_frames == 1:
        return 0
    period = check_int(2 * source_frames, 1, MAX_I128, "reflection period")
    phase = floor_mod(n, period)
    return phase if phase < source_frames else period - 1 - phase


def above_new_nyquist(fin: int, fout: int) -> tuple[str, Fraction]:
    ratio = Ratio.make(fin, fout)
    if ratio.fin <= ratio.fout:
        raise ContractError("alias probe requires downsampling")
    frequency = Fraction(fin + fout, 4)
    if not Fraction(fout, 2) < frequency < Fraction(fin, 2):
        raise ContractError("alias probe outside frozen Nyquist bracket")
    return (f"rgsml.fx.reg.pipeline.src.{fin}_to_{fout}.above_new_nyquist.v1",
            frequency)


def audit_rgsmsr1_structure(data: bytes, shape: KernelShape) -> str:
    """Q0 byte/shape/symmetry audit only; NOT a coefficient correctness proof.

    The caller must independently match the SHA-256 to a separately frozen
    manifest and re-generate the coefficients via certified MPFR/Ziv enclosures.
    """
    if len(data) != shape.frozen_bytes or len(data) < HEADER.size:
        raise ContractError("SRC_KERNEL_ASSET_INVALID: size")
    magic, version, encoding, L, M, T, reserved, N, count, payload = HEADER.unpack_from(data)
    if (magic != MAGIC or version != 1 or encoding != 1 or reserved != 0
        or (L, M, T, N, count, payload) !=
           (shape.L, shape.M, shape.T, shape.N, shape.N, 8 * shape.N)):
        raise ContractError("SRC_KERNEL_ASSET_INVALID: header")
    start = HEADER.size
    for j in range((shape.N + 1) // 2):
        a = start + 8 * j
        b = start + 8 * (shape.N - 1 - j)
        if data[a:a + 8] != data[b:b + 8]:
            raise ContractError("SRC_KERNEL_ASSET_INVALID: symmetry")
        if not isfinite(struct.unpack_from("<d", data, a)[0]):
            raise ContractError("SRC_KERNEL_ASSET_INVALID: nonfinite coefficient")
    return sha256(data).hexdigest()


def plan() -> dict:
    pi = pi_bounds()
    asset_shapes: dict[tuple[int, int], KernelShape] = {}
    pairs = []
    alias = []
    for fin in RATES:
        for fout in RATES:
            ratio = Ratio.make(fin, fout)
            identity = fin == fout
            shape = None if identity else kernel_shape(ratio, pi)
            if shape is not None:
                asset_shapes[(ratio.L, ratio.M)] = shape
            pairs.append({"fin": fin, "fout": fout, "L": ratio.L, "M": ratio.M,
                          "mode": "IDENTITY" if identity else "ASSET_REQUIRED",
                          "asset_ratio": None if identity else f"{ratio.L}/{ratio.M}"})
            if fin > fout:
                name, hz = above_new_nyquist(fin, fout)
                alias.append({"id": name, "frequency_numerator": hz.numerator,
                              "frequency_denominator": hz.denominator,
                              "amplitude": "1/4", "phase": "0"})
    assets = [{"L": shape.L, "M": shape.M, "T": shape.T, "N": shape.N,
               "D": shape.D, "asset_bytes": shape.frozen_bytes,
               "kernel_sha256": None, "qualification": "NOT_CERTIFIED"}
              for _, shape in sorted(asset_shapes.items())]
    if len(pairs) != 36 or len(assets) != 14 or len(alias) != 15:
        raise ContractError("frozen Phase-11 matrix count mismatch")
    return {"contract": ALGORITHM, "kernel_policy": KERNEL,
            "frame_map_policy": FRAME_MAP, "boundary_policy": BOUNDARY,
            "status": "Q0_MATH_PREFLIGHT_ONLY_NOT_RELEASE_QUALIFIED",
            "pairs": pairs, "assets": assets, "above_new_nyquist": alias,
            "total_frozen_asset_bytes": sum(x["asset_bytes"] for x in assets)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="emit deterministic planning JSON")
    args = parser.parse_args()
    result = plan()
    if args.json:
        print(json.dumps(result, sort_keys=True, indent=2))
    else:
        print("PHASE11_Q0_PREFLIGHT=PASS pairs=36 identities=6 conversions=30 "
              "distinct_ratios=14 downsample_fixtures=15")
        print(f"RGSMSR1_UNGENERATED_BYTES={result['total_frozen_asset_bytes']}")
        print("SRC_KERNEL_MPFR_ZIV=NOT_EXECUTED RELEASE_QUALIFIED=FALSE")


if __name__ == "__main__":
    main()
