#!/usr/bin/env python3
"""Independent exact-dyadic ZERO-STUFFED convolution cross-check (supplementary).

NOT the frozen SRC_FULL_CONVOLUTION_MPFR_ORACLE/1.0.0 replacement. This is
an exact rational cross-check over already-rounded IEEE binary64 asset taps
and input samples. The production polyphase evaluator is NEVER called.
An external, independently frozen expected SHA-256 is mandatory.
"""
from __future__ import annotations
from fractions import Fraction
from hashlib import sha256
from pathlib import Path
import struct

from phase11_math_preflight import (
    ContractError, Ratio, audit_rgsmsr1_structure, ceil_div_signed,
    even_reflect, kernel_shape, mapped_interval,
)

def read_locked_kernel(asset_bytes: bytes, ratio: Ratio, expected_sha256: str):
    if ratio.fin == ratio.fout:
        raise ContractError("identity SRC must not load a kernel")
    if (not isinstance(expected_sha256, str)
        or len(expected_sha256) != 64
        or any(c not in "0123456789abcdef" for c in expected_sha256)):
        raise ContractError("canonical 64-hex lockfile SHA required")
    shape = kernel_shape(ratio)
    actual = audit_rgsmsr1_structure(asset_bytes, shape)
    if actual != expected_sha256:
        raise ContractError("SRC_KERNEL_ASSET_INVALID: independent SHA mismatch")
    # Materialize each exact IEEE binary64 coefficient as an exact dyadic,
    # NOT an approximate Python fsum or a production C++ accumulator.
    return shape, tuple(
        Fraction.from_float(struct.unpack_from(
            "<d", asset_bytes, 52 + 8 * j)[0]) for j in range(shape.N))


def zero_stuffed_exact_at(
    source_bits: tuple[float, ...],
    ratio: Ratio,
    kernel: tuple[Fraction, ...],
    m: int,
) -> Fraction:
    """Conceptual high-rate grid, independent of production n-ascending phase code.

    Iterate prototype tap j in ascending order: upsample grid coordinate
    q-j is a real SOURCE frame iff divisible by L; all other high-rate
    input samples are mathematically zero. Fraction arithmetic is exact.
    """
    if not source_bits or not 0 <= m < ceil_div_signed(len(source_bits)*ratio.L,ratio.M):
        raise ContractError("nonempty full-track source and valid output index required")
    shape = kernel_shape(ratio)
    if len(kernel) != shape.N:
        raise ContractError("kernel length mismatch")
    q = shape.D + m * ratio.M
    exact_input = tuple(Fraction.from_float(float(x)) for x in source_bits)
    acc = Fraction(0)
    # An upsampled high-rate grid is zero except at indices divisible by L.
    # The first prototype tap with (q - j) % L == 0 is q % L.
    # Iterating *j in ascending order* preserves this independent conceptual
    # oracle and its exact Fraction sum, while skipping the provable zeros.
    # This is NOT the production n-ascending binary64 convolution.
    for j in range(q % ratio.L, len(kernel), ratio.L):
        src_index = (q - j) // ratio.L
        index = even_reflect(src_index, len(exact_input))
        acc += exact_input[index] * kernel[j]
    return acc


def zero_stuffed_crop(
    source_bits: tuple[float, ...], ratio: Ratio,
    kernel: tuple[Fraction, ...],
    source_interval: tuple[int, int] | None = None,
) -> tuple[Fraction, ...]:
    """Absolute output indices; crop never restarts a phase accumulator."""
    if not source_bits:
        return ()
    source_start, source_end = source_interval or (0, len(source_bits))
    if not 0 <= source_start <= source_end <= len(source_bits):
        raise ContractError("absolute source crop outside original resource")
    begin, end = mapped_interval(source_start, source_end, ratio)
    return tuple(zero_stuffed_exact_at(source_bits, ratio, kernel, m)
                 for m in range(begin, end))


def assert_exact_crop_invariance(
    source_bits: tuple[float, ...], ratio: Ratio,
    kernel: tuple[Fraction, ...], bounds: tuple[int, int],
) -> None:
    entire = zero_stuffed_crop(source_bits, ratio, kernel)
    begin, end = mapped_interval(bounds[0], bounds[1], ratio)
    crop = zero_stuffed_crop(source_bits, ratio, kernel, bounds)
    if entire[begin:end] != crop:
        raise ContractError("EXACT_ZERO_STUFFED_CROP_INVARIANCE_FAILED")


def screen_real_candidate_dyadic(asset_bytes: bytes, fin: int, fout: int) -> dict:
    """Full REAL candidate binary64 asset: exact finite-track Q0 smoke only.

    Self-derived SHA pins bytes *within this invocation*, not a frozen
    independently approved KernelManifest. Mathematical crop equivalence
    is exact; it does NOT establish coefficients, DTFT or Q1-Q6 response.
    """
    ratio = Ratio.make(fin, fout)
    if fin == fout:
        raise ContractError("identity path must remain bit-transparent")
    digest = sha256(asset_bytes).hexdigest()
    shape, kernel = read_locked_kernel(asset_bytes, ratio, digest)
    # Nonzero, signed, endpoint-sensitive short track; crop must use absolute
    # coordinates from the WHOLE source and preserve duplicated-end reflection.
    source = (0.25, -0.5, 0.125, 0.0, 0.5, -0.25, 0.375, 0.0)
    full = zero_stuffed_crop(source, ratio, kernel)
    expected_length = ceil_div_signed(len(source) * ratio.L, ratio.M)
    if len(full) != expected_length:
        raise ContractError("EXACT_DYADIC_OUTPUT_LENGTH_MISMATCH")
    assert_exact_crop_invariance(source, ratio, kernel, (1, 7))
    # Report DC response on actual rounded taps, but do NOT replace the
    # frozen continuous spectral response criterion with a sample threshold.
    dc = zero_stuffed_crop((0.25,) * len(source), ratio, kernel)
    reference_dc = Fraction(1, 4)
    max_dc_error = max((abs(x - reference_dc) for x in dc),
                       default=Fraction(0))
    return dict(
        status="REAL_BINARY64_EXACT_DYADIC_CROP_SCREEN_NOT_RELEASE_QUALIFIED",
        fin=fin, fout=fout, L=shape.L, M=shape.M, taps=shape.N,
        candidate_sha256=digest, checked_output_frames=expected_length,
        exact_dyadic_crop_invariant=True,
        max_abs_dc_error_screen=float(max_dc_error),
        certified_mpfr_full_convolution_oracle=False,
        mpfr_dtft_response_proven=False,
        release_qualified=False,
    )
