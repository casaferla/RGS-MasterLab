#!/usr/bin/env python3
"""REAL 1:2 MPFR/Ziv downsample candidate crosscheck, Q0 NONRELEASE ONLY.

Uses TWO genuine MPFR interval generators (same implementation), independent
mpmath 120/160-dps numeric comparison, exact dyadic crop arithmetic,
sampled 2^20 FFT negative screens and strict scalar Python expected PCM.
All diagnostics are explicitly NONCERTIFYING; no product SRC admission.
"""
from __future__ import annotations

from fractions import Fraction
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
from exact_dyadic_zero_stuff_oracle import (
    read_locked_kernel, screen_real_candidate_dyadic,
)
from independent_mpmath_crosscheck import compare_candidate
from phase11_math_preflight import Ratio, audit_rgsmsr1_structure, kernel_shape
from screen_candidate_spectral_response import screen_asset
from strict_scalar_polyphase_reference import strict_scalar_crop

SAMPLE = (0.0, 1.0, -0.5, 0.25, 0.75, 0.0, -0.25, 0.5)
ALIASES = ((88200, 44100), (96000, 48000))


class GenuineOneToTwoDownsampleQ0(unittest.TestCase):
    @unittest.skipUnless(
        os.environ.get("RGSML_MPFR_FULL_SRC_SMOKE") == "1",
        "Actual MPFR/Ziv 1:2 two-run generation is an opt-in offline gate")
    def test_real_downsample_and_python_pcm_evidence(self):
        canonical = canonical_pair_for_ratio()[(1, 2)]
        self.assertEqual(canonical, ALIASES[0])
        ratio = Ratio.make(*canonical)
        shape = kernel_shape(ratio)
        with TemporaryDirectory() as d:
            workdir = Path(d) / "checkpoint"
            asset, bits = stage_one(workdir, *canonical, max_precision=1024)
            self.assertEqual(
                stage_one(workdir, *canonical, max_precision=1024),
                (asset, bits))
            self.assertEqual(len(list(workdir.iterdir())), 1)
        digest = sha256(asset).hexdigest()
        self.assertEqual(len(asset), shape.frozen_bytes)
        self.assertEqual(audit_rgsmsr1_structure(asset, shape), digest)
        self.assertIn(bits, (256, 512, 1024))
        numerical = compare_candidate(asset, *canonical)
        self.assertTrue(numerical["independent_precision_stable"])
        self.assertTrue(numerical["candidate_bytes_match_both"])
        self.assertFalse(numerical["release_qualified"])
        # Conceptually independent exact-dyadic index/crop verification.
        dyadic = screen_real_candidate_dyadic(asset, *canonical)
        self.assertTrue(dyadic["exact_dyadic_crop_invariant"])
        self.assertFalse(dyadic["release_qualified"])
        shape2, dyadic_taps = read_locked_kernel(asset, ratio, digest)
        self.assertEqual(shape2.N, shape.N)
        output = strict_scalar_crop(SAMPLE, ratio, tuple(map(float, dyadic_taps)))
        self.assertEqual(len(output), 4)
        sampled = []
        for fin, fout in ALIASES:
            screen = screen_asset(
                asset, fin, fout, digest, minimum_fft=1 << 20)
            self.assertTrue(
                screen["sampled_pass"],
                f"Real 1:2 alias ({fin},{fout}) failed numerical FFT screen")
            self.assertEqual(
                screen["caveat"],
                "SAMPLED_FFT_ONLY_NOT_CERTIFIED_DTFT_OR_RELEASE")
            self.assertEqual(screen["nfft"], 1 << 20)
            sampled.append(screen)
            # Both absolute Fs aliases share the exact same frozen L/M taps.
            alias_ratio = Ratio.make(fin, fout)
            self.assertEqual(alias_ratio.L, 1)
            self.assertEqual(alias_ratio.M, 2)
            self.assertEqual(
                strict_scalar_crop(
                    SAMPLE, alias_ratio, tuple(map(float, dyadic_taps))),
                output)
        anchors = screen_exact_spectral_anchors(asset, *canonical)
        self.assertTrue(anchors["dc_within_strong_rational_screen"])
        self.assertTrue(
            anchors["high_grid_nyquist_within_strong_rational_screen"])
        self.assertFalse(anchors["full_frequency_band_proven"])
        self.assertFalse(anchors["release_qualified"])
        outfile = os.environ.get("RGSML_PHASE11_REAL_DOWNSAMPLE_OUT")
        if not outfile:
            return
        base = Path(outfile)
        base.parent.mkdir(parents=True, exist_ok=True)
        expected_pcm = b"".join(struct.pack("<d", x) for x in output)
        receipt = {
            "schema": "rgsml.phase11.real-downsample-q0-candidate/1",
            "status": "REAL_MPFR_1_OVER_2_CANDIDATE_NOT_RELEASE_QUALIFIED",
            "coefficient_sha256": digest,
            "ratio": {"L": 1, "M": 2},
            "canonical_generation_pair_hz": list(canonical),
            "same_ratio_aliases_hz": [list(x) for x in ALIASES],
            "coefficient_count": shape.N,
            "payload_bytes": len(asset),
            "rounding_precision_bits": bits,
            "same_generator_reproductions": 2,
            "independent_mpmath_two_precisions_match": True,
            "exact_dyadic_crop_invariant": True,
            "sampled_fft_negative_screens": sampled,
            "exact_dyadic_two_point": {
                "dc_within_strong_rational_screen":
                    anchors["dc_within_strong_rational_screen"],
                "high_grid_nyquist_within_strong_rational_screen":
                    anchors["high_grid_nyquist_within_strong_rational_screen"],
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
        for path, content in (
            (base, asset),
            (Path(str(base)+".expected.f64le"), expected_pcm),
            (Path(str(base)+".receipt.json"),
             (json.dumps(receipt, indent=2, sort_keys=True)+"\n").encode(
                 "utf-8")),
        ):
            tmp = Path(str(path)+".unpublished")
            try:
                tmp.write_bytes(content)
                tmp.replace(path)
            finally:
                tmp.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
