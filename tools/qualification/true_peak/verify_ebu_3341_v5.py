#!/usr/bin/env python3
"""RGS M16 B2a: fail-closed official-EBU asset provenance and integer-oracle preflight.

No EBU WAV is stored here. All official audio must be locally supplied for
internal technical R&D, under the EBU sequence terms. © EBU.
PASS here is NOT the production M16 Gate G; comparing the production C++
meter against this independently computed evidence remains a separate gate.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import sys
import wave
import zipfile

# Independent coefficients from the Work's integer matrix; never import C++ taps.
TAPS = (
    (14, 90, -161, 272, -487, 1125, 7964, -838, 390, -218, 122, -68),
    (-239, 240, -424, 730, -1364, 3810, 6388, -1641, 832, -477, 271, -155),
    (-155, 271, -477, 832, -1641, 6388, 3810, -1364, 730, -424, 240, -239),
    (-68, 122, -218, 390, -838, 7964, 1125, -487, 272, -161, 90, 14),
)

class GatePreflightError(ValueError):
    """Missing, unexpected, corrupt, or mismatched official asset/evidence."""

def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def pcm24_stereo(raw: bytes, frames: int):
    try:
        with wave.open(io.BytesIO(raw), "rb") as w:
            fmt = (w.getnchannels(), w.getframerate(), w.getsampwidth(), w.getcomptype())
            if fmt != (2, 48000, 3, "NONE"):
                raise GatePreflightError(f"Expected stereo PCM24 48kHz, found {fmt}")
            if w.getnframes() != frames:
                raise GatePreflightError(f"Wrong frame count: {w.getnframes()} != {frames}")
            pcm = w.readframes(frames)
            if w.readframes(1):
                raise GatePreflightError("Extra undeclared WAV samples")
    except (wave.Error, EOFError) as exc:
        raise GatePreflightError(f"Unrecognized PCM WAV: {exc}") from exc
    if len(pcm) != frames * 6:
        raise GatePreflightError("Truncated WAV audio data")
    left, right, pcm_hash = [], [], hashlib.sha256()
    for p in range(0, len(pcm), 6):
        l = int.from_bytes(pcm[p:p+3], "little", signed=True)
        r = int.from_bytes(pcm[p+3:p+6], "little", signed=True)
        left.append(l)
        right.append(r)
        pcm_hash.update(struct.pack("<dd", l / (1 << 23), r / (1 << 23)))
    return left, right, pcm_hash.hexdigest()

def independent_exact_peak(samples: list[int]):
    """Max |integer numerator| and earliest j=4*i+phase, with 11 zero EOS frames."""
    if not samples:
        raise GatePreflightError("Empty official resource")
    n, maximum, first_j = len(samples), -1, -1
    for i in range(n + 11):
        for phase, taps in enumerate(TAPS):
            numerator = 0
            for lag, tap in enumerate(taps):
                k = i - lag
                if 0 <= k < n:
                    numerator += tap * samples[k]
            value = abs(numerator)
            if value > maximum:
                maximum, first_j = value, i * 4 + phase
    return maximum, first_j

def select_archive(archive: Path, manifest):
    binary = archive.read_bytes()
    if digest(binary) != manifest["archive_sha256"]:
        raise GatePreflightError("Original EBU v5 archive SHA-256 mismatch")
    with zipfile.ZipFile(io.BytesIO(binary)) as z:
        result = {}
        for case in manifest["cases"]:
            matching = [a for a in z.infolist()
                        if not a.is_dir() and Path(a.filename).name == case["filename"]]
            if len(matching) != 1:
                raise GatePreflightError(f"Case {case['case']}: expected one WAV in ZIP, got {len(matching)}")
            result[case["case"]] = z.read(matching[0])
    return result

def select_directory(directory: Path, manifest):
    result = {}
    for case in manifest["cases"]:
        path = directory / case["filename"]
        if not path.is_file():
            raise GatePreflightError(f"Case {case['case']}: missing official WAV {path}")
        result[case["case"]] = path.read_bytes()
    return result

def verify(case, raw: bytes, manifest):
    n = case["case"]
    if digest(raw) != case["wav_sha256"]:
        raise GatePreflightError(f"Case {n}: official WAV provenance SHA-256 mismatch")
    left, right, pcm_hash = pcm24_stereo(raw, case["frames"])
    if pcm_hash != case["pcm64_sha256"]:
        raise GatePreflightError(f"Case {n}: decoded PCM64 SHA-256 mismatch")
    l_peak = independent_exact_peak(left)
    r_peak = independent_exact_peak(right)
    if l_peak != (case["left_numerator"], case["left_j"]):
        raise GatePreflightError(f"Case {n}: left exact numerator/index mismatch {l_peak}")
    if r_peak != (case["right_numerator"], case["right_j"]):
        raise GatePreflightError(f"Case {n}: right exact numerator/index mismatch {r_peak}")
    value = max(l_peak[0], r_peak[0]) / manifest["exact_peak_denominator"]
    dbtp = 20.0 * math.log10(value)
    nominal = case["expected_ebu_dbtp"]
    if not nominal + manifest["official_tolerance_dbtp"]["low_offset"] <= dbtp <= nominal + manifest["official_tolerance_dbtp"]["high_offset"]:
        raise GatePreflightError(f"Case {n}: asymmetric EBU tolerance failed ({dbtp} dBTP)")
    if abs(dbtp - case["independent_preproduction_dbtp"]) > 1e-9:
        raise GatePreflightError(f"Case {n}: Work independently measured dBTP mismatch")
    return {"case": n, "frames": case["frames"], "left_numerator": l_peak[0],
            "right_numerator": r_peak[0], "left_j": l_peak[1], "right_j": r_peak[1],
            "independent_itu_dbtp": dbtp, "ebu_official_tolerance_pass": True}

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--archive", type=Path, help="Original EBU v5 ZIP, not redistributed")
    group.add_argument("--directory", type=Path, help="Nine unchanged WAVs from official archive")
    args = parser.parse_args(argv)
    try:
        manifest = json.loads(Path(__file__).with_name("ebu_3341_cases_15_23_manifest.json").read_text(encoding="utf-8"))
        if manifest["status"] != "PREPRODUCTION_PROVENANCE_ONLY__NOT_PRODUCTION_GATE_G" or [c["case"] for c in manifest["cases"]] != list(range(15, 24)):
            raise GatePreflightError("Manifest scope/status invalid")
        files = select_archive(args.archive, manifest) if args.archive else select_directory(args.directory, manifest)
        results = [verify(c, files[c["case"]], manifest) for c in manifest["cases"]]
    except (GatePreflightError, FileNotFoundError, OSError, ValueError, zipfile.BadZipFile) as exc:
        print(json.dumps({"status": "FAIL_CLOSED", "reason": str(exc),
                          "production_gate_g": "NOT_EXECUTED"}, indent=2), file=sys.stderr)
        return 1
    print(json.dumps({"status": "INDEPENDENT_OFFICIAL_ASSET_PREFLIGHT_PASS",
                      "production_gate_g": "NOT_EXECUTED", "copyright": "© EBU",
                      "results": results}, indent=2))
    return 0

if __name__ == "__main__":
    sys.exit(main())
