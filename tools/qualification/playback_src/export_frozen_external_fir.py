#!/usr/bin/env python3
"""Build-time bit-exact export of the unified 16 original SRC FIR resources.

The two historic Playback FIRs are extracted bit-for-bit from frozen source.
When a genuine frozen ORIGINAL 14-Q0 ZIP is present, the exact 14 SHA-pinned
binary files are staged beside them in the SAME resources/fir directory. No
original filter is regenerated; no Python or separate install for end users.
The ordinary Focused CI may run without original-14 bytes, but a real FQ build
MUST require the complete authentic 16-file package and fail closed otherwise.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from math import gcd
from pathlib import Path
import re
import struct
import sys

KERNELS = (
    ("44100_48000", 30721, "ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4"),
    ("48000_44100", 28225, "37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22"),
)
WORDS = re.compile(r"UINT64_C\(0x([0-9a-fA-F]{16})\)")


def export(source_root: Path, output_root: Path,
           originals_archive: Path | None = None,
           require_originals: bool = False) -> None:
    output_root.mkdir(parents=True, exist_ok=True)
    entries = []
    for pair, expected_count, expected_sha in KERNELS:
        source = source_root / "audio" / "src" / "internal" / f"playback_src_kernel_{pair}.inc"
        encoded = source.read_text(encoding="utf-8")
        # Frozen source is an explicit C++ constant list, NOT code to evaluate.
        words = WORDS.findall(encoded)
        if len(words) != expected_count:
            raise ValueError(f"{pair}: frozen coefficient count changed")
        binary = b"".join(struct.pack("<Q", int(w, 16)) for w in words)
        digest = hashlib.sha256(binary).hexdigest()
        if digest != expected_sha or len(binary) != expected_count * 8:
            raise ValueError(f"{pair}: frozen playback SHA/length mismatch")
        name = f"playback_src_kernel_{pair}.bin"
        destination = output_root / name
        # Re-verify any cached build artifact before reusing it.
        if not destination.is_file() or destination.read_bytes() != binary:
            tmp = destination.with_suffix(".tmp")
            tmp.write_bytes(binary)
            tmp.replace(destination)
        entries.append({"file": name, "coefficients": expected_count,
                        "sha256": expected_sha,
                        "encoding": "ieee754-binary64-uint64-little-endian-j-ascending"})
    # One SHA-pinned inventory: same on-disk root for playback and Final SRC.
    # PO has explicitly authorized public distribution of the original 14.
    pins_source = (source_root / "audio" / "include" / "rgsml" / "audio"
                   / "internal" / "phase11_frozen_rgsmsr1_pins.hpp")
    pins = re.findall(r'\{(\d+),(\d+),"([a-f0-9]{64})"\}',
                      pins_source.read_text(encoding="utf-8"))
    if len(pins) != 14 or len(set((L, M) for L, M, _ in pins)) != 14:
        raise ValueError("Original 14-Q0 trusted FIR pin inventory changed")
    final_src = [
        {"file": f"RGSMSR1_L{L}_M{M}.bin", "ratioL": int(L),
         "ratioM": int(M), "sha256": digest, "private": True}
        for L, M, digest in pins
    ]
    if require_originals and originals_archive is None:
        raise ValueError("FQ requires exact original 14-Q0 FIR archive")
    originals_included = originals_archive is not None
    if originals_included:
        # This audited reader requires an EXACT matching frozen source ZIP
        # SHA, inventory, 14 per-file SHA/size, regular-file semantics and
        # candidate-origin manifest. No ambient network/hidden fallback.
        import importlib.util
        reader_path = (source_root / "tools" / "qualification" /
                       "final_src" / "install_private_q0_originals.py")
        spec = importlib.util.spec_from_file_location("rgsml_original_q0_archive_reader", reader_path)
        if spec is None or spec.loader is None:
            raise ValueError("Original Q0 ZIP authenticated reader unavailable")
        reader = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(reader)
        binaries = reader.checked_files(originals_archive)
        required = {entry["file"]: entry["sha256"] for entry in final_src}
        if set(binaries) != set(required):
            raise ValueError("Original 14-Q0 ZIP does not match frozen release inventory")
        for name in sorted(required):
            binary = binaries[name]
            if hashlib.sha256(binary).hexdigest() != required[name]:
                raise ValueError(f"Original Q0 FIR source pin mismatch: {name}")
            destination = output_root / name
            if not destination.is_file() or destination.read_bytes() != binary:
                temporary = destination.with_suffix(".tmp")
                temporary.write_bytes(binary)
                temporary.replace(destination)
    else:
        # Never leave an old 14-file set in a reused two-only build tree:
        # a stale prior FQ export must not appear authenticated by accident.
        for item in final_src:
            (output_root / item["file"]).unlink(missing_ok=True)

    # One trusted inventory: the exact two frozen Playback coefficient files,
    # plus 14 independently pinned Q0 originals with OPTIONAL actual bytes.
    pins_by_ratio = {(entry["ratioL"], entry["ratioM"]): entry
                     for entry in final_src}
    rates = (44100, 48000, 88200, 96000, 176400, 192000)
    routes = []
    for source in rates:
        for target in rates:
            if source == target:
                continue
            factor = gcd(source, target)
            ratio = (target // factor, source // factor)
            if ratio not in pins_by_ratio:
                raise ValueError(f"Unpinned SOURCE/DEVICE rate pair: {source}->{target}")
            historic = source == 44100 and target == 48000 or (
                source == 48000 and target == 44100)
            routes.append({
                "sourceRateHz": source, "targetRateHz": target,
                "ratioL": ratio[0], "ratioM": ratio[1],
                "kernelFile": (f"playback_src_kernel_{source}_{target}.bin"
                               if historic else pins_by_ratio[ratio]["file"]),
                "role": ("PLAYBACK_FROZEN" if historic else
                         "Q0_ORIGINAL_PENDING_DEVICE_QUALIFICATION"),
                "devicePlaybackAdmitted": bool(historic),
            })
    if len(routes) != 30 or len(pins_by_ratio) != 14 or sum(
            r["devicePlaybackAdmitted"] for r in routes) != 2:
        raise ValueError("SRC device route qualification inventory is inconsistent")
    manifest = {
        "schema": "rgsml.external-src-fir-assets/2",
        "installationRoot": "resources/fir",
        "assets": ([{"role": "PLAYBACK_FROZEN", **entry} for entry in entries]
                   + [{"role": "FINAL_SRC_Q0_ORIGINAL", **{k:v for k,v in entry.items() if k != "private"},
                       "packaged": originals_included}
                      for entry in final_src]),
        "routes": routes,
        "purpose": "frozen-16-src-fir-bitwise-originals",
        "releaseAuthority": False,
        "publicOriginalFirDistributionAuthorized": True,
        "complete16FilePackage": originals_included,
        "playback": entries,
        "finalSrc": {"source": "original-Q0-14-source-pinned",
                     "includedInPublicBuild": originals_included, "kernels": final_src}
    }
    if len(manifest["assets"]) != 16 or len({
            asset["file"] for asset in manifest["assets"]}) != 16:
        raise ValueError("Unified FIR catalog is not 16 unique SHA-pinned files")
    (output_root / "fir_manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--originals-zip", type=Path)
    parser.add_argument("--require-originals", action="store_true")
    args = parser.parse_args()
    export(args.source_root, args.output_root, args.originals_zip,
           args.require_originals)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, ImportError) as exc:
        sys.exit(f"EXTERNAL_FIR_BIT_EXACT_EXPORT_REFUSED: {exc}")
