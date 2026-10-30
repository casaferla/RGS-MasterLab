#!/usr/bin/env python3
"""Synthetic negative tests for offline genuine-Qt binary packaging; NOT audio."""
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from phase11_math_preflight import ContractError
from pack_private_offline_qt_test_bundle import pack,parse_ldd,verify_archive


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

    def test_archive_tamper_and_missing_members_rejected(self):
        ldd="\n".join((self.line(self.core),self.line(self.test)))
        with patch("pack_private_offline_qt_test_bundle.subprocess.run",
                   return_value=subprocess.CompletedProcess([],0,ldd,"")):
            pack(self.build,self.archive)
        self.assertEqual(len(verify_archive(self.archive)["dependencies"]),2)
        with tarfile.open(self.archive,"r:gz") as tar:
            source=[(m,tar.extractfile(m).read() if m.isfile() else None)
                    for m in tar.getmembers()]
        for kind in ("missing","altered","duplicate","symlink","unexpected","false_release"):
            with self.subTest(kind=kind):
                import copy
                items=list(source)
                if kind=="missing":
                    items=[(m,b) for m,b in items if not m.name.endswith("libQt6Test.so.6")]
                if kind=="altered":
                    items=[(m,b+b"TAMPER" if m.name.endswith("/rgsml_playback_src_tests") else b)
                           for m,b in items]
                if kind=="duplicate": items.append(items[-1])
                if kind=="symlink":
                    m,b=next((m,b) for m,b in items if m.name.endswith("libQt6Test.so.6"))
                    bad=copy.copy(m)
                    bad.type=tarfile.SYMTYPE
                    bad.linkname="/tmp/foreign"
                    items=[(bad,None) if x is m else (x,y) for x,y in items]
                if kind=="unexpected":
                    m=tarfile.TarInfo("rgsml_offline_qt_native/private/FIR.bin")
                    items.append((m,b"leak"))
                if kind=="false_release":
                    altered=[]
                    for m,b in items:
                        if m.name.endswith("PROVENANCE_NONRELEASE.json"):
                            record=json.loads(b)
                            record["release_qualified"]=True
                            b=json.dumps(record).encode()
                        altered.append((m,b))
                    items=altered
                path=Path(self.tmp.name)/("bad-"+kind+".tar.gz")
                with tarfile.open(path,"w:gz") as tar:
                    for m,b in items:
                        m=copy.copy(m)
                        if m.isfile():
                            m.size=len(b)
                            tar.addfile(m,io.BytesIO(b))
                        else: tar.addfile(m)
                with self.assertRaises(ContractError):
                    verify_archive(path)

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
