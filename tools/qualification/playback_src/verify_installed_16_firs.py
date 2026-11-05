#!/usr/bin/env python3
"""Verify EXACTLY 16 authentic binary FIRs beside the installed Windows EXE.

This is an offline FQ packaging verification, not an acoustic/MPFR or Q1-Q6
certificate. Original-Q0 and historic Playback follow the SAME deploy,
integrity and relative-directory contract. No user-side Python is required.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


# These historical Playback identities are frozen in the source exporter.
# The original-Q0 identities are loaded from the checked-in C++ pin header,
# NOT from the (user-editable) installed fir_manifest.json.
PLAYBACK_PINNED_SHA256 = {
    "playback_src_kernel_44100_48000.bin":
        "ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4",
    "playback_src_kernel_48000_44100.bin":
        "37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22",
}
ORIGINAL_PINS_HEADER = (
    Path("audio") / "include" / "rgsml" / "audio" / "internal"
    / "phase11_frozen_rgsmsr1_pins.hpp"
)
PIN_REGEX = re.compile(r'\{(\d+),(\d+),"([0-9a-f]{64})"\}')


def trusted_fir_inventory(source_root: Path | None = None) -> dict[str, str]:
    root = source_root or Path(__file__).resolve().parents[3]
    header = root / ORIGINAL_PINS_HEADER
    if not header.is_file() or header.is_symlink():
        raise ValueError("FQ_FIR_TRUSTED_CXX_PIN_HEADER_MISSING")
    rows = PIN_REGEX.findall(header.read_text(encoding="utf-8"))
    originals = {
        f"RGSMSR1_L{L}_M{M}.bin": digest for L, M, digest in rows
    }
    if len(rows) != 14 or len(originals) != 14:
        raise ValueError("FQ_FIR_TRUSTED_ORIGINAL_PIN_COUNT_INVALID")
    return {**PLAYBACK_PINNED_SHA256, **originals}


def verify(root: Path, source_root: Path | None = None) -> dict:
    if not root.is_dir() or root.is_symlink():
        raise ValueError("FQ_FIR_DIRECTORY_MISSING_OR_NOT_REGULAR")
    catalog=root/"fir_manifest.json"
    if not catalog.is_file() or catalog.is_symlink():
        raise ValueError("FQ_FIR_AUTHENTIC_MANIFEST_MISSING")
    manifest=json.loads(catalog.read_text(encoding="utf-8"))
    if manifest.get("installationRoot")!="resources/fir":
        raise ValueError("FQ_FIR_RESOURCE_ROOT_NOT_UNIFIED")
    if manifest.get("publicOriginalFirDistributionAuthorized") is not True:
        raise ValueError("FQ_14_ORIGINAL_FIR_DISTRIBUTION_NOT_AUTHORIZED")
    if manifest.get("complete16FilePackage") is not True:
        raise ValueError("FQ_16_REAL_FIR_BUNDLE_INCOMPLETE")
    if manifest.get("finalSrc",{}).get("includedInPublicBuild") is not True:
        raise ValueError("FQ_14_REAL_Q0_ORIGINAL_FIRS_NOT_EMBEDDED_IN_ZIP")
    assets=manifest.get("assets")
    if not isinstance(assets,list) or len(assets)!=16:
        raise ValueError("FQ_EXPECTED_EXACTLY_16_FIR_ENTRIES")
    names=[x.get("file") for x in assets if isinstance(x,dict)]
    if len(names)!=16 or len(set(names))!=16:
        raise ValueError("FQ_16_FIR_ASSET_INVENTORY_DUPLICATE_OR_INCOMPLETE")
    originals=[n for n in names if isinstance(n,str) and n.startswith("RGSMSR1_L") and n.endswith(".bin")]
    playback=[n for n in names if isinstance(n,str) and n.startswith("playback_src_kernel_") and n.endswith(".bin")]
    if len(originals)!=14 or len(playback)!=2:
        raise ValueError("FQ_EXPECTED_EXACTLY_14_Q0_AND_2_PLAYBACK")
    expected = trusted_fir_inventory(source_root)
    if set(names) != set(expected):
        raise ValueError("FQ_FIR_SOURCE_PINNED_INVENTORY_MISMATCH")
    if sorted(p.name for p in root.iterdir())!=sorted(names+["fir_manifest.json"]):
        raise ValueError("FQ_FIR_FLAT_DIRECTORY_HAS_MISSING_OR_EXTRA_ASSETS")
    for entry in assets:
        name=entry["file"]
        if not isinstance(name,str) or "/" in name or "\\" in name:
            raise ValueError("FQ_UNSAFE_FIR_RESOURCE_NAME")
        p=root/name
        if not p.is_file() or p.is_symlink():
            raise ValueError(f"FQ_FIR_MISSING_OR_SYMLINK: {name}")
        # Manifest and payload are both writable files inside a delivered
        # ZIP. A coordinated tamper of BOTH must still fail independently
        # against the source-controlled, reviewed frozen C++ pin inventory.
        if entry.get("sha256") != expected[name]:
            raise ValueError(f"FQ_FIR_MANIFEST_DISAGREES_WITH_SOURCE_PIN: {name}")
        if hashlib.sha256(p.read_bytes()).hexdigest()!=expected[name]:
            raise ValueError(f"FQ_FIR_BINARY_DISAGREES_WITH_SOURCE_PIN: {name}")
    return {"status":"FQ_FLAT_16_FIRS_PINNED_SHA256_VERIFIED",
            "fir_root":str(root),"files":16,"playback":2,"originalQ0":14}


def main()->None:
    parser=argparse.ArgumentParser()
    parser.add_argument("--fir-root",required=True,type=Path)
    args=parser.parse_args()
    print(json.dumps(verify(args.fir_root),sort_keys=True))


if __name__=="__main__":
    try:main()
    except (OSError,ValueError,TypeError,KeyError,json.JSONDecodeError) as exc:
        sys.exit(f"FQ_16_FIR_PACKAGE_GATE_FAILED: {exc}")
