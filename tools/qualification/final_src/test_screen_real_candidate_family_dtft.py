#!/usr/bin/env python3
"""Fast synthetic lifecycle guards for optional REAL all-14 DTFT runner.

These tests do NOT generate a genuine kernel or measure its full spectrum.
They exercise fail-closed all-14 integrity, per-ratio provenance and the
no-release guarantee before the heavy offline qualification campaign.
"""
from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from build_candidate_manifest import assemble_candidate, canonical_pair_for_ratio
from phase11_math_preflight import ContractError
from screen_real_candidate_family_dtft import attempt_candidate_family
from test_build_candidate_manifest import shape_only_fixture


def fixture_status(data, fin, fout, expected_sha, *, max_boxes):
    from hashlib import sha256
    if sha256(data).hexdigest() != expected_sha:
        raise ContractError("mock fixture hash mismatch")
    return {
        "status": "MPFR_DTFT_INTERVAL_ATTEMPT_NOT_RELEASE_QUALIFIED",
        "fin": fin, "fout": fout, "candidate_sha256": expected_sha,
        "release_qualified": False,
        "product_admission_authorized": False,
        "continuous_bounds_closed": True,
        "passband": {
            "status": "CONTINUUM_BOUND_PROVEN_NONRELEASE",
            "visited_boxes": 1, "remaining_boxes": 0,
            "release_qualified": False},
        "stopband": {
            "status": "CONTINUUM_BOUND_PROVEN_NONRELEASE",
            "visited_boxes": 1, "remaining_boxes": 0,
            "release_qualified": False},
    }


class CandidateFamilyDtftLifecycleTests(unittest.TestCase):
    def setUp(self):
        self.d = TemporaryDirectory()
        self.addCleanup(self.d.cleanup)
        self.folder = Path(self.d.name) / "candidate"
        self.sample = dict(list(canonical_pair_for_ratio().items())[:2])
        assemble_candidate(self.folder, pairs=self.sample,
                           generator=shape_only_fixture)

    def test_real_cli_requires_all_14_even_if_fixture_proof_claims_success(self):
        with self.assertRaisesRegex(ContractError, "all 14 ratios"):
            attempt_candidate_family(self.folder, screener=fixture_status)
        outcome = attempt_candidate_family(
            self.folder, screener=fixture_status,
            require_complete=False)
        self.assertEqual(outcome["ratios_screened"], 2)
        self.assertFalse(outcome["complete_14_ratio_family"])
        self.assertFalse(outcome["continuum_bounds_closed"])
        self.assertEqual(outcome["status"],
                         "CANDIDATE_DTFT_CONTINUUM_SCREEN_UNRESOLVED")
        self.assertFalse(outcome["product_admission_authorized"])
        self.assertFalse(outcome["q1_q6_passed"])
        self.assertFalse(outcome["release_qualified"])

    def test_bounded_unresolved_remains_unresolved(self):
        calls = 0
        def one_unresolved(*args, **kwargs):
            nonlocal calls
            result = fixture_status(*args, **kwargs)
            calls += 1
            if calls == 2:
                result["stopband"]["status"] = "UNRESOLVED"
                result["stopband"]["remaining_boxes"] = 3
                result["continuous_bounds_closed"] = False
            return result
        result = attempt_candidate_family(
            self.folder, require_complete=False, screener=one_unresolved)
        self.assertEqual(calls, 2)
        self.assertFalse(result["continuum_bounds_closed"])
        self.assertFalse(result["release_qualified"])

    def test_false_release_or_continuum_claim_is_rejected(self):
        for field, value in (
            ("release_qualified", True),
            ("product_admission_authorized", True),
            ("continuous_bounds_closed", False),
            ("candidate_sha256", "0" * 64),
        ):
            def forged(*args, **kwargs):
                result = fixture_status(*args, **kwargs)
                result[field] = value
                return result
            with self.subTest(field=field):
                with self.assertRaisesRegex(ContractError, "DTFT"):
                    attempt_candidate_family(
                        self.folder, require_complete=False,
                        screener=forged)

    def test_file_corruption_fails_before_any_spectral_work(self):
        asset = next(self.folder.glob("RGSMSR1_*.bin"))
        changed = bytearray(asset.read_bytes())
        changed[-1] ^= 1
        asset.write_bytes(changed)
        def never_screen(*args, **kwargs):
            self.fail("tampered assets must not reach the MPFR oracle")
        with self.assertRaises(ContractError):
            attempt_candidate_family(
                self.folder, require_complete=False, screener=never_screen)

    def test_wrong_budget_fails_closed(self):
        for bad in (0, -1, 1.5, True, 1_000_001):
            with self.subTest(bad=bad):
                with self.assertRaisesRegex(ContractError, "budget"):
                    attempt_candidate_family(
                        self.folder, max_boxes=bad, require_complete=False,
                        screener=fixture_status)


if __name__ == "__main__":
    unittest.main(verbosity=2)
