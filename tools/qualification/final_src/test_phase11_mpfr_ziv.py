#!/usr/bin/env python3
"""Small deterministic MPFR ABI/enclosure smoke; not an SRC release oracle."""
import os
import struct
import unittest
from fractions import Fraction

from generate_rgsmsr1_mpfr import (
    Engine, ProofUnresolved, RNDN, generate, i0_interval, sinc_ratio_interval,
)
from phase11_math_preflight import Ratio, audit_rgsmsr1_structure, kernel_shape


class MPFRZivBoundaryTests(unittest.TestCase):
    def test_integral_rational_sinc_zeros_are_exact(self):
        e = Engine(256)
        pi = e.pi()
        self.assertEqual(sinc_ratio_interval(e, pi, 0, 2, 1).double_rounding_cell(),
                         struct.pack("<d", 1.0))
        # ratio L/M=2/1: exact z = 49*u/100, hence +/-100*k zeros.
        zero = struct.pack("<d", 0.0)
        for u in (-500, -100, 100, 500):
            self.assertEqual(sinc_ratio_interval(e, pi, u, 2, 1).double_rounding_cell(),
                             zero)
        self.assertNotEqual(sinc_ratio_interval(e, pi, 101, 2, 1).double_rounding_cell(),
                            zero)

    def test_analytic_sinc_zeros_all_fourteen_reduced_ratios(self):
        from phase11_math_preflight import RATES
        ratios = {(Ratio.make(fin, fout).L, Ratio.make(fin, fout).M)
                  for fin in RATES for fout in RATES if fin != fout}
        self.assertEqual(len(ratios), 14)
        e = Engine(256)
        pi = e.pi()
        zero = struct.pack("<d", 0.0)
        checked = 0
        for L, M in sorted(ratios):
            # Frozen z=49*u/(50*max(L,M)): the first nonzero integral z
            # lies at u=50*max(L,M)/gcd(49,50*max(L,M)).
            from math import gcd
            first = 50 * max(L, M) // gcd(49, 50 * max(L, M))
            self.assertEqual(sinc_ratio_interval(e, pi, first, L, M).double_rounding_cell(),
                             zero)
            self.assertEqual(sinc_ratio_interval(e, pi, -first, L, M).double_rounding_cell(),
                             zero)
            checked += 2
        self.assertEqual(checked, 28)

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
        # Exercise the REAL resumable runner using the two already-required
        # full MPFR/Ziv executions. Do not add a third expensive generator.
        from pathlib import Path
        from tempfile import TemporaryDirectory
        from build_resumable_real_candidates import stage_one
        with TemporaryDirectory() as d:
            work = Path(d) / "resume"
            data, bits = stage_one(work, 48000, 96000, max_precision=1024)
            self.assertIn(bits, (256, 512, 1024))
            self.assertEqual(
                stage_one(work, 48000, 96000, max_precision=1024),
                (data, bits),
                "REAL already-proven per-ratio checkpoint must resume unchanged")
            self.assertEqual(len(list(work.iterdir())), 1)
        # A genuinely separate, high-precision numerical implementation
        # checks the exact candidate bytes at two rebuilt precisions.
        # This is an independent CROSSCHECK, not certified MPFR/DTFT Q1.
        from independent_mpmath_crosscheck import compare_candidate
        independent = compare_candidate(data, 48000, 96000)
        self.assertTrue(independent["independent_precision_stable"])
        self.assertTrue(independent["candidate_bytes_match_both"])
        self.assertFalse(independent["release_qualified"])
        # Independent conceptual ZERO-STUFFED arithmetic on actual generated
        # MPFR candidate bytes (not the prior synthetic center-delta fixture).
        # This digest derives from this still-unqualified candidate: it is
        # NOT an independently frozen release KernelManifest.
        import hashlib
        from exact_dyadic_zero_stuff_oracle import (
            read_locked_kernel, zero_stuffed_crop, assert_exact_crop_invariance)
        shape2, dyadic = read_locked_kernel(
            data, ratio, hashlib.sha256(data).hexdigest())
        self.assertEqual(shape2.N, shape.N)
        impulse = (0.0, 1.0, 0.0, 0.0)
        whole = zero_stuffed_crop(impulse, ratio, dyadic)
        self.assertEqual(len(whole), 8)
        assert_exact_crop_invariance(impulse, ratio, dyadic, (1, 3))
        # Same genuine MPFR-generated kernel bytes, but an independently
        # indexed n-ascending binary64 multiply/add reference. This cross-
        # checks product arithmetic order against exact dyadic j-ascending,
        # not a certified MPFR/QTP release PCM expected.
        from strict_scalar_polyphase_reference import strict_scalar_crop
        float_kernel = tuple(float(x) for x in dyadic)
        scalar_whole = strict_scalar_crop(impulse, ratio, float_kernel)
        self.assertEqual(len(scalar_whole), len(whole))
        self.assertEqual(strict_scalar_crop(impulse, ratio, float_kernel, (1, 3)),
                         scalar_whole[2:6])
        for scalar_sample, exact_sample in zip(scalar_whole, whole):
            self.assertLessEqual(
                abs(Fraction.from_float(scalar_sample) - exact_sample),
                Fraction(1, 10 ** 12))
        constant = zero_stuffed_crop((0.25,) * 4, ratio, dyadic)
        self.assertEqual(len(constant), 8)
        for sample in constant:
            self.assertAlmostEqual(float(sample), 0.25, delta=1e-8)

        self.assertEqual(len(data), shape.frozen_bytes)
        self.assertEqual(len(audit_rgsmsr1_structure(data, shape)), 64)
        # Analytic sinc zeros must survive the WHOLE normalization and
        # RGSMSR1 serialization as exact +0.0 binary64 coefficients.
        for offset in (-500, -100, 100, 500):
            at = 52 + 8 * (shape.D + offset)
            self.assertEqual(data[at:at + 8], struct.pack("<d", 0.0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
