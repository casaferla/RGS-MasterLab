#!/usr/bin/env python3
"""Supplemental non-production exact-dyadic zero-stuffed oracle tests."""
import hashlib
from fractions import Fraction
import struct
import unittest

from phase11_math_preflight import HEADER, MAGIC, ContractError, Ratio, kernel_shape
from exact_dyadic_zero_stuff_oracle import (
    read_locked_kernel, zero_stuffed_crop, assert_exact_crop_invariance,
)


def structural_delta_fixture(ratio: Ratio, gain: float):
    """Synthetic fixture ONLY; never a frozen Kaiser SRC kernel."""
    shape = kernel_shape(ratio)
    taps = bytearray(8 * shape.N)
    struct.pack_into("<d", taps, shape.D * 8, gain)
    head = HEADER.pack(MAGIC, 1, 1, shape.L, shape.M, shape.T,
                       0, shape.N, shape.N, 8 * shape.N)
    data = head + taps
    return data, hashlib.sha256(data).hexdigest()


class ExactDyadicSupplementalTests(unittest.TestCase):
    def test_upsample_zero_stuffed_impulse_and_canonical_crop(self):
        ratio = Ratio.make(48000, 96000)
        data, digest = structural_delta_fixture(ratio, 2.0)
        shape, kernel = read_locked_kernel(data, ratio, digest)
        self.assertEqual(len(kernel), shape.N)
        source = (1.0, 2.0, 3.0)
        full = zero_stuffed_crop(source, ratio, kernel)
        self.assertEqual(full, tuple(Fraction(x) for x in (2, 0, 4, 0, 6, 0)))
        assert_exact_crop_invariance(source, ratio, kernel, (1, 2))
        self.assertEqual(zero_stuffed_crop(source, ratio, kernel, (1, 2)),
                         (Fraction(4), Fraction(0)))

    def test_downsample_and_duplicated_endpoint_reflection(self):
        ratio = Ratio.make(96000, 48000)
        data, digest = structural_delta_fixture(ratio, 1.0)
        _, kernel = read_locked_kernel(data, ratio, digest)
        self.assertEqual(zero_stuffed_crop((1., 2., 3.), ratio, kernel),
                         (Fraction(1), Fraction(3)))
        assert_exact_crop_invariance((1., 2., 3.), ratio, kernel, (1, 3))

    def test_fast_zero_grid_skip_matches_literal_all_taps_and_reflection(self):
        # The optimization must remain an independent conceptual j-ascending
        # high-rate reference, not a second polyphase/n-ascending DSP path.
        from phase11_math_preflight import even_reflect
        for fin, fout in ((48000, 96000), (96000, 48000), (44100, 48000)):
            with self.subTest(fin=fin, fout=fout):
                ratio = Ratio.make(fin, fout)
                shape = kernel_shape(ratio)
                taps = bytearray(8 * shape.N)
                # For L=1, offset L coincides with offset 1. Assign
                # mirrored pairs together to keep byte-exact symmetry.
                weights = {0: 0.5, 1: -0.25, ratio.L: 0.125}
                for offset, value in weights.items():
                    for position in {shape.D - offset, shape.D + offset}:
                        struct.pack_into("<d", taps, 8 * position, value)
                data = HEADER.pack(MAGIC, 1, 1, shape.L, shape.M, shape.T,
                                   0, shape.N, shape.N, 8 * shape.N) + taps
                _, kernel = read_locked_kernel(data, ratio,
                                               hashlib.sha256(data).hexdigest())
                source = (0.25, -0.5, 0.75, 1.0)
                actual = zero_stuffed_crop(source, ratio, kernel)
                reference = []
                exact_source = tuple(Fraction.from_float(v) for v in source)
                for m in range(len(actual)):
                    q = shape.D + m * ratio.M
                    expected = Fraction(0)
                    for j, coeff in enumerate(kernel):
                        if (q - j) % ratio.L == 0:
                            index = even_reflect((q - j) // ratio.L, len(source))
                            expected += exact_source[index] * coeff
                    reference.append(expected)
                self.assertEqual(actual, tuple(reference))
                assert_exact_crop_invariance(source, ratio, kernel, (1, 3))

    def test_wrong_checksum_and_invented_certification_are_refused(self):
        ratio = Ratio.make(48000, 96000)
        data, digest = structural_delta_fixture(ratio, 2.0)
        with self.assertRaises(ContractError):
            read_locked_kernel(data, ratio, "")
        with self.assertRaises(ContractError):
            read_locked_kernel(data, ratio, "0" * 64)
        with self.assertRaises(ContractError):
            read_locked_kernel(data[:-1], ratio, digest)
        with self.assertRaises(ContractError):
            read_locked_kernel(data, Ratio.make(48000, 48000), digest)
        with self.assertRaises(ContractError):
            zero_stuffed_crop((1.,), ratio, (Fraction(1),), (1, 0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
