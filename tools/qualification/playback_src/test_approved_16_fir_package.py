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

    def test_no_experimental_runtime_converter_and_uniform_admission(self):
        # Source-health/preview/DEVICE may have distinct PCM owners,
        # but none may silently select a generated per-rate FIR. Make this
        # rejection part of both Focused OS preflight and later FQ builds.
        playback=(REPO/"audio"/"src"/"playback_sample_rate_adapter.cpp").read_text()
        full=(REPO/"audio"/"src"/"full_resource_output_rate_candidate.cpp").read_text()
        loader=(REPO/"audio"/"private_io"/"external_playback_fir_assets.cpp").read_text()
        policy=(REPO/"app"/"src"/"final_src_candidate_policy.hpp").read_text()
        self.assertNotIn("make_provisional_device_fir",playback)
        self.assertNotIn("experimental_rational_output_rate",full)
        self.assertIn("load_external_playback_fir(inputRate,outputRate)",playback)
        self.assertIn("PlaybackSampleRateAdapter::create",full)
        self.assertIn("phase11_frozen_original_fir_pin",loader)
        self.assertIn("PLAYBACK_FIR_TRUSTED_SHA256_MISMATCH",loader)
        self.assertNotIn("EXPERIMENTAL_UNQUALIFIED",policy)
        self.assertNotIn("EXISTING_44K_48K_FIR",policy)
        self.assertIn("PINNED_FIR_PENDING_PRODUCT_FQ",policy)

    def test_public_q0_ui_cannot_request_manual_fir_folder(self):
        # The distributed product must never ask a nontechnical user to
        # locate a historical private bank. It either has the authentic
        # EXE-local 16-file package or truthfully requires reinstall.
        qml=(REPO/"ui"/"qml"/"Main.qml").read_text(encoding="utf-8")
        main=(REPO/"app"/"src"/"main.cpp").read_text(encoding="utf-8")
        self.assertNotIn("menuUseExistingOriginalQ0Firs",qml)
        self.assertNotIn("existingOriginalQ0FirFolderDialog",qml)
        self.assertNotIn("Use existing Q0 FIR folder",qml)
        self.assertIn("menuQ0ResourcesIncomplete",qml)
        self.assertIn("Q0 FIR resources missing",qml)
        self.assertIn("FQ_DEPLOY_EXE_LOCAL_14_Q0_FIR_UNAVAILABLE",main)
        self.assertIn("if(!deploySmoke && !originalFirRoot.isEmpty())",main)

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

    def test_installed_manifest_and_binary_forged_together_must_fail(self):
        # A ZIP attacker can edit BOTH a FIR payload and its local manifest,
        # so checking only manifest-vs-payload hashes is not sufficient.
        # Release verification must also bind every installed file to the
        # independently reviewed SHA pins in the source-controlled C++.
        import hashlib
        from verify_installed_16_firs import verify, trusted_fir_inventory
        source = REPO / "resources" / "fir"
        trusted = trusted_fir_inventory(REPO)
        self.assertEqual(len(trusted), 16)
        for changed_name in (
            ORIGINALS[0], "playback_src_kernel_44100_48000.bin"
        ):
            with self.subTest(forged=changed_name), tempfile.TemporaryDirectory() as d:
                output = Path(d) / "resources" / "fir"
                export(REPO, output, require_originals=True, originals_dir=source)
                self.assertEqual(verify(output)["files"], 16)
                payload = output / changed_name
                before = bytearray(payload.read_bytes())
                self.assertEqual(hashlib.sha256(before).hexdigest(),
                                 trusted[changed_name])
                before[-1] ^= 1
                payload.write_bytes(before)
                forged_sha = hashlib.sha256(before).hexdigest()
                self.assertNotEqual(forged_sha, trusted[changed_name])
                manifest_path = output / "fir_manifest.json"
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                for entry in manifest["assets"]:
                    if entry["file"] == changed_name:
                        entry["sha256"] = forged_sha
                if changed_name.startswith("RGSMSR1_L"):
                    for entry in manifest["finalSrc"]["kernels"]:
                        if entry["file"] == changed_name:
                            entry["sha256"] = forged_sha
                else:
                    for entry in manifest["playback"]:
                        if entry["file"] == changed_name:
                            entry["sha256"] = forged_sha
                manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
                # Old manifest-only validator would report SUCCESS.
                # The independent trusted SHA must make this impossible.
                with self.assertRaisesRegex(ValueError,
                        "FQ_FIR_MANIFEST_DISAGREES_WITH_SOURCE_PIN"):
                    verify(output)

    def test_unicode_and_moved_flat_package_keep_original_sha(self):
        # Pure file-deployment proof, not a Windows Qt/PCM16 acoustic test.
        # Exact 16-file bundles must not depend on an absolute build folder,
        # ASCII-only install path or a user-installed historical Q0 directory.
        import shutil
        from verify_installed_16_firs import verify
        source=REPO/"resources"/"fir"
        with tempfile.TemporaryDirectory() as d:
            original=Path(d)/"installazione àΩ 中 con spazi"/"bin"/"resources"/"fir"
            export(REPO,original,require_originals=True,originals_dir=source)
            self.assertEqual(verify(original)["files"],16)
            relocated=Path(d)/"moved release folder"/"bin"/"resources"/"fir"
            relocated.parent.mkdir(parents=True,exist_ok=True)
            shutil.copytree(original,relocated)
            self.assertEqual(verify(relocated)["files"],16)
            self.assertEqual(
                {p.name for p in original.glob("*.bin")},
                {p.name for p in relocated.glob("*.bin")})

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

    def test_same_cmake_fq_deploy_gate_positive_and_tamper_negative(self):
        # FQ #89 regression: Debug install_deploy_smoke previously expected
        # `assets[i].private` from a withdrawn two-file public policy.
        # Execute the ACTUAL CMake installer asset check on both Focused OS.
        import subprocess
        source=REPO/"resources"/"fir"
        gate=REPO/"tests"/"cmake"/"verify_installed_fir_assets.cmake"
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/"resources"/"fir"
            export(REPO,output,require_originals=True,originals_dir=source)
            cmd=["cmake",f"-D_fir_root={output}","-P",str(gate)]
            checked=subprocess.run(cmd,capture_output=True,text=True,
                                   encoding="utf-8",errors="replace")
            self.assertEqual(checked.returncode,0,
                             checked.stdout+"\\n"+checked.stderr)
            # A single changed coefficient must fail even if the loose
            # manifest remains the same. No fake or regenerated FIR allowed.
            altered=output/ORIGINALS[0]
            binary=bytearray(altered.read_bytes())
            binary[-1]^=1
            altered.write_bytes(binary)
            rejected=subprocess.run(cmd,capture_output=True,text=True,
                                    encoding="utf-8",errors="replace")
            self.assertNotEqual(rejected.returncode,0)
            self.assertIn("INSTALLED_UNIFIED_FIR_BINARY_SHA_MISMATCH",
                          rejected.stdout+rejected.stderr)

    def test_cmake_gate_rejects_coordinated_manifest_and_binary_tamper(self):
        # Independently exercise the *existing CMake FQ install guard*,
        # not only the strengthened Python verifier: a coordinated manifest
        # and coefficient mutation must still fail the frozen C++ pin.
        import hashlib
        import subprocess
        source=REPO/"resources"/"fir"
        gate=REPO/"tests"/"cmake"/"verify_installed_fir_assets.cmake"
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/"resources"/"fir"
            export(REPO,output,require_originals=True,originals_dir=source)
            command=["cmake",f"-D_fir_root={output}","-P",str(gate)]
            ok=subprocess.run(command,capture_output=True,text=True,
                              encoding="utf-8",errors="replace")
            self.assertEqual(ok.returncode,0,ok.stdout+"\n"+ok.stderr)
            name=ORIGINALS[0]
            binary=bytearray((output/name).read_bytes())
            binary[-1]^=1
            (output/name).write_bytes(binary)
            new_digest=hashlib.sha256(binary).hexdigest()
            manifest_path=output/"fir_manifest.json"
            manifest=json.loads(manifest_path.read_text(encoding="utf-8"))
            for entry in manifest["assets"]:
                if entry["file"]==name:
                    entry["sha256"]=new_digest
            for entry in manifest["finalSrc"]["kernels"]:
                if entry["file"]==name:
                    entry["sha256"]=new_digest
            manifest_path.write_text(json.dumps(manifest),encoding="utf-8")
            failed=subprocess.run(command,capture_output=True,text=True,
                                  encoding="utf-8",errors="replace")
            self.assertNotEqual(failed.returncode,0)
            self.assertIn("INSTALLED_UNIFIED_FIR_FROZEN_Q0_PIN_CHANGED",
                          failed.stdout+failed.stderr)

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
