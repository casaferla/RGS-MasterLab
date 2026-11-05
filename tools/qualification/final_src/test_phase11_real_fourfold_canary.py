#!/usr/bin/env python3
"""REAL L/M=1/4 and 4/1 MPFR/Ziv Q0 candidates, NONRELEASE ONLY.

Two actual MPFR generations per reduced ratio; independent high-precision
mpmath, exact dyadic crop, sampled FFT, DC/Nyquist and native C++ PCM fixtures.
No continuous-frequency certificate, Q1-Q6 or production admission.
"""
from __future__ import annotations

from hashlib import sha256
import json
import os
from pathlib import Path
import struct
from tempfile import TemporaryDirectory
import unittest

from build_candidate_manifest import canonical_pair_for_ratio
from build_resumable_real_candidates import stage_one
from exact_dyadic_spectral_anchors import screen_exact_spectral_anchors
from exact_dyadic_zero_stuff_oracle import read_locked_kernel, screen_real_candidate_dyadic
from independent_mpmath_crosscheck import compare_candidate
from phase11_math_preflight import Ratio, audit_rgsmsr1_structure, kernel_shape
from screen_candidate_spectral_response import screen_asset
from strict_scalar_polyphase_reference import strict_scalar_crop

SAMPLE = (0.0, 1.0, -0.5, 0.25, 0.75, 0.0, -0.25, 0.5)
CASES = (
    (1, 4, ((176400, 44100), (192000, 48000))),
    (4, 1, ((44100, 176400), (48000, 192000))),
)


