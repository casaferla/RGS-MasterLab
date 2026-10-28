#!/usr/bin/env python3
"""Small deterministic MPFR ABI/enclosure smoke; not an SRC release oracle."""
import os
import struct
import unittest
from fractions import Fraction

from generate_rgsmsr1_mpfr import (
    Engine, ProofUnresolved, RNDN, generate, i0_interval,
)
from phase11_math_preflight import Ratio, audit_rgsmsr1_structure, kernel_shape


class MPFRZivBoundaryTests(unittest.TestCase):
    def test_real_directed_mpfr_interval_primitives(self):
        e = Engine(256)
        one = e.fraction(Fraction(1))
        two = e.fraction(Fraction(2))
        zero = e.fraction(Fraction(0))
        self.assertEqual((one + one).double_rounding_cell(),
                         struct.pack("<d", 2.0))
        self.assertEqual((one / two).double_rounding_cell(),
                         struct.pack("<d", 0.5))
        self.assertEqual((two * two).double_rounding_cell(),
                         struct.pack("<d", 4.0))
        # Directed subtraction at an exact zero may enclose both IEEE
        # signed zeros; this ambiguity must remain explicit, not be hidden.
        sine_zero = zero.sin()
        self.assertEqual(e.fn["get_d"](sine_zero.lo.ref, RNDN), 0.0)
        self.assertEqual(e.fn["get_d"](sine_zero.hi.ref, RNDN), 0.0)
        self.assertEqual((one.sqrt()).double_rounding_cell(),
                         struct.pack("<d", 1.0))
        self.assertEqual((i0_interval(zero)).double_rounding_cell(),
                         struct.pack("<d", 1.0))
        self.assertEqual((e.pi() / two).sin().double_rounding_cell(),
                         struct.pack("<d", 1.0))

    def test_provably_ambiguous_double_rounding_cell_fails_closed(self):
        e = Engine(256)
        # An interval containing both 1.0 and 1.0+2^-52 must never be
        # accepted as one binary64 coefficient, irrespective of midpoint.
        interval = e.fraction(Fraction(1))
        higher = e.fraction(Fraction(1) + Fraction(1, 1 << 52))
        interval.hi = higher.hi
        with self.assertRaises(ProofUnresolved):
            interval.double_rounding_cell()

    @unittest.skipUnless(os.environ.get("RGSML_MPFR_FULL_SRC_SMOKE") == "1",
                         "Full 1025-tap MPFR generation is an extended offline gate")
    def test_mpfr_generated_one_small_real_kernel(self):
        # Real 1025-tap 48k->96k kernel, not a toy shape. Unqualified
        # for production until second generator, DTFT, Q1-Q6 and manifest.
        ratio = Ratio.make(48000, 96000)
        shape = kernel_shape(ratio)
        data, bits = generate(48000, 96000, max_precision=1024)
        self.assertIn(bits, (256, 512, 1024))
        self.assertEqual(len(data), shape.frozen_bytes)
        self.assertEqual(len(audit_rgsmsr1_structure(data, shape)), 64)


if __name__ == "__main__":
    unittest.main(verbosity=2)
