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
import sys


def verify(root: Path) -> dict:
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
    if sorted(p.name for p in root.iterdir())!=sorted(names+["fir_manifest.json"]):
        raise ValueError("FQ_FIR_FLAT_DIRECTORY_HAS_MISSING_OR_EXTRA_ASSETS")
    for entry in assets:
        name=entry["file"]
        if not isinstance(name,str) or "/" in name or "\\" in name:
            raise ValueError("FQ_UNSAFE_FIR_RESOURCE_NAME")
        p=root/name
        if not p.is_file() or p.is_symlink():
            raise ValueError(f"FQ_FIR_MISSING_OR_SYMLINK: {name}")
        if hashlib.sha256(p.read_bytes()).hexdigest()!=entry.get("sha256"):
            raise ValueError(f"FQ_FIR_SOURCE_SHA_MISMATCH: {name}")
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
