#!/usr/bin/env python3
"""Adversarial mock contracts for genuine Q0 native Qt runner, NOT Q1 proof."""
from __future__ import annotations

from hashlib import sha256
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from phase11_math_preflight import ContractError
from run_four_genuine_q0_native_ci import execute, NATIVE_TESTS


class FourRealNativeTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.build=self.root/"build"
        self.build.mkdir()
        # Direct native driver now requires the same frozen playback FIR pack
        # as the CTest runtime, even with synthetic mock subprocesses.
        self.playback=self.build/"generated/rgsml/fir/playback"
        self.playback.mkdir(parents=True)
        for rate_pair in ("44100_48000","48000_44100"):
            (self.playback/f"playback_src_kernel_{rate_pair}.bin").write_bytes(
                b"mock-external-fir-not-a-real-coefficient")
        self.exe=self.build/"rgsml_playback_src_tests"
        self.exe.write_bytes(b"mock-test-binary")
        self.exe.chmod(0o755)
        self.two=self.root/"two.bin"
        self.one=self.root/"one.bin"
        self.pow4=self.root/"pow4"
        self.pow4.mkdir()
        originals=[
            self.two, self.one,
            self.pow4/"ratio_L1_M4.bin",
            self.pow4/"ratio_L4_M1.bin",
        ]
        self.pins=[]
        for n,p in enumerate(originals):
            b=b"real-stimulus-placeholder" + bytes([n])
            p.write_bytes(b)
            expected=Path(str(p)+".expected.f64le")
            expected.write_bytes(b + b"-oracle")
            self.pins.append((
                str(n),sha256(b).hexdigest(),
                sha256(expected.read_bytes()).hexdigest()))
        self.report=self.root/"report.json"

    def invoke(self, outputs):
        with patch("run_four_genuine_q0_native_ci.PINS",self.pins), \
             patch("run_four_genuine_q0_native_ci.subprocess.run",
                   side_effect=outputs) as runner:
            value=execute(build_dir=self.build,two=self.two,
                          one=self.one,fourfold_dir=self.pow4,
                          report=self.report)
            self.assertEqual(runner.call_count,4)
            for i,call in enumerate(runner.call_args_list):
                self.assertEqual(call.args[0][1],NATIVE_TESTS[i])
                self.assertIn("RGSML_PHASE11_REAL_CANARY_PATH",
                              call.kwargs["env"])
                self.assertEqual(
                    call.kwargs["env"]["RGSML_PLAYBACK_FIR_ROOT"],
                    str(self.playback.resolve()))
            return value

    @staticmethod
    def result(name,status="PASS",rc=0):
        return subprocess.CompletedProcess(
            [],rc,status+"   : rgsml::tests::PlaybackSampleRateAdapterTest::"+
            name+"()\n", "")

    def test_all_four_real_native_tests_named_pass(self):
        receipt=self.invoke([self.result(name) for name in NATIVE_TESTS])
        self.assertEqual(receipt["executed_named_native_qt_tests"],4)
        self.assertEqual(receipt["genuine_distinct_ratios"],4)
        self.assertIs(receipt["release_qualified"],False)
        self.assertIs(receipt["product_admission_authorized"],False)
        self.assertIs(receipt["source_master_full_family_verified"],False)
        self.assertTrue(self.report.is_file())

    def test_skip_must_fail_closed(self):
        for status in ("SKIP","FAIL","XFAIL","XPASS"):
            with self.subTest(status=status):
                self.report.unlink(missing_ok=True)
                invalid=[self.result(name) for name in NATIVE_TESTS]
                invalid[1]=self.result(NATIVE_TESTS[1],status=status)
                with self.assertRaises(ContractError):
                    self.invoke(invalid)
                self.assertFalse(self.report.exists())

    def test_missing_named_pass_or_nonzero_exit_fails(self):
        for invalid in (
            self.result("SomeOtherTest"),
            self.result(NATIVE_TESTS[1],rc=3),
        ):
            with self.subTest(invalid=invalid.returncode):
                self.report.unlink(missing_ok=True)
                steps=[self.result(name) for name in NATIVE_TESTS]
                steps[1]=invalid
                with self.assertRaises(ContractError):
                    self.invoke(steps)
                self.assertFalse(self.report.exists())

    def test_immutable_hashes_and_missing_binary_rejected(self):
        self.two.write_bytes(b"tampered")
        with self.assertRaises(ContractError):
            self.invoke([])
        self.two.write_bytes(b"real-stimulus-placeholder\x00")
        self.exe.unlink()
        with self.assertRaises(ContractError):
            self.invoke([])
        self.assertFalse(self.report.exists())

    def test_missing_external_playback_fir_pack_rejected(self):
        missing=self.playback/"playback_src_kernel_44100_48000.bin"
        missing.unlink()
        with self.assertRaises(ContractError):
            self.invoke([])
        self.assertFalse(self.report.exists())

    def test_existing_receipt_refused(self):
        self.report.write_text("must not overwrite")
        with self.assertRaises(ContractError):
            self.invoke([])
        self.assertEqual(self.report.read_text(),"must not overwrite")


if __name__=="__main__":
    unittest.main(verbosity=2)
