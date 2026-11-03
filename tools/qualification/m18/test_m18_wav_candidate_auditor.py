#!/usr/bin/env python3
"""Bounded positive and adversarial tests for independent M18 WAV auditor."""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_m18_wav_candidate import (CandidateError, audit_file,
                                       parse_canonical_header)


def pcm24(frames=3, rate=48000, channels=2):
    payload = (b"\0\0\0\xff\xff\x7f" * frames)[:frames * channels * 3]
    align = channels * 3
    return (b"RIFF" + struct.pack("<I", 36 + len(payload)) + b"WAVE"
            + b"fmt " + struct.pack("<IHHIIHH", 16, 1, channels, rate,
                                   rate * align, align, 24)
            + b"data" + struct.pack("<I", len(payload)) + payload)


def float32(frames=2, rate=192000, channels=2):
    payload = struct.pack("<4f", 0.0, -0.0, 1.25, -1.25)
    assert frames * channels == 4
    align = channels * 4
    return (b"RIFF" + struct.pack("<I", 50 + len(payload)) + b"WAVE"
            + b"fmt " + struct.pack("<IHHIIHHH", 18, 3, channels, rate,
                                   rate * align, align, 32, 0)
            + b"fact" + struct.pack("<II", 4, frames)
            + b"data" + struct.pack("<I", len(payload)) + payload)


def rf64_header(frames=715827883, rate=192000, channels=2):
    align = channels * 3
    payload_size = frames * align
    return (b"RF64" + struct.pack("<I", 0xffffffff) + b"WAVE"
            + b"ds64" + struct.pack("<IQQQI", 28, payload_size + 72,
                                   payload_size, frames, 0)
            + b"fmt " + struct.pack("<IHHIIHH", 16, 1, channels, rate,
                                   rate * align, align, 24)
            + b"data" + struct.pack("<I", 0xffffffff))


class TestAuditor(unittest.TestCase):
    def test_pcm24_integrity(self):
        data = pcm24()
        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder) / "a.wav"
            file.write_bytes(data)
            result = audit_file(file, expected_encoding="PCM24",
                                expected_rate=48000, expected_channels=2,
                                expected_frames=3,
                                expected_file_sha256=hashlib.sha256(data).hexdigest(),
                                expected_payload_sha256=hashlib.sha256(data[44:]).hexdigest())
            self.assertEqual(result["file_bytes"], len(data))
            self.assertEqual(result["payload_bytes"], 18)
            with self.assertRaisesRegex(CandidateError, "SHA256 mismatch"):
                audit_file(file, expected_file_sha256="0" * 64)
            with self.assertRaisesRegex(CandidateError, "rate mismatch"):
                audit_file(file, expected_rate=44100)

    def test_float32_signed_zero_and_finite_outside_unity(self):
        data = float32()
        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder) / "f.wav"
            file.write_bytes(data)
            self.assertEqual(audit_file(file)["encoding"], "FLOAT32")
            for bad in (0x7f800000, 0xff800000, 0x7fc00001):
                changed = bytearray(data)
                changed[58:62] = struct.pack("<I", bad)
                file.write_bytes(changed)
                with self.assertRaisesRegex(CandidateError, "nonfinite"):
                    audit_file(file)

    def test_rf64_header_without_allocating_four_gigabytes(self):
        header = rf64_header()
        g = parse_canonical_header(header)
        self.assertEqual(g.container, "RF64")
        self.assertEqual(g.payload_bytes, 4294967298)
        self.assertEqual(g.header_bytes, 80)
        with self.assertRaisesRegex(CandidateError, "incomplete/extra"):
            parse_canonical_header(header, actual_file_size=len(header))

    def test_fail_closed_truncation_trailing_and_fields(self):
        data = pcm24()
        cases = [data[:-1], data + b"\0", b"NOT!" + data[4:],
                 data[:22] + b"\0" + data[23:],
                 data[:40] + b"\0" + data[41:]]
        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder) / "broken.wav"
            for value in cases:
                file.write_bytes(value)
                with self.assertRaises(CandidateError):
                    audit_file(file)

    def test_float32_bad_fact_and_bad_payload_shape(self):
        original = float32()
        for offset in (38, 42, 46, 54):
            bad = bytearray(original)
            bad[offset] ^= 1
            with self.assertRaises(CandidateError):
                parse_canonical_header(bad, len(bad))

    def test_invalid_sha_arg(self):
        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder) / "a.wav"
            file.write_bytes(pcm24())
            with self.assertRaisesRegex(CandidateError, "digest format"):
                audit_file(file, expected_file_sha256="deadbeef")


if __name__ == "__main__":
    unittest.main(verbosity=2)
