#!/usr/bin/env python3
"""Rigorous small-fixture checks for non-release MPFR DTFT interval precursor."""
import unittest
from fractions import Fraction
from unittest.mock import patch

import mpfr_dtft_interval_preflight as dtft

from mpfr_dtft_interval_preflight import (
    PASS_TOL, STOP_TOL, enclose_dtft_box, prove_band,
)
from phase11_math_preflight import ContractError, Ratio


class DTFTIntervalProofTests(unittest.TestCase):
    def setUp(self):
        self.ratio = Ratio.make(48000, 96000)

    def test_full_frequency_box_for_exact_center_delta_is_unity(self):
        # H(f) = 1 for all real f; synthetic taps, NEVER a valid SRC lowpass.
        taps = tuple(map(Fraction, (0, 0, 2, 0, 0)))
        lo, hi = enclose_dtft_box(self.ratio, taps, 0, 48000)
        self.assertLessEqual(lo, 1)
        self.assertGreaterEqual(hi, 1)
        self.assertGreaterEqual(lo, 1 - PASS_TOL)
        self.assertLessEqual(hi, 1 + PASS_TOL)
        passed = prove_band(self.ratio, taps, 0, 48000,
                            kind="passband", max_boxes=1)
        self.assertEqual(passed["status"], "CONTINUUM_BOUND_PROVEN_NONRELEASE")
        self.assertFalse(passed["release_qualified"])
        # This kernel has NO stopband attenuation. Budget exhaustion must
        # produce UNRESOLVED, not a misleading spectral certificate.
        failed = prove_band(self.ratio, taps, 24000, 48000,
                            kind="stopband", max_boxes=3)
        self.assertEqual(failed["status"], "UNRESOLVED")
        self.assertFalse(failed["release_qualified"])

    def test_exact_first_moment_is_reused_for_entire_box_campaign(self):
        # Three adaptive boxes share ONE exact first-derivative bound.
        # Each center still performs the actual MPFR DTFT evaluation;
        # the common moment MUST NOT be recomputed N times per box.
        taps = tuple(map(Fraction, (0, 0, 2, 0, 0)))
        original = dtft._absolute_first_moment
        with patch.object(dtft, "_absolute_first_moment",
                          wraps=original) as moment_spy:
            outcome = prove_band(self.ratio, taps, 24000, 48000,
                                 kind="stopband", max_boxes=5)
        self.assertEqual(outcome["status"], "UNRESOLVED")
        self.assertEqual(outcome["visited_boxes"], 5)
        self.assertEqual(moment_spy.call_count, 1)
        self.assertFalse(outcome["release_qualified"])
        # Independently invoked box API must preserve the same exact
        # zero-phase enclosures, including a finite-width nonzero moment.
        nonzero = tuple(map(Fraction, (0, Fraction(1, 2), 1,
                                      Fraction(1, 2), 0)))
        center = dtft._enclose_dtft_box_validated(
            self.ratio, nonzero, 36000, 36100, bits=256,
            absolute_first_moment=original(nonzero))
        self.assertEqual(center, enclose_dtft_box(
            self.ratio, nonzero, 36000, 36100))

    def test_exact_small_symmetric_lowpass_frequency_endpoints(self):
        # Symmetric five-tap synthetic h: H(0)=1 and H(high-grid pi)=0.
        taps = tuple(map(Fraction,
                         (0, Fraction(1, 2), 1, Fraction(1, 2), 0)))
        dc_lo, dc_hi = enclose_dtft_box(self.ratio, taps, 0, 0)
        self.assertLessEqual(dc_lo, 1)
        self.assertGreaterEqual(dc_hi, 1)
        self.assertGreaterEqual(dc_lo, 1 - PASS_TOL)
        self.assertLessEqual(dc_hi, 1 + PASS_TOL)
        stop_lo, stop_hi = enclose_dtft_box(
            self.ratio, taps, 48000, 48000)
        self.assertLessEqual(stop_lo, 0)
        self.assertGreaterEqual(stop_hi, 0)
        self.assertGreaterEqual(stop_lo, -STOP_TOL)
        self.assertLessEqual(stop_hi, STOP_TOL)
        proof = prove_band(self.ratio, taps, 48000, 48000,
                           kind="stopband", max_boxes=1)
        self.assertEqual(proof["status"], "CONTINUUM_BOUND_PROVEN_NONRELEASE")

    def test_subnormal_dyadic_and_symmetry_fail_closed(self):
        # Smallest positive binary64 tap: exact dyadic import requires
        # shifting an MPFR integer, not an imprecise float fallback.
        tiny = Fraction(1, 1 << 1074)
        taps = (tiny, Fraction(0), Fraction(2), Fraction(0), tiny)
        lo, hi = enclose_dtft_box(self.ratio, taps, 0, 0)
        self.assertLessEqual(lo, 1 + tiny)
        self.assertGreaterEqual(hi, 1 + tiny)
        with self.assertRaisesRegex(ContractError, "symmetric"):
            enclose_dtft_box(self.ratio, (Fraction(1), Fraction(2),
                                         Fraction(3)), 0, 1)
        with self.assertRaisesRegex(ContractError, "Nyquist"):
            enclose_dtft_box(self.ratio, taps, -1, 1)
        with self.assertRaisesRegex(ContractError, "budget"):
            prove_band(self.ratio, taps, 0, 1, kind="passband", max_boxes=0)
        with self.assertRaisesRegex(ContractError, "kind"):
            prove_band(self.ratio, taps, 0, 1, kind="certified", max_boxes=1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
