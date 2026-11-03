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
        from build_candidate_manifest import canonical_pair_for_ratio
        canonical = canonical_pair_for_ratio()[(ratio.L, ratio.M)]
        self.assertEqual(canonical, (44100, 88200))
        # The MPFR kernel is a function of reduced L/M, not the absolute
        # Fs labels. Checkpoint receipts intentionally require canonical
        # aliases; 48->96 is used below as the *same* 2/1 response oracle.
        with TemporaryDirectory() as d:
            work = Path(d) / "resume"
            data, bits = stage_one(work, *canonical, max_precision=1024)
            self.assertIn(bits, (256, 512, 1024))
            self.assertEqual(
                stage_one(work, *canonical, max_precision=1024),
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
        # The SAME two genuine MPFR runs and independent mpmath checks now
        # furnish one short-lived Linux C++ candidate integration fixture.
        # No extra generator and absolutely no kernel release certificate.
        canary_out = os.environ.get("RGSML_PHASE11_REAL_CANARY_OUT")
        if canary_out:
            destination = Path(canary_out)
            destination.parent.mkdir(parents=True, exist_ok=True)
            expected = b"".join(struct.pack("<d", v) for v in scalar_whole)
            self.assertEqual(len(expected), 8 * 8)
            import json
            from screen_candidate_spectral_response import screen_asset
            from exact_dyadic_spectral_anchors import screen_exact_spectral_anchors
            digest = hashlib.sha256(data).hexdigest()
            # Real binary fixture observed and independently inspected in
            # GitHub Focused #901, rather than an in-run self-pinned digest.
            # This asserts cross-run exact reproducibility, NOT approval of
            # a release KernelManifest or an independent interval oracle.
            baseline = json.loads(
                Path(__file__).with_name(
                    "Q0_REAL_2TO1_CANARY_PIN_NOT_FOR_RELEASE.json"
                ).read_text(encoding="utf-8"))
            self.assertEqual(
                baseline["status"], "Q0_BITWISE_REPRODUCIBILITY_PIN_NOT_RELEASE")
            self.assertEqual(
                (baseline["ratio_L"], baseline["ratio_M"]), (2, 1))
            self.assertEqual(
                baseline["canonical_generation_pair_hz"], [44100, 88200])
            self.assertEqual(baseline["baseline_candidate_sha256"], digest)
            self.assertEqual(baseline["baseline_asset_bytes"], len(data))
            self.assertEqual(
                baseline["baseline_coefficient_count"], shape.N)
            # Ziv may legitimately converge at a different precision
            # on a future MPFR installation; matching the BINARY64 ASSET
            # bytes is the reproducibility contract, not iteration count.
            self.assertIn(
                baseline["baseline_precision_bits"], (256, 512, 1024))
            self.assertFalse(baseline["independent_release_kernel_sha_frozen"])
            self.assertFalse(baseline["continuous_dtft_proven"])
            self.assertFalse(baseline["q1_q6_passed"])
            self.assertFalse(baseline["product_admission_authorized"])
            self.assertFalse(baseline["release_qualified"])
            sampled_screens = [
                screen_asset(data, fin, fout, digest, minimum_fft=1 << 20)
                for fin, fout in ((44100, 88200), (48000, 96000))
            ]
            for screen in sampled_screens:
                self.assertTrue(screen["sampled_pass"],
                                "REAL 2/1 MPFR candidate failed sampled FFT negative screen")
                self.assertEqual(
                    screen["caveat"],
                    "SAMPLED_FFT_ONLY_NOT_CERTIFIED_DTFT_OR_RELEASE")
                self.assertEqual(screen["nfft"], 1 << 20)
                self.assertLessEqual(screen["ripple_peak_to_peak_db"], 0.001)
                self.assertGreaterEqual(
                    screen["sampled_stopband_attenuation_db"], 140.0)
            # Two EXACT dyadic rational spectral anchors are rigorous at
            # f=0 and high-grid Nyquist, but never cover a continuum band.
            anchors = screen_exact_spectral_anchors(data, 44100, 88200)
            self.assertTrue(anchors["dc_within_strong_rational_screen"])
            self.assertTrue(
                anchors["high_grid_nyquist_within_strong_rational_screen"])
            self.assertFalse(anchors["full_frequency_band_proven"])
            self.assertFalse(anchors["release_qualified"])
            receipt = {
                "schema": "rgsml.phase11.real-mpfr-q0-canary-artifact/2",
                "status": "REAL_MPFR_2_OVER_1_CANDIDATE_NOT_RELEASE_QUALIFIED",
                "kernel_ratio": {"L": 2, "M": 1},
                "canonical_generation_pair_hz": [44100, 88200],
                "independent_oracle_pair_hz": [48000, 96000],
                "coefficient_sha256": digest,
                "cross_run_baseline_sha256": baseline["baseline_candidate_sha256"],
                "cross_run_baseline_source_run_id":
                    baseline["source_github_run_id"],
                "cross_run_baseline_bitwise_match": True,
                "sampled_fft_negative_screens": sampled_screens,
                "exact_dyadic_two_point_screen": {
                    "dc_within_strong_rational_screen":
                        anchors["dc_within_strong_rational_screen"],
                    "high_grid_nyquist_within_strong_rational_screen":
                        anchors["high_grid_nyquist_within_strong_rational_screen"],
                    "dc_gain_exact_numerator": anchors["dc_gain_exact_numerator"],
                    "dc_gain_exact_denominator": anchors["dc_gain_exact_denominator"],
                    "high_grid_nyquist_exact_numerator":
                        anchors["high_grid_nyquist_exact_numerator"],
                    "high_grid_nyquist_exact_denominator":
                        anchors["high_grid_nyquist_exact_denominator"],
                    "full_frequency_band_proven": False,
                    "release_qualified": False,
                },
                "coefficient_count": shape.N,
                "payload_bytes": len(data),
                "rounding_precision_bits": bits,
                "same_generator_reproductions": 2,
                "independent_mpmath_both_precisions_matched": True,
                "strict_scalar_python_expected_f64le_bytes": len(expected),
                "git_pr_head_sha": os.environ.get(
                    "RGSML_PHASE11_CANDIDATE_PR_SHA", ""),
                "github_run_id": os.environ.get("GITHUB_RUN_ID", ""),
                "frozen_release_sha_approved": False,
                "independent_continuous_dtft_proven": False,
                "q1_q6_passed": False,
                "product_admission_authorized": False,
                "release_qualified": False,
            }
            for file, content in (
                (destination, data),
                (Path(str(destination) + ".expected.f64le"), expected),
                (Path(str(destination) + ".receipt.json"),
                 (json.dumps(receipt, sort_keys=True, indent=2)+"\n").encode("utf-8")),
            ):
                temporary = Path(str(file) + ".unpublished.tmp")
                try:
                    temporary.write_bytes(content)
                    temporary.replace(file)
                finally:
                    temporary.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
