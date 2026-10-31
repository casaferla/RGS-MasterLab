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
    pins = re.findall(r'\\{(\\d+),(\\d+),"([a-f0-9]{64})"\\}',
                      pins_source.read_text(encoding="utf-8"))
    if len(pins) != 14 or len(set((L, M) for L, M, _ in pins)) != 14:
        raise ValueError("Original 14-Q0 trusted FIR pin inventory changed")
    final_src = [
        {"file": f"RGSMSR1_L{L}_M{M}.bin", "ratioL": int(L),
         "ratioM": int(M), "sha256": digest, "private": True}
        for L, M, digest in pins
    ]
    manifest = {
        "schema": "rgsml.external-src-fir-assets/1",
        "purpose": "frozen-playback-fir-bitwise-migration",
        "releaseAuthority": False,
        "playback": entries,
        "finalSrc": {"source": "original-private-Q0-14-source-pinned",
                     "includedInPublicBuild": False, "kernels": final_src}
    }
    (output_root / "playback_fir_manifest.json").write_text(
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
