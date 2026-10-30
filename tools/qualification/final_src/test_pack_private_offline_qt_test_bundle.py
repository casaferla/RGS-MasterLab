#!/usr/bin/env python3
"""Synthetic negative tests for offline genuine-Qt binary packaging; NOT audio."""
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from phase11_math_preflight import ContractError
from pack_private_offline_qt_test_bundle import pack,parse_ldd


class PortableQtNativeBundleTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        p=Path(self.tmp.name)
        self.build=p/"build"
        self.build.mkdir()
        self.exe=self.build/"rgsml_playback_src_tests"
        self.exe.write_bytes(b"mock-binary-NOT-REAL-Qt")
        self.exe.chmod(0o755)
        self.libs=p/"mock-libs"
        self.libs.mkdir()
        self.core=self.libs/"libQt6Core.so.6"
        self.test=self.libs/"libQt6Test.so.6"
        self.core.write_bytes(b"TEST-SYNTHETIC-CORE")
        self.test.write_bytes(b"TEST-SYNTHETIC-QTTEST")
        self.archive=p/"portable-nonrelease.tar.gz"

    def line(self,lib):
        return f"{lib.name} => {lib.resolve()} (0x0)"

    def test_exact_binary_and_two_qt_libs_tar_preserves_sha(self):
        ldd="\n".join((self.line(self.core),self.line(self.test)))
        with patch("pack_private_offline_qt_test_bundle.subprocess.run",
                   return_value=subprocess.CompletedProcess([],0,ldd,"")):
            manifest=pack(self.build,self.archive)
        self.assertFalse(manifest["product_admission_authorized"])
        self.assertFalse(manifest["release_qualified"])
        self.assertFalse(manifest["native_archive_contains_private_q0_q1"])
        self.assertEqual(len(manifest["dependencies"]),2)
        self.assertTrue(self.archive.is_file())
        with tarfile.open(self.archive,"r:gz") as t:
            names=t.getnames()
            self.assertIn("rgsml_offline_qt_native/rgsml_playback_src_tests",names)
            self.assertIn("rgsml_offline_qt_native/lib/libQt6Core.so.6",names)
            self.assertIn("rgsml_offline_qt_native/lib/libQt6Test.so.6",names)
            receipt=json.loads(t.extractfile(
                "rgsml_offline_qt_native/PROVENANCE_NONRELEASE.json").read())
            self.assertEqual(receipt["native_elf_sha256"],
                             manifest["native_elf_sha256"])

    def test_missing_qt_deps_and_unresolved_deps_fail(self):
        for ldd in ("libNotQt.so.1 => /tmp/libOther.so.1 (0x0)",
                    self.line(self.core),
                    self.line(self.core)+"\nlibQt6Test.so.6 => not found"):
            with self.subTest(ldd=ldd):
                with patch("pack_private_offline_qt_test_bundle.subprocess.run",
                           return_value=subprocess.CompletedProcess([],0,ldd,"")):
                    with self.assertRaises(ContractError):
                        pack(self.build,self.archive)
                self.assertFalse(self.archive.exists())

    def test_missing_binary_and_preexisting_archive_refused(self):
        self.exe.unlink()
        with self.assertRaises(ContractError):
            pack(self.build,self.archive)
        self.archive.write_bytes(b"old")
        with self.assertRaises(ContractError):
            pack(self.build,self.archive)
        self.assertEqual(self.archive.read_bytes(),b"old")

    def test_ldd_parser_has_no_untrusted_nonabsolute_or_glibc(self):
        data="\n".join((
            self.line(self.core),self.line(self.test),
            "libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x0)",
            "libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x0)",
            "linux-vdso.so.1 (0x0)",
        ))
        files=parse_ldd(data)
        self.assertEqual({p.name for p in files},
                         {"libQt6Core.so.6","libQt6Test.so.6"})


if __name__=="__main__":
    unittest.main(verbosity=2)
