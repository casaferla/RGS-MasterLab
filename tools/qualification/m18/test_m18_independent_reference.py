#!/usr/bin/env python3
"""Independent exact-arithmetic M18 preproduction oracle (NOT production).

Fractions/integers are the authority here, not C++ implementation output.
The four original September Philox development words were mathematically
falsified; use the independently corrected October values exclusively.
This finite smoke gate is not Gate G statistical/audio qualification.
"""
from __future__ import annotations
import math
import struct
import unittest
from fractions import Fraction

MASK = (1 << 32) - 1
M0, M1 = 0xD2511F53, 0xCD9E8D57
W0, W1 = 0x9E3779B9, 0xBB67AE85
SEED = 0x0123456789ABCDEF


def philox(seed: int, frame: int, channel: int) -> tuple[int, ...]:
    counter = [frame & MASK, (frame >> 32) & MASK,
               channel & MASK, 0x52475344]
    key = [seed & MASK, (seed >> 32) & MASK]
    for round_num in range(10):
        p0, p1 = counter[0] * M0, counter[2] * M1
        counter = [((p1 >> 32) ^ counter[1] ^ key[0]) & MASK,
                   p1 & MASK,
                   ((p0 >> 32) ^ counter[3] ^ key[1]) & MASK,
                   p0 & MASK]
        if round_num != 9:
            key = [(key[0] + W0) & MASK, (key[1] + W1) & MASK]
    return tuple(counter)


def independent_rne(frac: Fraction) -> int:
    numerator, denominator = frac.numerator, frac.denominator
    magnitude, remainder = divmod(abs(numerator), denominator)
    if remainder * 2 > denominator or (
        remainder * 2 == denominator and magnitude % 2
    ):
        magnitude += 1
    return -magnitude if numerator < 0 else magnitude


def independent_quantize(x: float, seed: int, frame: int,
                         channel: int) -> int:
    if not math.isfinite(x):
        raise ValueError("NONFINITE_INPUT")
    r0, r1, r2, r3 = philox(seed, frame, channel)
    z0 = (r0 << 21) | (r1 >> 11)
    z1 = (r2 << 21) | (r3 >> 11)
    d = Fraction(z0-z1, 1 << 53)
    exact = Fraction.from_float(x) * (1 << 23) + d
    # Binary64 staging: float(Fraction) implements correctly rounded RN.
    staged = Fraction.from_float(float(exact))
    q = independent_rne(staged)
    if q < -8388608 or q > 8388607:
        raise OverflowError("PCM24_OVERLOAD")
    return q


def float_from_bits(bits: int) -> float:
    return struct.unpack("<d", bits.to_bytes(8, "little"))[0]


class M18IndependentOracle(unittest.TestCase):
    def test_corrected_philox_vectors(self) -> None:
        vectors = {
            (0, 0): "A66BC7F6 B58E890B FC849318 2C57BBBA",
            (1, 0): "7A4D4CA8 A88EDC40 3EE95F3F 7732B6AE",
            (2, 0): "FA533ACE 7052D5B8 B220CF76 0AA621C3",
            (3, 0): "0399B5AB D5D94BD3 4B58FCE8 DD6CA161",
            (4294967295, 0): "C5C4CD7C 5E7543E6 6FA5F827 F3F62D3F",
            (4294967296, 0): "2C54779B 8228AEAD AEE65C73 1143BA5B",
            (4294967297, 0): "E3D1D048 47CE0F7E 7EAD1A00 5A677ED7",
        }
        for (frame, channel), expected in vectors.items():
            self.assertEqual(" ".join(f"{w:08X}" for w in
                              philox(SEED, frame, channel)), expected)

    def test_exact_fraction_pcm24_oracles(self) -> None:
        # Literal input bits, not regenerated trigonometric waveforms.
        vectors = [
            (0x0000000000000000, 0),
            (0x8000000000000000, 0),
            (0x3E70000000000000, 1),
            (0xBE70000000000000, -1),
            (0x3E80000000000000, 1),
            (0xBE80000000000000, -2),
            (0x3E60000000000000, 0),
            (0xBE60000000000000, -1),
            (0x3E70000000000000, 1),
            (0xBE70000000000000, -1),
            (0x3FEFFFFFC0000000, 8388607),
            (0xBFF0000000000000, -8388608),
            (0x3FD0000000000000, 2097152),
            (0xBFD0000000000000, -2097152),
            (0x3FEFFFFFFFFFFFFF, 8388607),
        ]
        for frame, (bits, expected) in enumerate(vectors):
            self.assertEqual(independent_quantize(
                float_from_bits(bits), SEED, frame, 0), expected)
        with self.assertRaisesRegex(OverflowError, "PCM24_OVERLOAD"):
            independent_quantize(float_from_bits(0xBFEFFFFFFFFFFFFF),
                                 SEED, 15, 0)

    def test_rn64_overload_adversary(self) -> None:
        x = float_from_bits(0x3FEFFFFFA0000000)
        d = Fraction((1 << 53) - 1, 1 << 53)
        exact = Fraction.from_float(x) * (1 << 23) + d
        # Unrounded true real sum is < 8388607.5, hence would give q max.
        self.assertLess(exact, Fraction(16777215, 2))
        staged = float(exact)
        self.assertEqual(staged, 8388607.5)
        self.assertEqual(independent_rne(Fraction.from_float(staged)), 8388608)

    def test_float32_boundary_oracles(self) -> None:
        # Independent IEEE binary32 encodings and overflow boundary.
        cases = [
            (0x0000000000000000, "00000000"),
            (0x8000000000000000, "00000080"),
            (0x36A0000000000000, "01000000"),
            (0x3690000000000000, "00000000"),
            (0x3690000000000001, "01000000"),
            (0x3810000000000000, "00008000"),
            (0x380FFFFFFFFFFFFF, "00008000"),
            (0x47EFFFFFE0000000, "ffff7f7f"),
            (0x47EFFFFFEFFFFFFF, "ffff7f7f"),
            (0xB6A0000000000000, "01000080"),
        ]
        for bits, expected in cases:
            self.assertEqual(struct.pack("<f",float_from_bits(bits)).hex(),
                             expected)
        with self.assertRaises(OverflowError):
            struct.pack("<f",float_from_bits(0x47EFFFFFF0000000))

    def test_chunk_addressing(self) -> None:
        first = (1 << 32) - 5
        full = [philox(SEED, first+i, 0) for i in range(17)]
        cursor, ragged = 0, []
        for length in (1,2,3,7,4):
            ragged.extend(philox(SEED, first+cursor+i, 0)
                          for i in range(length))
            cursor += length
        self.assertEqual(full,ragged)
        for i in range(17):
            self.assertNotEqual(full[i],philox(SEED,first+i,1))


if __name__ == "__main__":
    unittest.main(verbosity=2)
