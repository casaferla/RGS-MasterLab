#!/usr/bin/env python3
"""Fast Q0 lifecycle tests for non-release atomic Phase-11 candidate builder."""
from pathlib import Path
import struct
from tempfile import TemporaryDirectory
import unittest

from build_candidate_manifest import assemble_candidate, canonical_pair_for_ratio
from phase11_math_preflight import (
    HEADER, MAGIC, Ratio, ContractError, kernel_shape,
)


def shape_only_fixture(fin, fout, precision):
    """Structurally valid synthetic CENTER DELTA, NEVER a real Kaiser kernel."""
    ratio = Ratio.make(fin, fout)
    shape = kernel_shape(ratio)
    payload = bytearray(8 * shape.N)
    struct.pack_into("<d", payload, 8 * shape.D, 1.0)
    data = HEADER.pack(MAGIC, 1, 1, shape.L, shape.M, shape.T, 0,
                       shape.N, shape.N, len(payload)) + payload
    return data, 256


class CandidateManifestLifecycle(unittest.TestCase):
    def test_all_fourteen_ratio_metadata_and_subset_is_not_release(self):
        allowed = canonical_pair_for_ratio()
        self.assertEqual(len(allowed), 14)
        sample = dict(list(allowed.items())[:2])
        with TemporaryDirectory() as d:
            target = Path(d) / "candidate"
            result = assemble_candidate(
                target, generator=shape_only_fixture, pairs=sample)
            self.assertTrue(target.is_dir())
            self.assertFalse(result["complete_ratio_family"])
            self.assertFalse(result["independent_generator_verified"])
            self.assertFalse(result["q1_q6_passed"])
            self.assertFalse(result["product_admission_authorized"])
            self.assertEqual(result["observed_ratio_count"], 2)
            self.assertEqual(len(list(target.glob("RGSMSR1_*.bin"))), 2)
            self.assertTrue((target / "CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json").is_file())
            with self.assertRaises(ContractError):
                assemble_candidate(target, generator=shape_only_fixture, pairs=sample)

    def test_mismatched_repeat_never_publishes_partial_assets(self):
        allowed = canonical_pair_for_ratio()
        sample = dict(list(allowed.items())[:1])
        counter = 0

        def divergent(fin, fout, precision):
            nonlocal counter
            counter += 1
            data, bits = shape_only_fixture(fin, fout, precision)
            if counter == 2:
                data = bytearray(data)
                data[-1] ^= 1
                data = bytes(data)
            return data, bits

        with TemporaryDirectory() as d:
            target = Path(d) / "cannot_publish"
            with self.assertRaisesRegex(ContractError, "REPRODUCTION_MISMATCH"):
                assemble_candidate(target, generator=divergent, pairs=sample)
            self.assertFalse(target.exists())
            self.assertFalse(any(Path(d).iterdir()),
                             "no partial temp assets should remain after failure")

    def test_all_ratio_independent_numeric_screen_is_optional_and_nonrelease(self):
        sample = dict(list(canonical_pair_for_ratio().items())[:2])
        observed = []

        def independent_stub(data, fin, fout):
            observed.append((fin, fout))
            self.assertEqual(data, shape_only_fixture(fin, fout, 1024)[0])
            return dict(independent_precision_stable=True,
                        candidate_bytes_match_both=True,
                        release_qualified=False)

        with TemporaryDirectory() as d:
            target = Path(d) / "screen"
            manifest = assemble_candidate(
                target, generator=shape_only_fixture, pairs=sample,
                independent_screen=independent_stub)
            self.assertEqual(len(observed), 2)
            self.assertFalse(manifest["complete_ratio_family"])
            self.assertFalse(manifest["independent_mpmath_screen_complete"])
            self.assertFalse(manifest["independent_generator_verified"])
            self.assertFalse(manifest["product_admission_authorized"])
            self.assertTrue(all(row["independent_mpmath_screen_passed"]
                                for row in manifest["assets"]))

    def test_failed_second_independent_screen_aborts_atomic_candidate(self):
        sample = dict(list(canonical_pair_for_ratio().items())[:2])
        checks = 0

        def independent_fails(data, fin, fout):
            nonlocal checks
            checks += 1
            return dict(independent_precision_stable=True,
                        candidate_bytes_match_both=(checks == 1),
                        release_qualified=False)

        with TemporaryDirectory() as d:
            target = Path(d) / "refused"
            with self.assertRaisesRegex(ContractError, "INDEPENDENT_NUMERICAL_SCREEN_FAILED"):
                assemble_candidate(target, generator=shape_only_fixture, pairs=sample,
                                   independent_screen=independent_fails)
            self.assertEqual(checks, 2)
            self.assertFalse(target.exists())
            self.assertFalse(any(Path(d).iterdir()))

    def test_actual_dyadic_candidate_screen_and_atomic_late_failure(self):
        from exact_dyadic_zero_stuff_oracle import screen_real_candidate_dyadic
        sample = dict(list(canonical_pair_for_ratio().items())[:2])
        observed = 0

        def screen(data, fin, fout):
            nonlocal observed
            observed += 1
            evidence = screen_real_candidate_dyadic(data, fin, fout)
            self.assertTrue(evidence["exact_dyadic_crop_invariant"])
            self.assertFalse(evidence["mpfr_dtft_response_proven"])
            self.assertFalse(evidence["release_qualified"])
            return evidence

        with TemporaryDirectory() as d:
            target = Path(d) / "dyadic_screen"
            result = assemble_candidate(target, generator=shape_only_fixture,
                                        pairs=sample, dyadic_screen=screen)
            self.assertEqual(observed, 2)
            self.assertFalse(result["complete_ratio_family"])
            self.assertFalse(result["exact_dyadic_screen_complete"])
            self.assertFalse(result["product_admission_authorized"])
            self.assertTrue(all(row["exact_dyadic_screen_passed"]
                                for row in result["assets"]))

        failing_calls = 0

        def deny_second(data, fin, fout):
            nonlocal failing_calls
            failing_calls += 1
            return dict(exact_dyadic_crop_invariant=(failing_calls == 1),
                        release_qualified=False)

        with TemporaryDirectory() as d:
            target = Path(d) / "refused"
            with self.assertRaisesRegex(ContractError, "EXACT_DYADIC_SCREEN_FAILED"):
                assemble_candidate(target, generator=shape_only_fixture,
                                   pairs=sample, dyadic_screen=deny_second)
            self.assertEqual(failing_calls, 2)
            self.assertFalse(target.exists())
            self.assertFalse(any(Path(d).iterdir()))

    def test_optional_sampled_fft_evidence_is_noncertifying_and_atomic(self):
        sample = dict(list(canonical_pair_for_ratio().items())[:2])
        observed = []

        def screen_good(data, fin, fout, digest):
            import hashlib
            self.assertEqual(hashlib.sha256(data).hexdigest(), digest)
            observed.append((fin, fout))
            return dict(sampled_pass=True,
                        caveat="SAMPLED_FFT_ONLY_NOT_CERTIFIED_DTFT_OR_RELEASE",
                        nfft=1 << 19, frequency_step_hz=0.5,
                        ripple_peak_to_peak_db=0.0002,
                        max_passband_error_db=0.0001,
                        sampled_stopband_attenuation_db=142.0)

        with TemporaryDirectory() as d:
            target = Path(d) / "screen"
            manifest = assemble_candidate(
                target, generator=shape_only_fixture, pairs=sample,
                sampled_fft_screen=screen_good)
            self.assertEqual(len(observed), 2)
            self.assertFalse(manifest["sampled_fft_screen_complete"])
            self.assertFalse(manifest["mpfr_dtft_oracle_passed"])
            self.assertFalse(manifest["q1_q6_passed"])
            self.assertFalse(manifest["product_admission_authorized"])
            self.assertTrue(all(row["sampled_fft_screen_passed"]
                                for row in manifest["assets"]))
            self.assertEqual(
                manifest["assets"][0]["sampled_fft_noncertifying_metrics"][
                    "sampled_stopband_attenuation_db"], 142.0)

        checks = 0

        def screen_late_failure(data, fin, fout, digest):
            nonlocal checks
            checks += 1
            return dict(sampled_pass=(checks == 1),
                        caveat="SAMPLED_FFT_ONLY_NOT_CERTIFIED_DTFT_OR_RELEASE")

        with TemporaryDirectory() as d:
            target = Path(d) / "refused"
            with self.assertRaisesRegex(
                    ContractError, "SAMPLED_FFT_SCREEN_FAILED"):
                assemble_candidate(
                    target, generator=shape_only_fixture, pairs=sample,
                    sampled_fft_screen=screen_late_failure)
            self.assertEqual(checks, 2)
            self.assertFalse(target.exists())
            self.assertFalse(any(Path(d).iterdir()))

        with TemporaryDirectory() as d:
            target = Path(d) / "no_mislabeled_proof"
            with self.assertRaisesRegex(
                    ContractError, "SAMPLED_FFT_SCREEN_FAILED"):
                assemble_candidate(
                    target, generator=shape_only_fixture, pairs=sample,
                    sampled_fft_screen=lambda *args: dict(
                        sampled_pass=True, caveat="CERTIFIED_DTFT"))
            self.assertFalse(target.exists())

    def test_reject_noncanonical_pair_and_precision_before_output(self):
        with TemporaryDirectory() as d:
            target = Path(d) / "invalid"
            with self.assertRaises(ContractError):
                assemble_candidate(target, generator=shape_only_fixture,
                                   max_precision=128)
            with self.assertRaises(ContractError):
                assemble_candidate(target, generator=shape_only_fixture,
                                   pairs={(2, 1): (44100, 192000)})
            self.assertFalse(target.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
