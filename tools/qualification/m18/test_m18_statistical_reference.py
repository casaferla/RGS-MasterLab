#!/usr/bin/env python3
"""Independent M18 statistical *screening* (not formal Gate G).

Theoretical targets are from the frozen M18 authority: TPDF mean=0,
variance=1/6 LSB^2; total PCM24 quantization error variance=1/4 LSB^2.
Fixed sample sizes and deliberately broad early-warning tolerances are NOT
the 2^20/adopted-Bernstein thresholds proposed in the October pre-audit.
This script uses its Python integer Philox and exact Fraction quantizer,
never any C++ implementation or a generated runtime test answer.
"""
from __future__ import annotations

import math
import unittest

from test_m18_independent_reference import (
    MASK, SEED, independent_quantize, philox,
)

SEEDS = (0, SEED, (1 << 64) - 1)
GRID = 1 << 53
SCALE = 1 << 23
N_DITHER = 32768
N_ERROR = 4096


def dither(seed: int, frame: int, channel: int) -> float:
    r0, r1, r2, r3 = philox(seed, frame, channel)
    z0 = (r0 << 21) | (r1 >> 11)
    z1 = (r2 << 21) | (r3 >> 11)
    assert 0 <= z0 < GRID and 0 <= z1 < GRID
    return (z0 - z1) / GRID


class M18IndependentStatisticalScreen(unittest.TestCase):
    def test_tpdf_moments_lags_and_stereo(self) -> None:
        # All seeds and channels are predeclared, not selected after results.
        for seed in SEEDS:
            left = [dither(seed, f, 0) for f in range(N_DITHER)]
            right = [dither(seed, f, 1) for f in range(N_DITHER)]
            for channel, data in enumerate((left, right)):
                with self.subTest(seed=f"{seed:016X}", channel=channel):
                    self.assertTrue(all(-1.0 < x < 1.0 for x in data))
                    mean = math.fsum(data) / N_DITHER
                    second = math.fsum(x*x for x in data) / N_DITHER
                    self.assertLess(abs(mean), 0.018)
                    self.assertLess(abs(second - 1/6), 0.016)
                    for lag in (1, 7, 31, 1024):
                        # Overlapping lag is a screening diagnostic, not
                        # the disjoint-pair Bernstein estimator of the audit.
                        moment = math.fsum(
                            data[i] * data[i+lag]
                            for i in range(N_DITHER-lag)
                        ) / (N_DITHER-lag)
                        self.assertLess(abs(moment / (1/6)), 0.075)
            cross = math.fsum(a*b for a,b in zip(left, right)) / N_DITHER
            self.assertLess(abs(cross / (1/6)), 0.075)

    def test_quantizer_total_error_and_zero_histogram(self) -> None:
        for channel in (0, 1):
            for variant in ("zero", "quarter", "half", "cycle"):
                with self.subTest(channel=channel, variant=variant):
                    errors = []
                    histogram = {-1: 0, 0: 0, 1: 0}
                    for frame in range(N_ERROR):
                        t = {
                            "zero": 0.0,
                            "quarter": 0.25,
                            "half": 0.5,
                            "cycle": (frame % 257) / 256 - 0.5,
                        }[variant]
                        q = independent_quantize(
                            t / SCALE, SEED, frame, channel
                        )
                        errors.append(q - t)
                        if variant == "zero":
                            self.assertIn(q, histogram)
                            histogram[q] += 1
                    mean = math.fsum(errors) / N_ERROR
                    second = math.fsum(x*x for x in errors) / N_ERROR
                    self.assertLess(abs(mean), 0.065)
                    self.assertLess(abs(second - 0.25), 0.065)
                    if variant == "zero":
                        for code, expected in ((-1, .125),(0,.75),(1,.125)):
                            self.assertLess(
                                abs(histogram[code]/N_ERROR-expected),
                                0.045
                            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
