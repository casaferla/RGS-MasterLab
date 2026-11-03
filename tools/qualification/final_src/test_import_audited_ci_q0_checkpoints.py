#!/usr/bin/env python3
"""Synthetic-only tests of audited CI Q0 checkpoint importer; NEVER real proof."""
from __future__ import annotations

from hashlib import sha256
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch
from zipfile import ZipFile, ZIP_DEFLATED

from build_candidate_manifest import canonical_pair_for_ratio
from build_resumable_real_candidates import _read_checkpoint, stage_one
from import_audited_ci_q0_checkpoints import (
    import_pinned_ci_q0, import_pinned_seed_bundle,
)
from build_resumable_real_candidates import BACKEND, RECEIPT, RECIPE
from phase11_math_preflight import ContractError, Ratio, kernel_shape
from test_build_candidate_manifest import shape_only_fixture


class AuditedCiBootstrapLifecycleTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.workdir = self.root / "checkpoints"
        self.zips = {
            "two": self.root / "two.zip",
            "one": self.root / "one.zip",
            "four": self.root / "four.zip",
        }
        self.pins = []
        for L, M, archive, filename in (
            (2, 1, "two", "rgsml-phase11-real-mpfr-48to96.bin"),
            (1, 2, "one", "rgsml-phase11-real-mpfr-88kto44k.bin"),
            (1, 4, "four", "ratio_L1_M4.bin"),
            (4, 1, "four", "ratio_L4_M1.bin"),
        ):
            pair = canonical_pair_for_ratio()[(L, M)]
            # Synthetic center delta is intentionally NOT MPFR or certifying.
            data, bits = shape_only_fixture(*pair, 1024)
            shape = kernel_shape(Ratio.make(*pair))
            pin = {
                "L": L, "M": M, "pair": list(pair),
                "sha": sha256(data).hexdigest(), "size": len(data),
                "N": shape.N, "bits": bits, "run": 12345,
                "pr": "synthetic-test-only-sha", "filename": filename,
                "archive_key": archive,
            }
            self.pins.append(pin)
        self._write_archives()

    def _write_archives(self, *, forged_pair=None):
        for archive, path in self.zips.items():
            with ZipFile(path, "w", compression=ZIP_DEFLATED) as z:
                for p in self.pins:
                    if p["archive_key"] != archive:
                        continue
                    pair = tuple(p["pair"])
                    data, _ = shape_only_fixture(*pair, 1024)
                    L, M = p["L"], p["M"]
                    receipt = {
                        "coefficient_sha256": p["sha"],
                        "coefficient_count": p["N"],
                        "payload_bytes": p["size"],
                        "rounding_precision_bits": p["bits"],
                        "canonical_generation_pair_hz": list(pair),
                        "github_run_id": str(p["run"]),
                        "git_pr_head_sha": p["pr"],
                        "same_generator_reproductions": 2,
                        "release_qualified": False,
                        "q1_q6_passed": False,
                        "product_admission_authorized": False,
                    }
                    if (L, M) == (2, 1):
                        receipt["kernel_ratio"] = {"L": L, "M": M}
                        receipt["independent_mpmath_both_precisions_matched"] = True
                    else:
                        receipt["ratio"] = {"L": L, "M": M}
                        receipt["independent_mpmath_two_precisions_match"] = True
                        receipt["exact_dyadic_crop_invariant"] = True
                        receipt["sampled_fft_negative_screens"] = [
                            {"sampled_pass": True, "nfft": 1 << 20}
                            for _ in range(2)]
                    if forged_pair == (L, M):
                        receipt["git_pr_head_sha"] = "forged-provenance"
                    z.writestr(p["filename"], data)
                    z.writestr(p["filename"] + ".expected.f64le",
                               bytes(64 if (L, M) == (2, 1)
                                     else 32 if (L, M) == (1, 2)
                                     else 16 if (L, M) == (1, 4)
                                     else 256))
                    z.writestr(p["filename"] + ".receipt.json",
                               json.dumps(receipt, sort_keys=True))

    def _import(self):
        with patch("import_audited_ci_q0_checkpoints._pinned_candidates",
                   return_value=tuple(self.pins)):
            return import_pinned_ci_q0(
                self.workdir, self.zips["two"], self.zips["one"],
                self.zips["four"])

    def test_four_strictly_audited_synthetic_checkpoints_then_resumed(self):
        result = self._import()
        self.assertEqual(result["ratios"], 4)
        self.assertEqual(result["remaining_genuine_ratio_candidates"], 10)
        self.assertFalse(result["release_qualified"])
        self.assertFalse(result["product_admission_authorized"])
        self.assertEqual(len(list(self.workdir.iterdir())), 4)
        for p in self.pins:
            data, precision = _read_checkpoint(self.workdir, *p["pair"])
            self.assertEqual(sha256(data).hexdigest(), p["sha"])
            self.assertEqual(precision, 256)
            # The EXISTING 14-ratio generator resumes these checkpoints
            # without a third expensive generator execution.
            def never_generate(*_args):
                self.fail("audited Q0 seed must be reused, not regenerated")
            reused, reused_bits = stage_one(
                self.workdir, *p["pair"], generator=never_generate)
            self.assertEqual(reused, data)
            self.assertEqual(reused_bits, precision)
        # Re-import never overwrites published checkpoints.
        self.assertEqual(self._import(), result)
        self.assertEqual(len(list(self.workdir.iterdir())), 4)

    def test_bad_ci_receipt_fails_before_any_checkpoint_publication(self):
        self._write_archives(forged_pair=(4, 1))
        with self.assertRaisesRegex(ContractError, "provenance"):
            self._import()
        self.assertFalse(self.workdir.exists())

    def test_existing_conflicting_checkpoint_is_never_replaced(self):
        self._import()
        p = next(item for item in self.pins if (item["L"], item["M"]) == (1, 4))
        path = (self.workdir / "ratio_L1_M4" /
                "RGSMSR1_L1_M4.bin")
        original = path.read_bytes()
        changed = bytearray(original)
        changed[-1] ^= 1
        path.write_bytes(changed)
        with self.assertRaises(ContractError):
            self._import()
        self.assertEqual(path.read_bytes(), changed)

    def test_missing_archive_refuses_publication(self):
        self.zips["one"].unlink()
        with self.assertRaises(ContractError):
            self._import()
        self.assertFalse(self.workdir.exists())

    def _write_seed_bundle(self, *, tamper=None):
        bundle = self.root / "audited-four-q0-checkpoints.zip"
        with ZipFile(bundle, "w", compression=ZIP_DEFLATED) as archive:
            for p in self.pins:
                L, M = p["L"], p["M"]
                prefix = f"ratio_L{L}_M{M}/"
                data, _ = shape_only_fixture(*p["pair"], 1024)
                shape = kernel_shape(Ratio.make(*p["pair"]))
                receipt = {
                    "recipe": RECIPE, "backend": BACKEND,
                    "fin": p["pair"][0], "fout": p["pair"][1],
                    "L": L, "M": M, "T": shape.T, "N": shape.N,
                    "bytes": p["size"], "sha256": p["sha"],
                    "precision_bits": 256,
                    "same_generator_double_run": True,
                    "independent_generator_verified": False,
                    "mpfr_dtft_oracle_passed": False,
                    "q1_q6_passed": False,
                    "release_qualified": False,
                    "product_admission_authorized": False,
                    "origin": "INDEPENDENTLY_AUDITED_CI_Q0_ARTIFACT",
                    "origin_github_run_id": p["run"],
                    "origin_pr_sha": p["pr"],
                }
                if tamper == (L, M):
                    receipt["origin_pr_sha"] = "forged-PR"
                archive.writestr(
                    prefix + f"RGSMSR1_L{L}_M{M}.bin", data)
                archive.writestr(
                    prefix + RECEIPT, json.dumps(receipt, sort_keys=True))
        return bundle

    def _import_bundle(self, bundle):
        with patch("import_audited_ci_q0_checkpoints._pinned_candidates",
                   return_value=tuple(self.pins)):
            return import_pinned_seed_bundle(self.workdir, bundle)

    def test_single_bundle_reuses_all_four_without_regeneration(self):
        bundle = self._write_seed_bundle()
        result = self._import_bundle(bundle)
        self.assertEqual(result["ratios"], 4)
        self.assertEqual(result["remaining_genuine_ratio_candidates"], 10)
        self.assertFalse(result["release_qualified"])
        self.assertFalse(result["product_admission_authorized"])
        self.assertEqual(self._import_bundle(bundle), result)
        for p in self.pins:
            actual, bits = _read_checkpoint(self.workdir, *p["pair"])
            self.assertEqual(sha256(actual).hexdigest(), p["sha"])
            self.assertEqual(bits, 256)
            def no_regeneration(*_):
                self.fail("a safely seeded Q0 candidate must be reused")
            self.assertEqual(
                stage_one(self.workdir, *p["pair"], generator=no_regeneration),
                (actual, bits))

    def test_single_bundle_forged_provenance_fails_atomically(self):
        bundle = self._write_seed_bundle(tamper=(4, 1))
        with self.assertRaisesRegex(ContractError, "provenance"):
            self._import_bundle(bundle)
        self.assertFalse(self.workdir.exists())

    def test_single_bundle_extra_file_fails_before_publication(self):
        bundle = self._write_seed_bundle()
        with ZipFile(bundle, "a") as archive:
            archive.writestr("../other.bin", b"not a candidate")
        with self.assertRaisesRegex(ContractError, "inventory"):
            self._import_bundle(bundle)
        self.assertFalse(self.workdir.exists())

    def test_single_bundle_conflicting_existing_checkpoint_fails_closed(self):
        bundle = self._write_seed_bundle()
        self._import_bundle(bundle)
        candidate = self.workdir / "ratio_L2_M1" / "RGSMSR1_L2_M1.bin"
        altered = bytearray(candidate.read_bytes())
        altered[52] ^= 1
        candidate.write_bytes(altered)
        with self.assertRaises(ContractError):
            self._import_bundle(bundle)
        self.assertEqual(candidate.read_bytes(), altered)


if __name__ == "__main__":
    unittest.main(verbosity=2)
