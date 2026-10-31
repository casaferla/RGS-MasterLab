#!/usr/bin/env python3
"""Offline Phase-11 Final SRC strict-scalar polyphase reference (NON-PRODUCT).

This is an independently indexed, n-ascending binary64 evaluation of the
approved convolution law. Every Python float multiplication and subsequent
addition is a separate operation; there is no fused multiply-add. It is a
crosscheck for a FUTURE C++ strict-scalar implementation, not a replacement
for the required independent MPFR full-convolution oracle, certified DTFT,
frozen kernel manifest, product release or Q1-Q6 conformance gates.
"""
from __future__ import annotations

from math import isfinite

from phase11_math_preflight import (
    ContractError, Ratio, ceil_div_signed, even_reflect,
    floor_div_signed, kernel_shape, mapped_interval,
)


def _source_checked(source: tuple[float, ...]) -> None:
    if not source:
        raise ContractError("full source PCM must be nonempty")
    if any(not isfinite(x) for x in source):
        raise ContractError("nonfinite source PCM")


def _kernel_checked(ratio: Ratio, binary64_taps: tuple[float, ...]) -> None:
    if ratio.fin == ratio.fout:
        if binary64_taps:
            raise ContractError("identity must be transparent, not filtered")
        return
    if len(binary64_taps) != kernel_shape(ratio).N:
        raise ContractError("RGSMSR1 exact prototype tap count required")
    if any(not isfinite(x) for x in binary64_taps):
        raise ContractError("nonfinite prototype tap")


def _at_checked(source: tuple[float, ...], ratio: Ratio,
                binary64_taps: tuple[float, ...], m: int,
                shape) -> float:
    """One output after whole-track/kernel validation by the caller."""
    q = shape.D + m * ratio.M
    first = ceil_div_signed(q - (shape.N - 1), ratio.L)
    last = floor_div_signed(q, ratio.L)
    acc = 0.0
    for n in range(first, last + 1):
        j = q - n * ratio.L
        sample = source[even_reflect(n, len(source))]
        product = sample * binary64_taps[j]  # rounded binary64 product
        acc = acc + product                 # separate binary64 addition
    if not isfinite(acc):
        raise ContractError("nonfinite strict scalar SRC output")
    return acc


def strict_scalar_at(source: tuple[float, ...], ratio: Ratio,
                     binary64_taps: tuple[float, ...], m: int) -> float:
    """Canonical q, nLo/nHi, signed n-ascending strict scalar law.

    The input is the entire original source; reflection never uses an
    isolated crop. Coefficients are already rounded RGSMSR1 binary64 taps.
    """
    _source_checked(source)
    _kernel_checked(ratio, binary64_taps)
    count = ceil_div_signed(len(source) * ratio.L, ratio.M)
    if not 0 <= m < count:
        raise ContractError("output frame outside full source")
    if ratio.fin == ratio.fout:
        return source[m]  # raw identity: preserves signed zero/subnormals.
    return _at_checked(source, ratio, binary64_taps, m, kernel_shape(ratio))


def strict_scalar_crop(source: tuple[float, ...], ratio: Ratio,
                       binary64_taps: tuple[float, ...],
                       source_interval: tuple[int, int] | None = None
                       ) -> tuple[float, ...]:
    """Absolute half-open frame-map and whole-resource even reflection."""
    _source_checked(source)
    _kernel_checked(ratio, binary64_taps)
    start, end = source_interval if source_interval is not None else (
        0, len(source))
    if not 0 <= start <= end <= len(source):
        raise ContractError("invalid source crop")
    begin, finish = mapped_interval(start, end, ratio)
    if ratio.fin == ratio.fout:
        # Identity bypass is byte-transparent even for -0.0/subnormals.
        return source[begin:finish]
    shape = kernel_shape(ratio)  # validate/resolve once per full crop
    return tuple(_at_checked(source, ratio, binary64_taps, m, shape)
                 for m in range(begin, finish))
