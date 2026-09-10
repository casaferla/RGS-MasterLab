#!/usr/bin/env python3
"""Create deterministic, low-level stereo PCM16 listening fixtures."""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
import wave
from pathlib import Path


def sample(frame: int, rate: int, channel: int) -> int:
    time = frame / rate
    level = 0.06309573444801933  # -24 dBFS peak
    value = 0.0
    if 0.50 <= time < 1.50 and channel == 0:
        value = level * math.sin(2.0 * math.pi * 440.0 * time)
    elif 2.00 <= time < 3.00 and channel == 1:
        value = level * math.sin(2.0 * math.pi * 660.0 * time)
    elif 3.50 <= time < 6.50:
        frequency = 440.0 if channel == 0 else 660.0
        value = level * math.sin(2.0 * math.pi * frequency * time)
    return max(-32768, min(32767, round(value * 32767.0)))


def generate(path: Path, rate: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    frames = rate * 7
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(rate)
        block = bytearray()
        for frame in range(frames):
            block += struct.pack("<hh", sample(frame, rate, 0), sample(frame, rate, 1))
        output.writeframes(block)
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    print(f"{path.as_posix()} rate={rate} frames={frames} sha256={digest}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo-root", type=Path, default=Path(__file__).resolve().parents[3]
    )
    args = parser.parse_args()
    root = args.repo_root.resolve()
    destination = root / "tests/audio_golden/playback_src"
    generate(destination / "listening_stereo_44100_pcm16.wav", 44_100)
    generate(destination / "listening_stereo_48000_pcm16.wav", 48_000)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