class GenuineFourfoldRatioQ0(unittest.TestCase):
    @unittest.skipUnless(
        os.environ.get("RGSML_MPFR_FULL_SRC_SMOKE") == "1",
        "Real 1:4 and 4:1 generation requires the opt-in offline gate")
    def test_two_real_fourfold_ratios_with_independent_pcm(self):
        # Prior CI #904 genuine binary artifacts were downloaded, hashed,
        # and independently inspected. Replay locks reject changed binary64
        # coefficients ACROSS CI runs; NOT an independently approved release
        # coefficient manifest, DTFT continuum certificate or Q1-Q6.
        baseline = json.loads(Path(__file__).with_name(
            "Q0_REAL_FOURFOLD_CANARY_PINS_NOT_FOR_RELEASE.json"
        ).read_text(encoding="utf-8"))
        self.assertEqual(baseline["status"],
                         "CROSS_RUN_REAL_MPFR_CANDIDATES_NOT_FOR_RELEASE")
        self.assertEqual(baseline["source_github_run_id"], 40949824092)
        locked = {(entry["L"], entry["M"]): entry
                  for entry in baseline["baselines"]}
        self.assertEqual(set(locked), {(1, 4), (4, 1)})
        for claim in ("independent_release_kernel_sha_frozen",
                      "continuous_dtft_proven", "q1_q6_passed",
                      "product_admission_authorized", "release_qualified"):
            self.assertIs(baseline[claim], False)
        evidence = []
        for L, M, aliases in CASES:
            canonical = canonical_pair_for_ratio()[(L, M)]
            self.assertEqual(canonical, aliases[0])
            ratio = Ratio.make(*canonical)
            shape = kernel_shape(ratio)
            with TemporaryDirectory() as directory:
                work = Path(directory) / "checkpoint"
                asset, precision = stage_one(
                    work, *canonical, max_precision=1024)
                self.assertEqual(
                    stage_one(work, *canonical, max_precision=1024),
                    (asset, precision))
                self.assertEqual(len(list(work.iterdir())), 1)
            digest = sha256(asset).hexdigest()
            self.assertEqual(len(asset), shape.frozen_bytes)
            self.assertEqual(audit_rgsmsr1_structure(asset, shape), digest)
            self.assertIn(precision, (256, 512, 1024))
            pin = locked[(L, M)]
            self.assertEqual(pin["canonical_generation_pair_hz"],
                             list(canonical))
            self.assertEqual(pin["sha256"], digest)
            self.assertEqual(pin["bytes"], len(asset))
            self.assertEqual(pin["coefficient_count"], shape.N)
            self.assertIn(pin["precision_bits"], (256, 512, 1024))
            independent = compare_candidate(asset, *canonical)
            self.assertTrue(independent["independent_precision_stable"])
            self.assertTrue(independent["candidate_bytes_match_both"])
            self.assertFalse(independent["release_qualified"])
            dyadic = screen_real_candidate_dyadic(asset, *canonical)
            self.assertTrue(dyadic["exact_dyadic_crop_invariant"])
            self.assertFalse(dyadic["release_qualified"])
            _, taps = read_locked_kernel(asset, ratio, digest)
            output = strict_scalar_crop(SAMPLE, ratio, tuple(map(float, taps)))
            self.assertEqual(len(output), (len(SAMPLE) * L + M - 1) // M)
            screens = []
            for fin, fout in aliases:
                alias = Ratio.make(fin, fout)
                self.assertEqual((alias.L, alias.M), (L, M))
                alias_output = strict_scalar_crop(
                    SAMPLE, alias, tuple(map(float, taps)))
                self.assertEqual(
                    b"".join(struct.pack("<d", x) for x in alias_output),
                    b"".join(struct.pack("<d", x) for x in output))
                screen = screen_asset(
                    asset, fin, fout, digest, minimum_fft=1 << 20)
                self.assertTrue(
                    screen["sampled_pass"],
                    f"REAL Q0 sampled spectral screen failed: {fin}->{fout}")
                self.assertEqual(screen["nfft"], 1 << 20)
                self.assertEqual(
                    screen["caveat"],
                    "SAMPLED_FFT_ONLY_NOT_CERTIFIED_DTFT_OR_RELEASE")
                screens.append(screen)
            anchors = screen_exact_spectral_anchors(asset, *canonical)
            self.assertTrue(anchors["dc_within_strong_rational_screen"])
            self.assertTrue(
                anchors["high_grid_nyquist_within_strong_rational_screen"])
            self.assertFalse(anchors["full_frequency_band_proven"])
            self.assertFalse(anchors["release_qualified"])
            receipt = {
                "schema": "rgsml.phase11.real-fourfold-q0-candidate/1",
                "status": "REAL_MPFR_FOURFOLD_CANDIDATE_NOT_RELEASE_QUALIFIED",
                "coefficient_sha256": digest,
                "cross_run_baseline_sha256": pin["sha256"],
                "cross_run_baseline_source_run_id":
                    baseline["source_github_run_id"],
                "cross_run_baseline_bitwise_match": True,
                "ratio": {"L": L, "M": M},
                "canonical_generation_pair_hz": list(canonical),
                "same_ratio_aliases_hz": [list(a) for a in aliases],
                "coefficient_count": shape.N,
                "payload_bytes": len(asset),
                "rounding_precision_bits": precision,
                "same_generator_reproductions": 2,
                "independent_mpmath_two_precisions_match": True,
                "exact_dyadic_crop_invariant": True,
                "sampled_fft_negative_screens": screens,
                "exact_dyadic_two_point": {
                    "dc_within_strong_rational_screen": True,
                    "high_grid_nyquist_within_strong_rational_screen": True,
                    "full_frequency_band_proven": False,
                },
                "python_scalar_pcm_frames": len(output),
                "git_pr_head_sha": os.environ.get(
                    "RGSML_PHASE11_CANDIDATE_PR_SHA", ""),
                "github_run_id": os.environ.get("GITHUB_RUN_ID", ""),
                "independent_release_sha_approved": False,
                "continuous_dtft_proven": False,
                "q1_q6_passed": False,
                "product_admission_authorized": False,
                "release_qualified": False,
            }
            evidence.append((L, M, asset, output, receipt))
        # Publish ephemeral fixtures only AFTER both independent Q0 screens.
        out = os.environ.get("RGSML_PHASE11_REAL_POW4_OUT_DIR")
        if not out:
            return
        root = Path(out)
        root.mkdir(parents=True, exist_ok=True)
        for L, M, asset, pcm, receipt in evidence:
            base = root / f"ratio_L{L}_M{M}.bin"
            for path, data in (
                (base, asset),
                (Path(str(base)+".expected.f64le"),
                 b"".join(struct.pack("<d", x) for x in pcm)),
                (Path(str(base)+".receipt.json"),
                 (json.dumps(receipt, indent=2, sort_keys=True)+"\n").encode(
                     "utf-8")),
            ):
                tmp = Path(str(path)+".unpublished")
                try:
                    tmp.write_bytes(data)
                    tmp.replace(path)
                finally:
                    tmp.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
