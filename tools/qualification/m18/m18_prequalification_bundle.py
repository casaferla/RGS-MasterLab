#!/usr/bin/env python3
"""Create audited, downloadable prequalification audio artifact: NOT M18 FQ.

The seven existing ultra-low-level WAVs are primarily numerical and typically
inaudible. The four new synthetic tonal WAVs are for exploration/listening.
All 11 are produced via M18's actual isolated C++ terminal-file exporter.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess

from test_m18_audio_pathology import inspect_one
from verify_m18_wav_candidate import audit_file

LOW_NAMES = {
    "sine_025lsb": 2, "sine_2lsb": 1, "fade_16lsb": 1,
    "sparse_tail": 1, "low_frequency_decay": 1,
    "identical_stereo_ambience": 1,
}


def run_native(binary: str, folder: Path, prefix: str, marker: str) -> list[list[str]]:
    p = subprocess.run([binary, "--emit-fixtures", str(folder)],
                       capture_output=True, text=True, check=True, timeout=120)
    if marker not in p.stdout:
        raise ValueError(f"missing native completion marker {marker}")
    rows = [line.split("\t")[1:] for line in p.stdout.splitlines()
            if line.startswith(prefix + "\t")]
    return rows


def signed_pcm24(payload: bytes) -> list[float]:
    return [int.from_bytes(payload[i:i+3], "little", signed=True) * 2 ** -23
            for i in range(0, len(payload), 3)]


def check_audible_pair(folder: Path, rate: int,
                        pcm_row: list[str], f32_row: list[str]) -> dict:
    def read_row(row: list[str]) -> list[float]:
        filename, encoding, hz, channels, frames, fullsha, payloadsha = row
        if filename != f"m18-synthetic-audible-{encoding}-{rate}-stereo.wav":
            raise ValueError("unexpected synthetic filename")
        if (int(hz), int(channels), int(frames)) != (rate, 2, 5 * rate):
            raise ValueError("invalid synthetic geometry")
        info = audit_file(folder / filename,
                          expected_encoding=("PCM24" if encoding == "pcm24" else "FLOAT32"),
                          expected_rate=rate, expected_channels=2,
                          expected_frames=5 * rate,
                          expected_file_sha256=fullsha,
                          expected_payload_sha256=payloadsha)
        data = (folder / filename).read_bytes()[info["header_bytes"]:]
        if encoding == "pcm24":
            return signed_pcm24(data)
        if encoding == "float32":
            return [x[0] for x in struct.iter_unpack("<f", data)]
        raise ValueError("unexpected output format")

    pcm, f32 = read_row(pcm_row), read_row(f32_row)
    if len(pcm) != len(f32) or len(pcm) != 10 * rate:
        raise ValueError("mismatched decoded frame count")
    max_gap = max(abs(a-b) for a, b in zip(pcm, f32))
    if max_gap > 2.8e-7:
        raise ValueError(f"unexpected PCM24/Float32 mismatch {max_gap}")
    rms = math.sqrt(math.fsum(x*x for x in f32)/len(f32))
    peak = max(abs(x) for x in f32)
    if not (0.035 < rms < 0.45 and 0.12 < peak < 0.65):
        raise ValueError(f"audible test signal out of level window {rms} {peak}")
    last_half_second = f32[-rate:]
    last_rms = math.sqrt(math.fsum(x*x for x in last_half_second)/len(last_half_second))
    if not (0 < last_rms < rms/2):
        raise ValueError("audible fixture fade invalid")
    return {"rate_hz": rate, "rms": rms, "peak": peak,
            "last_half_second_rms": last_rms,
            "max_pcm24_float32_delta": max_gap}


def build_bundle(low_exe: str, audible_exe: str,
                 output: Path, source_sha: str) -> None:
    if output.exists():
        if any(output.iterdir()):
            raise ValueError("output folder must be empty")
    else:
        output.mkdir(parents=True)
    low = run_native(low_exe, output, "M18_AUDIO",
                     "M18_SEVEN_LOWLEVEL_WAV_PROGRAMS_PASS_NOT_LISTENING_FQ")
    audible = run_native(audible_exe, output, "M18_LISTEN",
                         "M18_FOUR_AUDIBLE_SYNTHETIC_PREQUAL_WAV_PASS_NOT_LISTENING_GATE")
    if len(low) != 7 or len(audible) != 4:
        raise ValueError("incorrect generated WAV count")
    observations = [inspect_one(output, row) for row in low]
    for name, count in LOW_NAMES.items():
        if sum(row[1] == name for row in low) != count:
            raise ValueError(f"missing low-level fixture {name}")
    # Existing frozen-class sine signal is rate-independent in quantization.
    lows = [row for row in low if row[1] == "sine_025lsb"]
    if lows[0][6] != lows[1][6]:
        raise ValueError("low sine Master-rate-invariant payload violation")
    result_pairs = []
    for rate in (44100, 48000):
        pair = [row for row in audible if row[2] == str(rate)]
        if len(pair) != 2 or {row[1] for row in pair} != {"pcm24", "float32"}:
            raise ValueError("missing synthetic output encoding pair")
        result_pairs.append(check_audible_pair(output, rate,
            next(row for row in pair if row[1] == "pcm24"),
            next(row for row in pair if row[1] == "float32")))
    expected_files = {row[0] for row in low + audible}
    if len(expected_files) != 11:
        raise ValueError("duplicate fixture filenames")
    if {p.name for p in output.iterdir()} != expected_files:
        raise ValueError("unapproved or missing file in artifact folder")
    entries = []
    for row in sorted(low + audible):
        filename, purpose, rate, channels, frames, sha, payloadsha = row
        raw = (output / filename).read_bytes()
        if hashlib.sha256(raw).hexdigest() != sha:
            raise ValueError("full-file SHA mismatch")
        entries.append({"filename": filename, "purpose": purpose,
                        "sample_rate_hz": int(rate), "channels": int(channels),
                        "frames": int(frames), "full_file_sha256": sha,
                        "payload_sha256": payloadsha, "bytes": len(raw)})
    manifest = {"status": "PREQUALIFICATION_ONLY_NOT_FQ",
                "source_sha": source_sha, "sample_count": 11,
                "files": entries, "independent_lowlevel_checks": observations,
                "independent_audible_checks": result_pairs}
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (output / "SHA256SUMS.txt").write_text(
        "".join(f"{entry['full_file_sha256']}  {entry['filename']}\n"
                for entry in entries), encoding="utf-8")
    (output / "README.txt").write_text(
        "RGS MasterLab M18: PREQUALIFICATION ONLY - NO PRODUCT FQ\n\n"
        "Synthetic diagnostic signals; no copyrighted music; no official listening gate.\n"
        "Seven sine/fade/tail/ambience WAVs are at microscopic (<1LSB) or\n"
        "very low PCM24 levels and are not intended for ordinary listening.\n"
        "DO NOT turn headphones or amplifier to extreme volume in pursuit\n"
        "of hearing dither; use numerical analyzers for these seven files.\n\n"
        "Four m18-synthetic-audible-* stereo WAVs are 5-second, original\n"
        "audible tonal textures with a fade to silence at 44.1/48k; compare\n"
        "PCM24 with Float32 using matched playback gain (NOT a null-test\n"
        "certificate; any audible difference might be playback dependent).\n"
        "No M17 limiter, Gold, SOURCE conversion, Qt, Windows production\n"
        "transaction or final project persistence is qualified by this pack.\n"
        "manifest.json and SHA256SUMS.txt allow reproducible verification.\n",
        encoding="utf-8")
    print("M18_11_REAL_WAV_PREQUALIFICATION_BUNDLE_PASS_NOT_PRODUCT_FQ")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--low-exe", required=True)
    parser.add_argument("--audible-exe", required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--source-sha", required=True)
    args = parser.parse_args()
    build_bundle(args.low_exe, args.audible_exe, args.output_dir, args.source_sha)


if __name__ == "__main__":
    main()