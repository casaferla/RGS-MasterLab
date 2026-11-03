#!/usr/bin/env python3
"""Adversarial atomic bulk genuine-Q0/36-Q1/15-alias orchestration tests.

Mocks are only control-flow tests: NEVER genuine native audio evidence.
"""
from __future__ import annotations

from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from phase11_math_preflight import ContractError
from run_phase11_private_m17_bulk_gate import execute, REAL_ALIAS_TEST


class FullPrivateBulkFailClosedTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        p=Path(self.tmp.name)
        self.exe=p/"rgsml_playback_src_tests"
        self.exe.write_bytes(b"fake-Qt-test-exe-for-unit-control-only")
        self.exe.chmod(0o700)
        self.q0=p/"original14.zip"
        self.q1=p/"original36.zip"
        self.q0.write_bytes(b"fake-private-Q0-archive")
        self.q1.write_bytes(b"fake-private-Q1-archive")
        self.out=p/"atomic-new-nonrelease"

    def run_gate(self,*,native_status="PASS",q2count=36,
                 frequency_count=15,time_count=15):
        def native_mock(exe,q0,q1,fixture,report,**kwargs):
            (fixture/"real_assets").mkdir(parents=True)
            (fixture/"q1_expected").mkdir()
            report.write_text('{"not_real_native":true}\n')
            return {
                "real_kernels":14,"source_master_pairs_verified":36,
                "native_test":"phase11External14RealCandidatesAll36OfflineNativeRehearsal",
                "native_test_not_skipped":True,
                "release_qualified":False,
                "product_admission_authorized":False,
            }
        q2={
            "checked_pairs":q2count,"checked_signal_cases":72,
            "absolute_frame_crop_bitwise_invariance":True,
            "release_qualified":False,"product_admission_authorized":False,
        }
        f={
            "sampled_frequency_points":frequency_count,"all_points_pass":True,
            "release_qualified":False,"product_admission_authorized":False,
        }
        t={
            "checked_pairs":time_count,"all_points_pass":True,
            "release_qualified":False,"product_admission_authorized":False,
        }
        qt=subprocess.CompletedProcess(
            [],0,native_status+
            "   : rgsml::tests::PlaybackSampleRateAdapterTest::"+
            REAL_ALIAS_TEST+"()\n","")
        with patch("run_phase11_private_m17_bulk_gate.native_q1",
                   side_effect=native_mock) as native, \
             patch("run_phase11_private_m17_bulk_gate.subprocess.run",
                   return_value=qt) as runner, \
             patch("run_phase11_private_m17_bulk_gate.q2_real",
                   return_value=q2) as q2run, \
             patch("run_phase11_private_m17_bulk_gate.alias_frequency",
                   return_value=f) as frequency, \
             patch("run_phase11_private_m17_bulk_gate.alias_signal",
                   return_value=t) as time:
            value=execute(native_exe=self.exe,q0_zip=self.q0,
                          q1_zip=self.q1,new_output=self.out,
                          timeout_seconds=60)
            self.assertEqual(native.call_count,1)
            self.assertEqual(runner.call_count,1)
            self.assertEqual(runner.call_args.args[0][1],REAL_ALIAS_TEST)
            self.assertEqual(q2run.call_count,1)
            self.assertEqual(frequency.call_count,1)
            self.assertEqual(time.call_count,1)
            return value

    def test_five_genuine_gate_receipts_only_on_complete_control_path(self):
        result=self.run_gate()
        self.assertEqual(result["source_master_pairs"],36)
        self.assertEqual(result["real_15_alias_frequency_points"],15)
        self.assertEqual(result["genuine_Q0_kernels"],14)
        self.assertIs(result["release_qualified"],False)
        self.assertIs(result["full_continuum_passband_and_stopband_certified"],False)
        self.assertTrue((self.out/"BULK_EVIDENCE_NONRELEASE.json").is_file())
        self.assertEqual(len(result["partial_reports_sha256"]),5)
        self.assertTrue((self.out/"verified_fixtures").is_dir())

    def test_qt_skip_and_failure_never_publish_partial_success(self):
        for status in ("SKIP","FAIL","XFAIL","XPASS"):
            with self.subTest(status=status):
                with self.assertRaises(ContractError):
                    self.run_gate(native_status=status)
                self.assertFalse(self.out.exists())

    def test_wrong_q2_or_independent_alias_counts_refuse_atomic_receipt(self):
        for kwargs in (
            {"q2count":35},{"frequency_count":14},{"time_count":14},
        ):
            with self.subTest(kwargs=kwargs):
                with self.assertRaises(ContractError):
                    self.run_gate(**kwargs)
                self.assertFalse(self.out.exists())

    def test_existing_output_directory_and_missing_native_binary_fail(self):
        self.out.mkdir()
        with self.assertRaises(ContractError):
            self.run_gate()
        self.out.rmdir()
        self.exe.unlink()
        with self.assertRaises(ContractError):
            self.run_gate()
        self.assertFalse(self.out.exists())

    def test_invalid_budget_fails_without_writing(self):
        for bad in (0,29,1801,True):
            with self.subTest(budget=bad):
                with self.assertRaises(ContractError):
                    execute(native_exe=self.exe,q0_zip=self.q0,
                            q1_zip=self.q1,new_output=self.out,
                            timeout_seconds=bad)
                self.assertFalse(self.out.exists())


if __name__=="__main__":
    unittest.main(verbosity=2)
