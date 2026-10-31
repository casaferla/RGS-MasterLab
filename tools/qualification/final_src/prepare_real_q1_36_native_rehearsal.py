#!/usr/bin/env python3
"""Fail-closed offline handoff of genuine 14-asset Q0 + 36 Q1 expected vectors.

No production admission; no file fetch; no GUI; no generator; NEVER certify
Q1-Q6. Inputs are frozen independent NONRELEASE archives on RGS ContentBox.
Extract only after both ZIP pins and independent per-asset/per-vector checks.
Writes one new directory atomically for operator-controlled native testing.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import shutil
import tempfile
from zipfile import ZipFile

from build_candidate_manifest import canonical_pair_for_ratio
from phase11_math_preflight import ContractError, Ratio
from verify_candidate_family import audit_candidate_family

CANDIDATE_ZIP_SHA = "94d3340667ab5c86b2db5f2d0e191f58b5d05863413312fb3545faed51798ce4"
ORACLE_ZIP_SHA = "1c48bf8cd078ab9ad75649303ddc3dae370fd1a16acfea94e3804bd355b09dff"
ORACLE_MANIFEST_SHA = "0072fb8cf2fa5930c8200c67cac21d710724b0eb0ee1f2d73e9dbf9798938d37"


def _extract_exact_archive(archive: Path, target: Path,
                           names: set[str]) -> None:
    with ZipFile(archive) as z:
        items = z.infolist()
        if (len(items) != len(names)
            or len({p.filename for p in items}) != len(items)
            or {p.filename for p in items} != names
            or any(p.is_dir() or p.file_size > 12_000_000
                   or "/" in p.filename or chr(92) in p.filename
                   or p.filename in (".", "..") for p in items)):
            raise ContractError("unexpected member, size or path in frozen Q1 archive")
        target.mkdir()
        for item in items:
            data = z.read(item.filename)
            if len(data) != item.file_size:
                raise ContractError("truncated frozen real asset")
            (target / item.filename).write_bytes(data)


def prepare(candidate_zip: Path, oracle_zip: Path, output: Path) -> dict:
    if (sha256(candidate_zip.read_bytes()).hexdigest() != CANDIDATE_ZIP_SHA
        or sha256(oracle_zip.read_bytes()).hexdigest() != ORACLE_ZIP_SHA):
        raise ContractError("frozen Q0/Q1 archive SHA differs from Drive authority")
    if (output.exists() or output.is_symlink()
        or not output.parent.is_dir()):
        raise ContractError("output must be new and have an existing parent")
    pairs = canonical_pair_for_ratio()
    candidate_names = {"CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json"}
    candidate_names |= {
        f"RGSMSR1_L{L}_M{M}.bin" for L, M in pairs
    }
    oracle_names = {"Q1_EXPECTED_MANIFEST_NONRELEASE.json",
                    "build_q1_36_real_expected_vectors.py"}
    rates = (44100, 48000, 88200, 96000, 176400, 192000)
    oracle_names |= {
        f"Q1_EXPECTED_{fin}_TO_{fout}.f64le"
        for fin in rates for fout in rates
    }
    tmp = Path(tempfile.mkdtemp(
        prefix=".rgsml-q1-36-unpublished-", dir=output.parent))
    try:
        assets = tmp / "real_assets"
        vectors = tmp / "q1_expected"
        _extract_exact_archive(candidate_zip, assets, candidate_names)
        integrity = audit_candidate_family(assets, require_complete=True)
        _extract_exact_archive(oracle_zip, vectors, oracle_names)
        manifest_bytes = (vectors / "Q1_EXPECTED_MANIFEST_NONRELEASE.json").read_bytes()
        if sha256(manifest_bytes).hexdigest() != ORACLE_MANIFEST_SHA:
            raise ContractError("36-path reference vector manifest SHA mismatch")
        report = json.loads(manifest_bytes)
        if (report.get("path_count") != 36
            or report.get("status") != "REHEARSAL_ONLY_NOT_FOR_RELEASE"
            or report.get("product_admission_authorized") is not False
            or report.get("release_qualified") is not False):
            raise ContractError("36-path oracle manifest falsely claims release")
        expected = {(fin, fout) for fin in rates for fout in rates}
        candidate_manifest = json.loads(
            (assets / "CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json").read_text())
        kernels = {(a["L"], a["M"]): a["candidate_sha256"]
                   for a in candidate_manifest["assets"]}
        seen = set()
        for row in report.get("outputs", []):
            fin, fout = row.get("fin"), row.get("fout")
            if (fin, fout) not in expected or (fin, fout) in seen:
                raise ContractError("oracle row missing, extra or duplicated")
            seen.add((fin, fout))
            filename = f"Q1_EXPECTED_{fin}_TO_{fout}.f64le"
            data = (vectors / filename).read_bytes()
            ratio = Ratio.make(fin, fout)
            if (row.get("expected_file") != filename
                or row.get("frames") != (8 * ratio.L + ratio.M - 1) // ratio.M
                or row.get("channels") != 2
                or len(data) != row["frames"] * 16
                or sha256(data).hexdigest() != row.get("interleaved_f64le_sha256")
                or row.get("real_q0_kernel_sha256") != (
                    None if fin == fout else kernels[(ratio.L, ratio.M)])):
                raise ContractError("oracle SHA, shape or genuine kernel pin mismatch")
        if seen != expected or integrity["complete"] is not True:
            raise ContractError("expected all 36 source/master pairs and 14 real kernels")
        (tmp / "NATIVE_Q1_REHEARSAL_NONRELEASE.txt").write_text(
            "ONLY CANDIDATE evidence: NEVER authorizes production SRC.\\n"
            "RGSML_PHASE11_REAL_14_Q0_DIR=" + str(output / "real_assets") + "\\n"
            "RGSML_PHASE11_REAL_36_Q1_DIR=" + str(output / "q1_expected") + "\\n"
            "With BOTH env values set, run rgsml_playback_src_tests\\n")
        if output.exists() or output.is_symlink():
            raise ContractError("refuse to overwrite a native Q1 fixture directory")
        tmp.rename(output)
        return {"status": "REAL_14_AND_36_OFFLINE_NATIVE_REHEARSAL_READY_NONRELEASE",
                "real_kernels": 14, "expected_paths": 36,
                "assets_dir": str(output / "real_assets"),
                "oracle_dir": str(output / "q1_expected"),
                "release_qualified": False,
                "product_admission_authorized": False}
    finally:
        if tmp.exists():
            shutil.rmtree(tmp)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--candidate-zip", required=True, type=Path)
    p.add_argument("--q1-reference-zip", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    a = p.parse_args()
    print(json.dumps(prepare(a.candidate_zip, a.q1_reference_zip, a.output),
                     sort_keys=True, indent=2))
