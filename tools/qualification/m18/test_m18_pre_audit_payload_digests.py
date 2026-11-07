#!/usr/bin/env python3
"""Independent pre-audit SHA256 sample-payload oracle, NOT delivery Gate G.

These three hex hashes were fixed in M18 2026-10-04 independent numerical
pre-audit section I. This test rebuilds addressed Philox and RN64 PCM24
codes through the separate Python Fraction reference: it never imports
or executes the native M18 sample codec.
"""
from __future__ import annotations

import hashlib
import unittest

from test_m18_independent_reference import (
    SEED, independent_quantize, philox,
)

SCALE = 1 << 23
VECTORS = (
    (0, 257, 2,
     "2adc005bceabf3de5f7186dc5084db3abf38a5f59e6e5e00f0a0322180a65d56"),
    ((1 << 32)-1, 257, 2,
     "4d79f5f4b7032d1134b365cf9aaa7041b37319c1a0aeae4f2e73d5c44f4d9f2c"),
    ((1 << 32)-4097, 8209, 1,
     "25090a67926933541c0d2a502f98075b6c8fc344b9b047d09513af520131f447"),
)


def sample(frame: int) -> float:
    # The pre-audit's frozen source PCM is ((absoluteFrame % 7)-3)/(4*S).
    return ((frame % 7)-3) / (4*SCALE)


def encode(start: int, count: int, channels: int) -> bytes:
    data = bytearray()
    for frame in range(start, start+count):
        for channel in range(channels):
            code = independent_quantize(sample(frame), SEED, frame, channel)
            if not -(1 << 23) <= code < (1 << 23):
                raise AssertionError("unexpected overload in frozen oracle")
            data.extend((code & 0xFFFFFF).to_bytes(3, "little"))
    return bytes(data)


def partitioned(start: int, count: int, channels: int,
                partitions: tuple[int, ...]) -> bytes:
    pieces = []
    cursor = 0
    k = 0
    while cursor < count:
        n = min(partitions[k % len(partitions)], count-cursor)
        pieces.append(encode(start+cursor, n, channels))
        cursor += n
        k += 1
    return b"".join(pieces)


class M18PreAuditPayloadDigest(unittest.TestCase):
    def test_three_independently_frozen_sha256(self) -> None:
        for start, count, channels, digest in VECTORS:
            with self.subTest(start=start, channels=channels):
                self.assertEqual(len(digest), 64, "frozen SHA-256 must be 64 hex characters")
                b = encode(start, count, channels)
                self.assertEqual(len(b), count*channels*3)
                self.assertEqual(hashlib.sha256(b).hexdigest(), digest)

    def test_adversarial_chunk_partitions(self) -> None:
        partitions = (1, 2, 3, 7, 31, 64, 127, 256,
                      511, 1024, 4096, 8191)
        for start, count, channels, digest in VECTORS:
            with self.subTest(start=start, channels=channels):
                b = partitioned(start, count, channels, partitions)
                self.assertEqual(hashlib.sha256(b).hexdigest(), digest)
        self.assertEqual(
            partitioned(0, 257, 2, (1,2,3,7,31,64,127,22)),
            encode(0,257,2))

    def test_checkpoint_resume_seed_identity(self) -> None:
        for start,count,channels,_ in VECTORS:
            checkpoint = 113 if count == 257 else 4097
            self.assertEqual(
                encode(start,checkpoint,channels)
                + encode(start+checkpoint,count-checkpoint,channels),
                encode(start,count,channels))

    def test_channel_address_metamorphism(self) -> None:
        for frame in range(257):
            # Channel independence is word-address uniqueness, NOT a promise
            # that every rounded PCM code or projected D must differ.
            self.assertNotEqual(philox(SEED,frame,0),philox(SEED,frame,1))
        mono=encode(0,257,1)
        stereo=encode(0,257,2)
        self.assertEqual(mono,b"".join(
            stereo[i:i+3] for i in range(0,len(stereo),6)))


if __name__ == "__main__":
    unittest.main(verbosity=2)
