#!/usr/bin/env python3
"""Adversarial NONRELEASE numerical screens; real Q0 canary when supplied."""
from __future__ import annotations
from hashlib import sha256
import os
from pathlib import Path
import unittest

import numpy as np
from phase11_math_preflight import ContractError
from screen_real_15_downsample_alias_probe import numerical_probe, STOP_DB


class DownsampleAliasProbeTests(unittest.TestCase):
    def test_delta_fir_is_not_an_antialias_filter(self):
        h=np.array([0.0,1.0,0.0],dtype=np.float64)
        for fin,fout in ((88200,44100),(96000,48000)):
            self.assertAlmostEqual(numerical_probe(h,fin,fout,1),0.0,places=10)

    def test_symmetric_normalized_smoothing_is_not_false_certified(self):
        taps=np.array([0.25,0.5,0.25],dtype=np.float64)
        measured=numerical_probe(taps,96000,48000,1)
        self.assertGreater(measured,0)
        self.assertLess(measured,STOP_DB)

    def test_bad_normalization_asymmetry_and_nonfinite_rejected(self):
        for taps in (
            [0.0,0.5,0.0], [0.0,0.5,0.5],
            [0.0, float("nan"), 0.0],
            [0.0, float("inf"), 0.0],
            [1.0,2.0], []):
            with self.subTest(taps=taps):
                with self.assertRaises(ContractError):
                    numerical_probe(np.array(taps,dtype="<f8"),
                                    96000,48000,1)

    def test_invalid_downsample_pair_and_ratio_rejected(self):
        h=np.array([0.0,1.0,0.0],dtype="<f8")
        for fin,fout,L in ((48000,96000,2),(48000,48000,1),
                           (96000,48000,0)):
            with self.subTest(case=(fin,fout,L)):
                with self.assertRaises(ContractError):
                    numerical_probe(h,fin,fout,L)

    def test_one_original_real_frozen_Q0_canary_when_supplied(self):
        name=os.environ.get("RGSML_PHASE11_REAL_DOWNSAMPLE_PATH")
        if not name:
            self.skipTest("Genuine MPFR binary not supplied; ordinary unit tests only")
        data=Path(name).read_bytes()
        self.assertEqual(sha256(data).hexdigest(),
                         "b97776527687180bea6dbf86a8da877c0bdaa528d305abe4b7beaf806ecaf48e")
        h=np.frombuffer(data,dtype="<f8",offset=52)
        self.assertGreaterEqual(numerical_probe(h,88200,44100,1),STOP_DB)


if __name__=="__main__":
    unittest.main(verbosity=2)
