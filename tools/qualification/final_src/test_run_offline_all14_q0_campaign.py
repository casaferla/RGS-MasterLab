#!/usr/bin/env python3
"""Synthetic orchestration-only checks; does NOT generate genuine MPFR kernels."""
from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from phase11_math_preflight import ContractError
from run_offline_all14_q0_campaign import run_campaign


class All14CampaignOrchestrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.work = root / "checkpoints"
        self.folder = root / "candidate"
        self.seed = root / "seed.zip"
        self.calls = []

    def _seed(self, *args):
        self.calls.append("seed")
        return {
            "ratios": 4, "source_ci_artifact_pins_verified": True,
            "release_qualified": False,
        }

    def _stage(self, *args, **kwargs):
        self.calls.append("stage")
        return {
            "ratios": 14,
            "status": "14_DOUBLE_RUN_CANDIDATES_CHECKPOINTED_NOT_RELEASE",
            "product_admission_authorized": False,
        }

    def _run(self, *, stage=None, seed=None, workers=2,
             candidate=None, precision=1024, publisher=None, verifier=None):
        optional = {}
        if publisher is not None:
            optional["publisher"] = publisher
        if verifier is not None:
            optional["verifier"] = verifier
        return run_campaign(
            self.seed, self.work, candidate or self.folder,
            workers=workers, max_precision=precision,
            seed_importer=seed or self._seed,
            generator=stage or self._stage, **optional)

    def test_unsafe_limits_refused_before_seed_reads(self):
        for workers in (0, -1, 5, True, 2.5):
            with self.subTest(workers=workers):
                with self.assertRaisesRegex(ContractError, "worker"):
                    self._run(workers=workers)
        for precision in (0, 128, 4097, True, 1.5):
            with self.subTest(precision=precision):
                with self.assertRaisesRegex(ContractError, "precision"):
                    self._run(precision=precision)
        self.assertEqual(self.calls, [])

    def test_bad_source_provenance_refused_without_generation(self):
        def dishonest_seed(*args):
            self.calls.append("seed")
            return {
                "ratios": 4,
                "source_ci_artifact_pins_verified": False,
                "release_qualified": False,
            }
        with self.assertRaisesRegex(ContractError, "seeds"):
            self._run(seed=dishonest_seed)
        self.assertEqual(self.calls, ["seed"])

    def test_missing_real_ratio_refuses_publication(self):
        def partial(*args, **kwargs):
            self.calls.append("stage")
            return {
                "ratios": 13,
                "status": "14_DOUBLE_RUN_CANDIDATES_CHECKPOINTED_NOT_RELEASE",
                "product_admission_authorized": False,
            }
        with self.assertRaisesRegex(ContractError, "fourteen"):
            self._run(stage=partial)
        self.assertEqual(self.calls, ["seed", "stage"])
        self.assertFalse(self.folder.exists())

    def test_product_admission_forgery_is_refused_before_screens(self):
        def forged(*args, **kwargs):
            self.calls.append("stage")
            return {
                "ratios": 14,
                "status": "14_DOUBLE_RUN_CANDIDATES_CHECKPOINTED_NOT_RELEASE",
                "product_admission_authorized": True,
            }
        with self.assertRaisesRegex(ContractError, "fourteen"):
            self._run(stage=forged)
        self.assertFalse(self.folder.exists())

    def _publish_fixture(self, _work, folder, **_):
        self.calls.append("publish")
        folder.mkdir()
        (folder / "fixture.txt").write_text("nonrelease", encoding="utf-8")
        return {
            "observed_ratio_count": 14, "complete_ratio_family": True,
            "independent_mpmath_screen_complete": True,
            "exact_dyadic_screen_complete": True,
            "sampled_fft_screen_complete": True,
            "exact_two_point_screen_complete": True,
            "q1_q6_passed": False,
            "product_admission_authorized": False,
            "status": "CANDIDATE_NOT_RELEASE_QUALIFIED",
        }

    def test_rejected_final_audit_does_not_publish_a_folder(self):
        def incomplete(folder, *, require_complete):
            self.calls.append("audit")
            self.assertTrue((folder / "fixture.txt").exists())
            self.assertTrue(require_complete)
            return {"complete": False,
                    "status": "CANDIDATE_INTEGRITY_ONLY_NOT_RELEASE_QUALIFIED"}
        with self.assertRaises(ContractError):
            self._run(publisher=self._publish_fixture, verifier=incomplete)
        self.assertFalse(self.folder.exists())
        self.assertEqual(self.calls, ["seed", "stage", "publish", "audit"])

    def test_verified_candidate_is_published_atomically_but_not_released(self):
        def complete(folder, *, require_complete):
            self.calls.append("audit")
            self.assertTrue(require_complete)
            self.assertTrue((folder / "fixture.txt").is_file())
            self.assertFalse(self.folder.exists())
            return {"complete": True,
                    "status": "CANDIDATE_INTEGRITY_ONLY_NOT_RELEASE_QUALIFIED"}
        result = self._run(
            publisher=self._publish_fixture, verifier=complete)
        self.assertEqual(self.calls, ["seed", "stage", "publish", "audit"])
        self.assertTrue((self.folder / "fixture.txt").is_file())
        self.assertEqual(result["nonseed_ratio_count"], 10)
        self.assertIsNone(result["new_generation_count_in_this_invocation"])
        self.assertFalse(result["release_qualified"])
        self.assertFalse(result["product_admission_authorized"])
        self.assertFalse(result["q1_q6_passed"])
        with self.assertRaises(ContractError):
            self._run(publisher=self._publish_fixture, verifier=complete)

    def test_no_overwrite_or_nested_folder(self):
        self.folder.mkdir()
        with self.assertRaisesRegex(ContractError, "overwrite"):
            self._run()
        nested = self.work / "new-candidate"
        with self.assertRaisesRegex(ContractError, "disjoint"):
            self._run(candidate=nested)
        outer = self.work.parent
        with self.assertRaisesRegex(ContractError, "overwrite"):
            self._run(candidate=outer)
        self.assertEqual(self.calls, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
