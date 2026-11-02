#!/usr/bin/env python3
"""FQ all-16 FIR export policy tests (source bytes never regenerated).

The fourteen SHA-pinned original source binaries now live directly in the
repository. Focused CI verifies a full real 16-file package on both systems;
FQ must refuse missing/tampered originals. All existing PlayBack and
Original-Q0 SHA pins remain unchanged; this test never fabricates substitutes.
"""
from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest
from zipfile import ZipFile

HERE=Path(__file__).resolve().parent
REPO=HERE.parents[2]
sys.path.insert(0,str(HERE))
from export_frozen_external_fir import export, KERNELS  # noqa: E402

ORIGINALS=(
    "RGSMSR1_L1_M2.bin", "RGSMSR1_L1_M4.bin",
    "RGSMSR1_L2_M1.bin", "RGSMSR1_L4_M1.bin",
    "RGSMSR1_L40_M147.bin", "RGSMSR1_L80_M147.bin",
    "RGSMSR1_L147_M40.bin", "RGSMSR1_L147_M80.bin",
    "RGSMSR1_L147_M160.bin", "RGSMSR1_L147_M320.bin",
    "RGSMSR1_L147_M640.bin", "RGSMSR1_L160_M147.bin",
    "RGSMSR1_L320_M147.bin", "RGSMSR1_L640_M147.bin",
)


class All16FirsPackagingTest(unittest.TestCase):
    def test_no_original_archive_never_claims_complete_package(self):
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/"resources"/"fir"
            export(REPO,output)
            self.assertEqual(len(list(output.glob("*.bin"))),2)
            self.assertFalse(any((output/n).exists() for n in ORIGINALS))
            catalog=json.loads((output/"fir_manifest.json").read_text())
            self.assertEqual(len(catalog["assets"]),16)
            self.assertFalse(catalog["complete16FilePackage"])
            self.assertFalse(catalog["finalSrc"]["includedInPublicBuild"])
            self.assertTrue(catalog["publicOriginalFirDistributionAuthorized"])
            expected={f"playback_src_kernel_{name}.bin":sha
                      for name,_,sha in KERNELS}
            self.assertEqual({x["file"]:x["sha256"]
                for x in catalog["assets"][:2]},expected)

    def test_official_fq_refuses_absent_originals(self):
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/"resources"/"fir"
            with self.assertRaisesRegex(
                    ValueError,"FQ requires exact original 14-Q0"):
                export(REPO,output,require_originals=True)

    def test_wrong_archive_refused_even_if_all_names_present(self):
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/"resources"/"fir"
            fake=Path(d)/"fake_originals.zip"
            with ZipFile(fake,"w") as z:
                for name in ORIGINALS:
                    z.writestr(name,b"not an original FIR")
                z.writestr("CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json","{}")
            with self.assertRaisesRegex(ValueError,"ZIP SHA-256 mismatch"):
                export(REPO,output,originals_archive=fake,
                       require_originals=True)
            self.assertFalse((output/"fir_manifest.json").exists())

    def test_direct_14_original_files_and_unified_package(self):
        source=REPO/"resources"/"fir"
        self.assertEqual(sum((source/n).is_file() for n in ORIGINALS),14)
        from verify_installed_16_firs import verify
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/"resources"/"fir"
            export(REPO,output,require_originals=True,originals_dir=source)
            result=verify(output)
            self.assertEqual(result["files"],16)
            self.assertEqual(result["originalQ0"],14)
            self.assertEqual(result["playback"],2)
            self.assertEqual(len(list(output.glob("*.bin"))),16)

    def test_direct_original_tamper_is_rejected(self):
        import shutil
        source=REPO/"resources"/"fir"
        with tempfile.TemporaryDirectory() as d:
            direct=Path(d)/"source"
            direct.mkdir()
            for name in ORIGINALS:
                shutil.copyfile(source/name,direct/name)
            changed=direct/ORIGINALS[0]
            binary=bytearray(changed.read_bytes())
            binary[-1]^=1
            changed.write_bytes(binary)
            output=Path(d)/"destination"
            with self.assertRaisesRegex(ValueError,"source pin mismatch"):
                export(REPO,output,require_originals=True,originals_dir=direct)
            self.assertFalse((output/"fir_manifest.json").exists())

    def test_real_original_archive_if_staged_by_fq_or_developer(self):
        # The real original archive has an exact immutable SHA and is on
        # ContentBox; normal public Focused CI doesn't yet contain it. Once
        # the release source has been imported this test exercises all 16.
        archive=REPO/"resources"/"fir"/"phase11_original_14_q0.zip"
        if not archive.is_file():
            self.skipTest("Approved exact original source not imported in public checkout yet")
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/"resources"/"fir"
            export(REPO,output,originals_archive=archive,
                   require_originals=True)
            names={p.name for p in output.glob("*.bin")}
            self.assertEqual(len(names),16)
            self.assertEqual(sum(n in names for n in ORIGINALS),14)
            self.assertEqual(sum(n.startswith("playback_src_kernel_") for n in names),2)
            catalog=json.loads((output/"fir_manifest.json").read_text())
            self.assertTrue(catalog["complete16FilePackage"])
            self.assertTrue(catalog["finalSrc"]["includedInPublicBuild"])
            import hashlib
            for entry in catalog["assets"]:
                self.assertEqual(hashlib.sha256(
                    (output/entry["file"]).read_bytes()).hexdigest(),
                    entry["sha256"])


if __name__=="__main__":
    unittest.main()
