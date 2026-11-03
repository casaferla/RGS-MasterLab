#!/usr/bin/env python3
"""Fast native Q1 gate tests; mocks exercise FAIL CLOSED, not audio evidence."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from phase11_math_preflight import ContractError
from run_native_q1_36_real_offline import execute, TEST_NAME


class GenuineNativeQ1GateTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        p = Path(self.tmp.name)
        self.exe = p / "native-test"
        self.exe.write_bytes(b"native-test-not-a-real-Qt-executable")
        self.exe.chmod(0o700)
        self.candidate = p / "candidate.zip"
        self.oracle = p / "oracle.zip"
        self.candidate.write_bytes(b"mock-q0")
        self.oracle.write_bytes(b"mock-q1")
        self.workdir = p / "new-fixtures"
        self.report = p / "new-result.json"

    def call(self, result, *, real=14, paths=36):
        fixture = {
            "real_kernels": real, "expected_paths": paths,
            "product_admission_authorized": False,
            "assets_dir": str(self.workdir / "real_assets"),
            "oracle_dir": str(self.workdir / "q1_expected"),
        }
        with patch("run_native_q1_36_real_offline.prepare", return_value=fixture), \
             patch("run_native_q1_36_real_offline.subprocess.run",
                   return_value=result) as runner:
            try:
                return execute(self.exe, self.candidate, self.oracle,
                               self.workdir, self.report, timeout_seconds=45)
            finally:
                # platform.platform() may invoke subprocess.run internally
                # on Linux. The LAST mocked call is therefore not necessarily
                # our C++ invocation: inspect the exact native test argv.
                native = [
                    call for call in runner.call_args_list
                    if call.args and len(call.args[0]) >= 2
                    and call.args[0][1] == TEST_NAME
                ]
                if runner.call_count:
                    self.assertEqual(len(native), 1)
                    env = native[0].kwargs["env"]
                    self.assertEqual(env["RGSML_PHASE11_REAL_14_Q0_DIR"],
                                     fixture["assets_dir"])
                    self.assertEqual(env["RGSML_PHASE11_REAL_36_Q1_DIR"],
                                     fixture["oracle_dir"])

    def test_explicit_pass_only_without_skipped_tests(self):
        value = self.call(subprocess.CompletedProcess(
            [], 0, "PASS   : rgsml::tests::" + TEST_NAME + "()\n", ""))
        self.assertEqual(value["source_master_pairs_verified"], 36)
        self.assertFalse(value["release_qualified"])
        self.assertFalse(value["product_admission_authorized"])
        self.assertTrue(value["native_test_not_skipped"])
        self.assertTrue(self.report.is_file())

    def test_platform_probe_after_native_subprocess_never_masks_env(self):
        # Regression for #916 Linux: platform.platform() can run uname
        # THROUGH the same mocked subprocess, after the Qt invocation.
        # The native argv/env audit must inspect the native call, not the last.
        def identity_probe():
            subprocess.run(["uname", "-p"])
            return "Linux-test-probe"
        with patch("run_native_q1_36_real_offline.platform.platform",
                   side_effect=identity_probe):
            result = self.call(subprocess.CompletedProcess(
                [], 0, "PASS   : rgsml::tests::" + TEST_NAME + "()\n", ""))
        self.assertEqual(result["source_master_pairs_verified"], 36)
        self.assertTrue(result["native_test_not_skipped"])

    def test_false_positive_success_exit_but_missing_exact_test(self):
        with self.assertRaises(ContractError):
            self.call(subprocess.CompletedProcess([], 0,
                "PASS : unrelated::OtherTest()\n", ""))
        self.assertFalse(self.report.exists())

    def test_skipped_test_must_not_be_accepted(self):
        with self.assertRaises(ContractError):
            self.call(subprocess.CompletedProcess([], 0,
                "SKIP   : rgsml::tests::" + TEST_NAME + "()\n", ""))
        self.assertFalse(self.report.exists())

    def test_pass_plus_any_fail_is_not_accepted(self):
        with self.assertRaises(ContractError):
            self.call(subprocess.CompletedProcess([], 0,
                "PASS   : rgsml::tests::" + TEST_NAME +
                "()\nFAIL : SomethingElse()\n", ""))
        self.assertFalse(self.report.exists())

    def test_nonzero_exit_cannot_publish(self):
        with self.assertRaises(ContractError):
            self.call(subprocess.CompletedProcess([], 1,
                "PASS   : rgsml::tests::" + TEST_NAME + "()\n", ""))
        self.assertFalse(self.report.exists())

    def test_missing_asset_inventory_fails_before_native_launch(self):
        with self.assertRaises(ContractError):
            self.call(subprocess.CompletedProcess([], 0, "", ""),
                      paths=35)
        self.assertFalse(self.report.exists())

    def test_timeout_and_existing_report_rejected(self):
        with self.assertRaises(ContractError):
            execute(self.exe, self.candidate, self.oracle,
                    self.workdir, self.report, timeout_seconds=0)
        self.report.write_text("previous-evidence-must-not-overwrite")
        with self.assertRaises(ContractError):
            execute(self.exe, self.candidate, self.oracle,
                    self.workdir, self.report, timeout_seconds=45)


if __name__ == "__main__":
    unittest.main(verbosity=2)
