#!/usr/bin/env python3
"""Bit-exact externalization of the TWO frozen playback FIR coefficient arrays.

This is a BUILD-TIME converter, not a generator: it copies the already frozen
uint64 binary64 words into little-endian resources without MPFR, recomputation
or filtering changes. No Python is needed on an end-user Windows installation.
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


def export(source_root: Path, output_root: Path) -> None:
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
    # Trust-anchor inventory only: the original 14 private FIR BYTES never
    # enter the public build. Import their already-public fixed SHA identities
    # to keep ONE total 16-FIR application resource inventory.
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
    # One trusted inventory: the exact two frozen Playback coefficient files,
    # plus 14 pinned Q0 original PRIVATE identities (NO private coefficient bytes).
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
                   + [{"role": "FINAL_SRC_Q0_PRIVATE_NONRELEASE", **entry}
                      for entry in final_src]),
        "routes": routes,
        "purpose": "frozen-playback-fir-bitwise-migration",
        "releaseAuthority": False,
        "playback": entries,
        "finalSrc": {"source": "original-private-Q0-14-source-pinned",
                     "includedInPublicBuild": False, "kernels": final_src}
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
    args = parser.parse_args()
    export(args.source_root, args.output_root)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as exc:
        sys.exit(f"EXTERNAL_FIR_BIT_EXACT_EXPORT_REFUSED: {exc}")
