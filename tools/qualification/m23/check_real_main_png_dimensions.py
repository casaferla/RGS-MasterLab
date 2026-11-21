#!/usr/bin/env python3
"""Fail closed on mislabeled Product Main visual evidence (not a release gate)."""
from __future__ import annotations

import pathlib
import struct
import sys

STEMS = ("m19-levels", "m20-tonal-stereo", "m21-metrics-delta")
SIZES = ((1184, 688), (1440, 900))
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def audit(directory: pathlib.Path) -> None:
    expected = {
        f"PRODUCT_REAL_MAIN_{stem}_{width}x{height}_UNAVAILABLE.png": (width, height)
        for stem in STEMS
        for width, height in SIZES
    }
    actual = {entry.name for entry in directory.glob("PRODUCT_REAL_MAIN_*_UNAVAILABLE.png")}
    if actual != set(expected):
        raise ValueError(
            f"Expected exactly six identity-specific PNGs. "
            f"Missing={sorted(set(expected)-actual)} extra={sorted(actual-set(expected))}"
        )
    for name, requested_size in expected.items():
        path = directory / name
        with path.open("rb") as file:
            header = file.read(24)
        if len(header) != 24 or header[:8] != PNG_SIGNATURE:
            raise ValueError(f"Not a complete PNG IHDR: {name}")
        actual_size = struct.unpack(">II", header[16:24])
        if actual_size != requested_size:
            raise ValueError(
                f"Mislabelled native Product Main PNG {name}: "
                f"requested={requested_size}, actual={actual_size}"
            )


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("USAGE: check_real_main_png_dimensions.py DIRECTORY")
    try:
        audit(pathlib.Path(sys.argv[1]))
    except (OSError, ValueError) as exc:
        print(f"FAIL_CLOSED_REAL_MAIN_PNG: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
    print("PASS_EXACT_SIX_REAL_MAIN_PNG_IDENTITIES_AND_PIXEL_DIMENSIONS_NON_FQ")
