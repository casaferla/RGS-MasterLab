#!/usr/bin/env python3
"""Independent spectral oracle for checked-in playback SRC assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import sys
from pathlib import Path


WORD = re.compile(r"UINT64_C\(0x([0-9a-fA-F]{16})\)")


def load_bits(path: Path) -> list[int]:
    return [
        int(match, 16)
        for match in WORD.findall(path.read_text(encoding="ascii"))
    ]


def magnitude(bits: list[int], frequency_hz: float, high_rate_hz: int, gain: int) -> float:
    coefficients = [
        struct.unpack("<d", struct.pack("<Q", value))[0] for value in bits
    ]
    delay = (len(coefficients) - 1) // 2
    omega = 2.0 * math.pi * frequency_hz / high_rate_hz
    response = coefficients[delay]
    for distance in range(1, delay + 1):
        response += (
            2.0
            * coefficients[delay - distance]
            * math.cos(omega * distance)
        )
    return abs(response) / gain


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo-root", type=Path, default=Path(__file__).resolve().parents[3]
    )
    args = parser.parse_args()
    root = args.repo_root.resolve()
    manifest_path = root / "tests/audio_golden/playback_src/kernel_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schemaId") != "rgsml.playback-src.kernel-manifest/1.0.0":
        raise RuntimeError("kernel manifest schema mismatch")

    for kernel in manifest["kernels"]:
        bits = load_bits(root / kernel["coefficientAsset"])
        if len(bits) != kernel["N"]:
            raise RuntimeError("coefficient count mismatch")
        digest = hashlib.sha256(
            b"".join(struct.pack("<Q", value) for value in bits)
        ).hexdigest()
        if digest != kernel["kernelSha256"]:
            raise RuntimeError("coefficient checksum mismatch")
        high_rate = kernel["L"] * 44_100

        passband = [20_000.0 * index / 256.0 for index in range(257)]
        passband_db = [
            20.0 * math.log10(magnitude(bits, f, high_rate, kernel["L"]))
            for f in passband
        ]
        ripple = max(passband_db) - min(passband_db)

        stopband = [
            22_050.0 + (high_rate / 2.0 - 22_050.0) * index / 2048.0
            for index in range(2049)
        ]
        stopband_peak = max(
            magnitude(bits, f, high_rate, kernel["L"]) for f in stopband
        )
        attenuation = -20.0 * math.log10(
            max(stopband_peak, sys.float_info.min)
        )

        print(
            f"{kernel['fin']}->{kernel['fout']} ripple_db={ripple:.9f} "
            f"stopband_attenuation_db={attenuation:.6f} sha256={digest}"
        )
        if ripple > 0.001 or attenuation < 110.0:
            raise RuntimeError(
                "playback SRC response violates the frozen quality floor"
            )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
