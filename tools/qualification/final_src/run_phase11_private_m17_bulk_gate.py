#!/usr/bin/env python3
"""ONE private fail-closed Phase11 genuine native+independent offline gate.

Requires actual Qt-built RGS MasterLab native test executable, genuine frozen
14-Q0+36-Q1 private ContentBox ZIPs. Runs in one NEW atomic output directory:
  A) real Qt 36 source/master two-channel binary64 oracle
  B) real Qt 15 one-tenth-second anti-alias SOURCE->MASTER output sine probes
  C) independent Q2 real36 absolute-frame/partitions/signed-zero subset
  D) 15-point high precision independent DTFT sampled alias screening
  E) 15 one-second SciPy independent real-kernel time-domain alias tests
No skipped/failed/partial gate can leave a successful receipt. Does not
qualify Q0 FULL-BAND DTFT, full Q1-Q6, runtime release or physical M17 FQ.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from phase11_math_preflight import ContractError
from run_native_q1_36_real_offline import execute as native_q1, TEST_NAME
from qualify_q2_real_36_partition_rehearsal import qualify as q2_real
from screen_real_15_downsample_alias_probe import run_all as alias_frequency
from screen_real_15_downsample_time_domain import check_time_domain as alias_signal

REAL_ALIAS_TEST = "phase11External14RealCandidates15NativeAliasSineRehearsal"
BAD_QTEST = re.compile(r"^\\s*(?:FAIL|SKIP|XFAIL|XPASS)\\s*:", re.MULTILINE)
NATIVE_PASS = re.compile(
    r"^PASS\\s*:\\s*[^\\n]*::" + REAL_ALIAS_TEST + r"\\s*\\(\\s*\\)",
    re.MULTILINE,
)


def _hash(path: Path) -> str:
    h = sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(data)
    return h.hexdigest()


def _assert_qualifying_record(name: str, record: dict, count_key: str,
                              expected: int, flag_key: str) -> None:
    if (record.get(count_key) != expected
        or record.get(flag_key) is not True
        or record.get("release_qualified") is not False
        or record.get("product_admission_authorized") is not False):
        raise ContractError("missing/nonqualifying genuine stage: " + name)


def execute(*, native_exe: Path, q0_zip: Path, q1_zip: Path,
            new_output: Path, timeout_seconds: int = 1200) -> dict:
    if (new_output.exists() or new_output.is_symlink()
        or not new_output.parent.is_dir()
        or new_output == q0_zip or new_output == q1_zip):
        raise ContractError("bulk gate needs a NEW, unused evidence directory")
    if type(timeout_seconds) is not int or not 30 <= timeout_seconds <= 1800:
        raise ContractError("invalid native bulk-gate time budget")
    if not native_exe.is_file() or not os.access(native_exe, os.X_OK):
        raise ContractError("actual compiled Qt SRC test executable is required")
    # Isolate everything until ALL independent gates have truly succeeded.
    scratch = Path(tempfile.mkdtemp(
        prefix=".rgs-phase11-private-bulk-unqualified-",
        dir=new_output.parent))
    try:
        fixture = scratch / "verified_fixtures"
        first_report = scratch / "native_36_q1.json"
        native = native_q1(native_exe,q0_zip,q1_zip,fixture,first_report,
                           timeout_seconds=timeout_seconds)
        _assert_qualifying_record("Qt native Q1",native,
                                  "source_master_pairs_verified",36,
                                  "native_test_not_skipped")
        if native.get("real_kernels") != 14 or native.get("native_test") != TEST_NAME:
            raise ContractError("native 36-Q1 receipt lacks genuine 14-kernel identity")
        assets = fixture / "real_assets"
        vectors = fixture / "q1_expected"
        env = os.environ.copy()
        env["RGSML_PHASE11_REAL_14_Q0_DIR"] = str(assets.resolve())
        env["RGSML_PHASE11_REAL_36_Q1_DIR"] = str(vectors.resolve())
        try:
            result = subprocess.run(
                [str(native_exe.resolve()), REAL_ALIAS_TEST, "-o", "-,txt"],
                env=env, capture_output=True, text=True,
                encoding="utf-8", errors="replace",
                timeout=timeout_seconds, check=False)
        except subprocess.TimeoutExpired as err:
            raise ContractError("real Qt 15-alias native gate timed out") from err
        output = result.stdout + "\n" + result.stderr
        if (result.returncode != 0 or len(NATIVE_PASS.findall(output)) != 1
            or BAD_QTEST.search(output)):
            tail = "\n".join(output.splitlines()[-12:])[:2200]
            raise ContractError("real Qt 15-alias test absent/SKIP/FAIL:\n" + tail)
        alias_native = {
            "schema":"rgsml.phase11.native-real15-alias-probe/1",
            "status":"ALL_15_REAL_NATIVE_QT_ALIAS_SINES_PASS_NONRELEASE",
            "real_downsample_pairs":15, "source_seconds_per_case":0.1,
            "min_required_attenuation_db":140.0,
            "genuine_q0_archive_sha256": _hash(q0_zip),
            "native_executable_sha256":_hash(native_exe),
            "native_named_test":REAL_ALIAS_TEST,
            "real_native_qt_pass_unskipped":True,
            "full_frequency_band_certified":False,
            "product_admission_authorized":False,
            "release_qualified":False,
        }
        (scratch/"native_15_alias.json").write_text(
            json.dumps(alias_native,indent=2,sort_keys=True)+"\n")
        q2=q2_real(assets,vectors)
        if (q2.get("checked_pairs")!=36 or q2.get("checked_signal_cases")!=72
            or q2.get("absolute_frame_crop_bitwise_invariance") is not True
            or q2.get("release_qualified") is not False):
            raise ContractError("real Q2 36-pair full/crop subset not validated")
        (scratch/"offline_q2_36_partition.json").write_text(
            json.dumps(q2,indent=2,sort_keys=True)+"\n")
        freq=alias_frequency(assets)
        _assert_qualifying_record("15 frequency probes",freq,
                                  "sampled_frequency_points",15,
                                  "all_points_pass")
        (scratch/"offline_15_frequency.json").write_text(
            json.dumps(freq,indent=2,sort_keys=True)+"\n")
        sine=alias_signal(assets)
        _assert_qualifying_record("15 independent signal outputs",sine,
                                  "checked_pairs",15,"all_points_pass")
        (scratch/"offline_15_signal.json").write_text(
            json.dumps(sine,indent=2,sort_keys=True)+"\n")
        steps=("native_36_q1.json","native_15_alias.json",
               "offline_q2_36_partition.json","offline_15_frequency.json",
               "offline_15_signal.json")
        manifest={
            "schema":"rgsml.phase11.private-native-q1-q2-alias-bulk-nonrelease/1",
            "status":"REAL_PRIVATE_Q1_Q2_ALIAS_SUBSET_BULK_PASS_NOT_RELEASE",
            "native_project_qt_36_real_Q1_unskipped":True,
            "native_project_qt_15_real_alias_unskipped":True,
            "independent_real_Q2_subcases":72,
            "real_15_alias_frequency_points":15,
            "real_15_alias_independent_sine_outputs":15,
            "genuine_Q0_kernels":14,
            "source_master_pairs":36,
            "original_q0_zip_sha256":_hash(q0_zip),
            "independent_q1_zip_sha256":_hash(q1_zip),
            "actual_native_executable_sha256":_hash(native_exe),
            "partial_reports_sha256": {k:_hash(scratch/k) for k in steps},
            "full_continuum_passband_and_stopband_certified":False,
            "full_q1_q6_conformance":False,
            "product_terminal_one_prepare_fq":False,
            "product_admission_authorized":False,
            "release_qualified":False,
        }
        (scratch/"BULK_EVIDENCE_NONRELEASE.json").write_text(
            json.dumps(manifest,indent=2,sort_keys=True)+"\n")
        if new_output.exists() or new_output.is_symlink():
            raise ContractError("refusing to overwrite concurrent bulk result")
        scratch.rename(new_output)
        return manifest
    finally:
        if scratch.exists():
            shutil.rmtree(scratch)


def main() -> None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--native-exe",type=Path,required=True)
    p.add_argument("--candidate-zip",type=Path,required=True)
    p.add_argument("--q1-reference-zip",type=Path,required=True)
    p.add_argument("--new-output-dir",type=Path,required=True)
    p.add_argument("--timeout-seconds",type=int,default=1200)
    a=p.parse_args()
    evidence=execute(native_exe=a.native_exe,
                     q0_zip=a.candidate_zip,q1_zip=a.q1_reference_zip,
                     new_output=a.new_output_dir,
                     timeout_seconds=a.timeout_seconds)
    print(evidence["status"])


if __name__ == "__main__":
    main()
