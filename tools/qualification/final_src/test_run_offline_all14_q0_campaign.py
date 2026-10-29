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
             candidate=None, precision=1024):
        return run_campaign(
            self.seed, self.work, candidate or self.folder,
            workers=workers, max_precision=precision,
            seed_importer=seed or self._seed,
            generator=stage or self._stage)

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
