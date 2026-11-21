#!/usr/bin/env python3
"""Independent, read-only auditor for isolated M18 WAV delivery candidates.

Qualification tooling only: NOT a publisher, M16 true-peak analyzer, formal
Gate G, or authority to integrate M18 before accepted M17 + CI reset.
"""
from __future__ import annotations
import argparse
from dataclasses import asdict, dataclass
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

RATES = frozenset((44100, 48000, 88200, 96000, 176400, 192000))
MAX_U32 = (1 << 32) - 1
MAX_I64 = (1 << 63) - 1
SHA256_PATTERN = re.compile(r"[0-9a-fA-F]{64}\Z")


class CandidateError(ValueError):
    """Noncanonical, incomplete or invalid candidate: fail closed."""


@dataclass(frozen=True)
class CandidateGeometry:
    encoding: str
    container: str
    sample_rate: int
    channels: int
    frame_count: int
    header_bytes: int
    payload_bytes: int
    file_bytes: int


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise CandidateError(message)


def _u32(data: bytes, offset: int) -> int:
    _require(len(data) >= offset + 4, "truncated u32 field")
    return struct.unpack_from("<I", data, offset)[0]


def parse_canonical_header(data: bytes, actual_file_size: int | None = None) -> CandidateGeometry:
    """Validate exact M18 RIFF/RF64 geometry without decoding the payload."""
    _require(len(data) >= 12, "truncated RIFF header")
    rf64 = data[:4] == b"RF64"
    _require(rf64 or data[:4] == b"RIFF", "not a RIFF/RF64 candidate")
    _require(data[8:12] == b"WAVE", "missing WAVE marker")
    pos = 12
    ds64_frame_count = ds64_data_bytes = ds64_riff_bytes = None
    if rf64:
        _require(_u32(data, 4) == MAX_U32, "RF64 RIFF sentinel missing")
        _require(len(data) >= pos + 36, "truncated ds64 chunk")
        _require(data[pos:pos + 4] == b"ds64", "missing RF64 ds64")
        _require(_u32(data, pos + 4) == 28, "unexpected ds64 size")
        ds64_riff_bytes, ds64_data_bytes, ds64_frame_count, table_length = (
            struct.unpack_from("<QQQI", data, pos + 8))
        _require(table_length == 0, "unsupported ds64 table")
        pos += 36
    _require(len(data) >= pos + 24, "truncated fmt chunk")
    _require(data[pos:pos + 4] == b"fmt ", "missing canonical fmt chunk")
    fmt_size = _u32(data, pos + 4)
    _require(fmt_size in (16, 18), "unexpected fmt length")
    _require(len(data) >= pos + 8 + fmt_size, "truncated fmt body")
    audio_tag, channels, rate, byte_rate, align, depth = (
        struct.unpack_from("<HHIIHH", data, pos + 8))
    encoding = "PCM24" if audio_tag == 1 and depth == 24 else (
        "FLOAT32" if audio_tag == 3 and depth == 32 else "")
    _require(bool(encoding), "unsupported WAV sample representation")
    _require(fmt_size == (16 if encoding == "PCM24" else 18),
             "unexpected fmt size for encoding")
    _require(channels in (1, 2), "unsupported channel layout")
    _require(rate in RATES, "unsupported output sample rate")
    bytes_per_sample = 3 if encoding == "PCM24" else 4
    _require(align == channels * bytes_per_sample, "bad block alignment")
    _require(byte_rate == rate * align, "bad byte rate")
    if encoding == "FLOAT32":
        _require(struct.unpack_from("<H", data, pos + 24)[0] == 0,
                 "noncanonical Float32 cbSize")
    pos += 8 + fmt_size
    fact_count = None
    if encoding == "FLOAT32":
        _require(len(data) >= pos + 12, "truncated Float32 fact")
        _require(data[pos:pos + 4] == b"fact" and _u32(data, pos + 4) == 4,
                 "missing/noncanonical Float32 fact")
        fact_count = _u32(data, pos + 8)
        pos += 12
    _require(len(data) >= pos + 8, "truncated data chunk header")
    _require(data[pos:pos + 4] == b"data", "missing canonical data chunk")
    data_field = _u32(data, pos + 4)
    pos += 8
    header_size = pos
    expected_header = ({"PCM24": 80, "FLOAT32": 94}[encoding]
                       if rf64 else {"PCM24": 44, "FLOAT32": 58}[encoding])
    _require(header_size == expected_header, "unexpected canonical header length")
    if rf64:
        _require(data_field == MAX_U32, "RF64 data sentinel missing")
        assert ds64_data_bytes is not None and ds64_frame_count is not None
        assert ds64_riff_bytes is not None
        payload_bytes = ds64_data_bytes
        frames = ds64_frame_count
        riff_header = 44 if encoding == "PCM24" else 58
        _require(payload_bytes > MAX_U32 - 1 - (riff_header - 8),
                 "RF64 used below canonical RIFF threshold")
        _require(ds64_riff_bytes == payload_bytes + header_size - 8,
                 "RF64 ds64 RIFF size mismatch")
    else:
        payload_bytes = data_field
        _require(payload_bytes <= MAX_U32 - 1 - (header_size - 8),
                 "RIFF exceeds canonical size transition")
        _require(_u32(data, 4) == payload_bytes + header_size - 8,
                 "RIFF size mismatch")
        _require(payload_bytes % align == 0, "fractional audio frame")
        frames = payload_bytes // align
    _require(frames <= MAX_I64, "frame count exceeds M18 plan maximum")
    _require(payload_bytes == frames * align, "payload/frame-count mismatch")
    if encoding == "FLOAT32":
        _require(fact_count == min(frames, MAX_U32), "Float32 fact count mismatch")
    size = header_size + payload_bytes
    _require(size <= (1 << 64) - 1, "file size overflow")
    if actual_file_size is not None:
        _require(actual_file_size == size, "incomplete/extra bytes in WAV file")
    return CandidateGeometry(encoding, "RF64" if rf64 else "RIFF", rate,
                             channels, frames, header_size, payload_bytes, size)


