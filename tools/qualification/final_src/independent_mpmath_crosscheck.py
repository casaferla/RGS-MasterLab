#!/usr/bin/env python3
"""Independent HIGH-PRECISION NUMERICAL RGSMSR1 cross-check (OFFLINE ONLY).

This uses mpmath and its own Kaiser/I0 recurrence, NOT the MPFR interval
generator code. Two freshly computed precision levels are compared with one
already generated candidate. It is a diagnostic independent *algorithmic*
reproduction, not certified interval arithmetic or the frozen MPFR DTFT Q1
oracle. Never issue a release manifest from this tool.
"""
from __future__ import annotations

from hashlib import sha256
from pathlib import Path
import argparse
import json
import struct

from phase11_math_preflight import (
    HEADER, MAGIC, Ratio, audit_rgsmsr1_structure, kernel_shape,
)


def independent_bytes(fin: int, fout: int, decimal_digits: int = 140) -> bytes:
    if decimal_digits < 100:
        raise ValueError("independent screen needs at least 100 decimal digits")
    try:
        import mpmath as mp
    except ImportError as exc:
        raise RuntimeError("Install mpmath for this OFFLINE optional diagnostic") from exc
    ratio = Ratio.make(fin, fout)
    shape = kernel_shape(ratio)
    L, M, T, N, D = shape.L, shape.M, shape.T, shape.N, shape.D
    with mp.workdps(decimal_digits):
        beta = mp.mpf(778563) / 50000
        fc = mp.mpf(49) / (100 * max(L, M))
        pi = mp.pi
        threshold = mp.power(2, -300)

        def i0_recurrence(z):
            # Frozen first-true predicate evaluated at high precision,
            # with a different source implementation from MPFR intervals.
            term = mp.mpf(1)
            acc = mp.mpf(1)
            zsq = z*z/4
            for r in range(1, 320):
                term = term*zsq/(r*r)
                acc += term
                if term <= acc*threshold:
                    return acc
            raise ArithmeticError("frozen I0 recurrence failed")

        inv_i0_beta = 1 / i0_recurrence(beta)
        raw = []
        for j in range(D + 1):
            u = j-D
            t = mp.mpf(u) / D
            window = i0_recurrence(beta * mp.sqrt(1-t*t)) * inv_i0_beta
            z = 2*fc*u
            numerator, denominator = 49*u, 50*max(L, M)
            if u == 0:
                sinc = mp.mpf(1)
            elif numerator % denominator == 0:
                # Algebraic identity sin(pi*k)=0, exactly.
                sinc = mp.mpf(0)
            else:
                sinc = mp.sin(pi*z)/(pi*z)
            raw.append(2*fc*sinc*window)

        total = mp.mpf(0)
        # Frozen j-ascending normalization order even though raw is mirrored.
        for j in range(N):
            total += raw[j] if j <= D else raw[N-1-j]
        scale = mp.mpf(L)/total
        unique = [struct.pack("<d", float(v*scale)) for v in raw]
        coefficients = b"".join(unique+unique[-2::-1])
        if len(coefficients) != 8*N:
            raise ValueError("independent coefficient count mismatch")
        return HEADER.pack(MAGIC,1,1,L,M,T,0,N,N,8*N)+coefficients


def compare_candidate(data: bytes, fin: int, fout: int,
                      *, dps_low: int = 120, dps_high: int = 160) -> dict:
    ratio = Ratio.make(fin, fout)
    shape = kernel_shape(ratio)
    sha = audit_rgsmsr1_structure(data, shape)
    first = independent_bytes(fin, fout, dps_low)
    second = independent_bytes(fin, fout, dps_high)
    low_match = first == data
    high_match = second == data
    stable = first == second
    return dict(
        status="INDEPENDENT_MPMATH_PRECISION_SCREEN_NOT_RELEASE_QUALIFIED",
        fin=fin, fout=fout, ratio=f"{shape.L}/{shape.M}",
        candidate_sha256=sha,
        independent_sha256_low=sha256(first).hexdigest(),
        independent_sha256_high=sha256(second).hexdigest(),
        low_digits=dps_low, high_digits=dps_high,
        independent_precision_stable=stable,
        candidate_bytes_match_both=low_match and high_match,
        certified_interval_rounding=False,
        mpfr_dtft_response_proven=False,
        release_qualified=False,
    )


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--fin", type=int, required=True)
    p.add_argument("--fout", type=int, required=True)
    p.add_argument("--asset", type=Path, required=True)
    p.add_argument("--low-dps", type=int, default=120)
    p.add_argument("--high-dps", type=int, default=160)
    args = p.parse_args()
    result = compare_candidate(args.asset.read_bytes(), args.fin, args.fout,
                               dps_low=args.low_dps, dps_high=args.high_dps)
    print(json.dumps(result, indent=2, sort_keys=True))
    if not result["independent_precision_stable"] or not result["candidate_bytes_match_both"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
