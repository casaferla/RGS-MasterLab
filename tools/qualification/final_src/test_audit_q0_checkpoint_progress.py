#!/usr/bin/env python3
"""Synthetic, no-MPFR unit tests for a READ-ONLY real checkpoint audit.

Test-injected synthetic coefficient bytes are never release proof and never
accepted by the real production reader, which rejects unqualified receipts.
"""
from __future__ import annotations

from hashlib import sha256
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from audit_q0_checkpoint_progress import audit_progress
from build_candidate_manifest import canonical_pair_for_ratio
from phase11_math_preflight import ContractError
from test_build_candidate_manifest import shape_only_fixture


class ReadOnlyProgressTests(unittest.TestCase):
    def setUp(self):
        self.temporary = TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "checkpoints"
        self.values = {}
        for (L, M), pair in canonical_pair_for_ratio().items():
            data, bits = shape_only_fixture(*pair, 256)
            self.values[(L, M)] = (data, bits)
        self.pins = tuple({
            "L": L, "M": M, "pair": list(canonical_pair_for_ratio()[(L, M)]),
            "sha": sha256(self.values[(L, M)][0]).hexdigest(),
            "bits": 256,
        } for (L, M) in ((2, 1), (1, 2), (1, 4), (4, 1)))
        self.reads = 0

    def _read_fixture(self, work, fin, fout):
        self.assertEqual(work, self.root)
        self.reads += 1
        from phase11_math_preflight import Ratio
        key = Ratio.make(fin, fout)
        return self.values[(key.L, key.M)]

    def _progress(self):
        return audit_progress(
            self.root, checked_reader=self._read_fixture,
            pinned_reader=lambda: self.pins)

    def _create(self, ratios):
        self.root.mkdir(exist_ok=True)
        for L, M in ratios:
            (self.root / f"ratio_L{L}_M{M}").mkdir(exist_ok=True)

    def test_missing_root_does_not_look_like_qualifying_work(self):
        report = self._progress()
        self.assertFalse(report["checkpoint_root_present"])
        self.assertEqual(report["validated_real_checkpoint_count"], 0)
        self.assertEqual(report["remaining_missing_ratio_count"], 14)
        self.assertEqual(self.reads, 0)
        self.assertIsNone(report["time_to_complete"])
        self.assertFalse(report["release_qualified"])
        self.assertFalse(report["product_admission_authorized"])

    def test_four_original_ci_seeds_leave_ten_actual_missing_ratios(self):
        self._create(((2, 1), (1, 2), (1, 4), (4, 1)))
        report = self._progress()
        self.assertEqual(report["validated_real_checkpoint_count"], 4)
        self.assertEqual(report["validated_pinned_original_ci_count"], 4)
        self.assertEqual(report["remaining_missing_ratio_count"], 10)
        self.assertEqual(self.reads, 4)
        self.assertEqual(len(report["missing"]), 10)
        self.assertEqual(
            report["verified_rgsmsr1_bytes"],
            sum(len(self.values[k][0]) for k in
                ((2, 1), (1, 2), (1, 4), (4, 1))))
        self.assertFalse(report["continuum_dtft_proven"])
        self.assertFalse(report["q1_q6_passed"])
        self.assertIsNone(report["time_to_complete"])

    def test_synthetic_fourteen_must_still_be_nonrelease(self):
        self._create(tuple(canonical_pair_for_ratio()))
        report = self._progress()
        self.assertEqual(report["validated_real_checkpoint_count"], 14)
        self.assertEqual(report["remaining_missing_ratio_count"], 0)
        self.assertEqual(report["validated_pinned_original_ci_count"], 4)
        self.assertEqual(self.reads, 14)
        self.assertEqual(report["status"],
                         "ALL_14_REAL_CHECKPOINTS_VALIDATED_NOT_RELEASE")
        self.assertFalse(report["product_admission_authorized"])
        self.assertFalse(report["release_qualified"])
        self.assertFalse(report["q1_q6_passed"])

    def test_wrong_original_ci_coefficient_sha_refused(self):
        self._create(((2, 1),))
        altered = [dict(x) for x in self.pins]
        altered[0]["sha"] = "0"*64
        with self.assertRaisesRegex(ContractError, "original real CI"):
            audit_progress(
                self.root, checked_reader=self._read_fixture,
                pinned_reader=lambda: tuple(altered))

    def test_incomplete_temporary_dir_cannot_count_as_checkpoint(self):
        self._create(((2, 1),))
        (self.root / ".incomplete-ratio-audit-pending").mkdir()
        result = self._progress()
        self.assertEqual(result["outstanding_incomplete_temporary_entries"],1)
        self.assertEqual(result["validated_real_checkpoint_count"],1)
        self.assertEqual(result["remaining_missing_ratio_count"],13)
        self.assertEqual(result["status"],
                         "PARTIAL_OR_INCOMPLETE_REAL_CHECKPOINTS_NOT_RELEASE")

    def test_unexpected_entry_is_error_not_a_fourteenth_ratio(self):
        self._create(((2, 1),))
        (self.root / "ratio_L999_M1").mkdir()
        with self.assertRaisesRegex(ContractError,"unexpected"):
            self._progress()
        self.assertEqual(self.reads,0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