def _digest_arg(value: str | None) -> str | None:
    if value is None:
        return None
    _require(bool(SHA256_PATTERN.fullmatch(value)), "invalid SHA-256 digest format")
    return value.lower()


def audit_file(path: Path, *, expected_encoding: str | None = None,
               expected_rate: int | None = None,
               expected_channels: int | None = None,
               expected_frames: int | None = None,
               expected_file_sha256: str | None = None,
               expected_payload_sha256: str | None = None) -> dict:
    """Read/verify a complete candidate; do not alter or publish it."""
    try:
        with path.open("rb") as stream:
            size = path.stat().st_size
            header = stream.read(94)  # Max exact header, bounded RAM.
            geometry = parse_canonical_header(header, size)
            expectations = ((expected_encoding, geometry.encoding, "encoding"),
                            (expected_rate, geometry.sample_rate, "rate"),
                            (expected_channels, geometry.channels, "channels"),
                            (expected_frames, geometry.frame_count, "frames"))
            for want, found, name in expectations:
                _require(want is None or want == found, f"{name} mismatch")
            full_sha = hashlib.sha256()
            payload_sha = hashlib.sha256()
            full_sha.update(header[:geometry.header_bytes])
            stream.seek(geometry.header_bytes)
            remaining = geometry.payload_bytes
            incomplete_float = b""
            while remaining:
                chunk = stream.read(min(256 * 1024, remaining))
                _require(bool(chunk), "unexpected payload EOF")
                remaining -= len(chunk)
                full_sha.update(chunk)
                payload_sha.update(chunk)
                if geometry.encoding == "FLOAT32":
                    chunk = incomplete_float + chunk
                    complete = len(chunk) - len(chunk) % 4
                    for (word,) in struct.iter_unpack("<I", chunk[:complete]):
                        _require((word & 0x7F800000) != 0x7F800000,
                                 "nonfinite Float32 payload")
                    incomplete_float = chunk[complete:]
            _require(not incomplete_float, "partial Float32 word")
            _require(stream.read(1) == b"", "trailing bytes")
            full_hex, payload_hex = full_sha.hexdigest(), payload_sha.hexdigest()
            _require(_digest_arg(expected_file_sha256) in (None, full_hex),
                     "full-file SHA256 mismatch")
            _require(_digest_arg(expected_payload_sha256) in (None, payload_hex),
                     "payload SHA256 mismatch")
            return {**asdict(geometry), "full_file_sha256": full_hex,
                    "payload_sha256": payload_hex,
                    "scope": "independent-preproduction-structure-hash-finite-only"}
    except OSError as error:
        raise CandidateError(f"file access failed: {error.strerror}") from error


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    parser.add_argument("--expect-encoding", choices=("PCM24", "FLOAT32"))
    parser.add_argument("--expect-rate", type=int)
    parser.add_argument("--expect-channels", type=int)
    parser.add_argument("--expect-frames", type=int)
    parser.add_argument("--expect-file-sha256")
    parser.add_argument("--expect-payload-sha256")
    args = parser.parse_args(argv)
    try:
        print(json.dumps(audit_file(args.file,
            expected_encoding=args.expect_encoding,
            expected_rate=args.expect_rate,
            expected_channels=args.expect_channels,
            expected_frames=args.expect_frames,
            expected_file_sha256=args.expect_file_sha256,
            expected_payload_sha256=args.expect_payload_sha256), sort_keys=True))
    except CandidateError as error:
        print(f"M18_CANDIDATE_REJECTED: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
