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
