#!/usr/bin/env python3
"""Fast Q0 comparison of strict-scalar n-ascending and exact-dyadic j-ascending.

These are synthetic coefficient fixtures and NOT certified real FIR responses.
"""
from fractions import Fraction
import math
import struct
import unittest

from phase11_math_preflight import (
    ContractError, Ratio, ceil_div_signed, even_reflect,
    floor_div_signed, kernel_shape, mapped_interval,
)
from exact_dyadic_zero_stuff_oracle import zero_stuffed_exact_at
from strict_scalar_polyphase_reference import strict_scalar_at, strict_scalar_crop


def small_symmetric_fixture(ratio: Ratio) -> tuple[float, ...]:
    shape = kernel_shape(ratio)
    taps = [0.0] * shape.N
    # Binary64 values include inexact decimals to test separate FP ops.
    for offset, value in {0: 0.3, 1: -0.1, ratio.L: 0.125}.items():
        for j in {shape.D - offset, shape.D + offset}:
            taps[j] = value
    return tuple(taps)


class PolyphaseStrictScalarReferenceTests(unittest.TestCase):
    def test_n_ascending_convolution_matches_independent_exact_j_grid(self):
        # Including L=1, M=2 and L=160, M=147 fractional-phase paths.
        for fin, fout in ((48000, 96000), (96000, 48000),
                          (44100, 48000)):
            with self.subTest(fin=fin, fout=fout):
                ratio = Ratio.make(fin, fout)
                taps = small_symmetric_fixture(ratio)
                exact_taps = tuple(Fraction.from_float(x) for x in taps)
                source = (0.25, -0.5, 0.75, -0.125, 0.5, 0.0)
                whole = strict_scalar_crop(source, ratio, taps)
                self.assertEqual(len(whole),
                                 ceil_div_signed(len(source)*ratio.L, ratio.M))
                eps = Fraction(1, 1 << 53)
                for m, measured in enumerate(whole):
                    expected = zero_stuffed_exact_at(
                        source, ratio, exact_taps, m)
                    shape = kernel_shape(ratio)
                    q = shape.D + m*ratio.M
                    first = ceil_div_signed(q-shape.N+1, ratio.L)
                    last = floor_div_signed(q, ratio.L)
                    terms = [
                        Fraction.from_float(source[even_reflect(n,len(source))])
                        * exact_taps[q-n*ratio.L]
                        for n in range(first,last+1)
                    ]
                    # Standard no-underflow sum-of-products forward bound:
                    # gamma_(2k) * sum(abs(exact products)), k terms.
                    k = len(terms)
                    self.assertLess(2*k*eps, 1)
                    gamma = (2*k*eps)/(1-2*k*eps)
                    error = abs(Fraction.from_float(measured)-expected)
                    self.assertLessEqual(error, gamma*sum(map(abs,terms)))
                start,end = mapped_interval(1,4,ratio)
                self.assertEqual(strict_scalar_crop(source,ratio,taps,(1,4)),
                                 whole[start:end])

    def test_identity_is_bit_transparent_and_unfiltered(self):
        ratio = Ratio.make(48000,48000)
        source = (-0.0, float.fromhex("0x0.0000000000001p-1022"),
                  -0.375, 0.0)
        result = strict_scalar_crop(source,ratio,())
        self.assertEqual(tuple(struct.pack("<d",x) for x in result),
                         tuple(struct.pack("<d",x) for x in source))
        self.assertEqual(struct.pack("<d",strict_scalar_at(source,ratio,(),0)),
                         struct.pack("<d",-0.0))
        self.assertEqual(strict_scalar_crop(source,ratio,(),(1,3)),
                         source[1:3])

    def test_single_source_frame_uses_duplicated_endpoint_reflection(self):
        ratio = Ratio.make(48000,96000)
        taps = small_symmetric_fixture(ratio)
        source = (0.25,)
        result = strict_scalar_crop(source,ratio,taps)
        self.assertEqual(len(result),2)
        exact = tuple(Fraction.from_float(x) for x in taps)
        for m, value in enumerate(result):
            reference = zero_stuffed_exact_at(source,ratio,exact,m)
            self.assertAlmostEqual(value,float(reference),delta=2e-15)

    def test_invalid_geometry_and_nonfinite_rejected(self):
        ratio = Ratio.make(48000,96000)
        taps = small_symmetric_fixture(ratio)
        src = (0.25,0.5)
        with self.assertRaises(ContractError):
            strict_scalar_crop((),ratio,taps)
        with self.assertRaises(ContractError):
            strict_scalar_crop((math.nan,),ratio,taps)
        with self.assertRaises(ContractError):
            strict_scalar_crop(src,ratio,(0.1,))
        with self.assertRaises(ContractError):
            strict_scalar_crop(src,ratio,taps,(2,1))
        with self.assertRaises(ContractError):
            strict_scalar_at(src,ratio,taps,10_000)
        identity = Ratio.make(48000,48000)
        with self.assertRaises(ContractError):
            strict_scalar_crop(src,identity,taps)


if __name__ == "__main__":
    unittest.main(verbosity=2)
