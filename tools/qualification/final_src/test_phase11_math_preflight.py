#!/usr/bin/env python3
"""Executable Phase-11 Q0 math/asset-header regression; never an SRC release gate."""
import hashlib
from fractions import Fraction
from math import gcd
import struct
import unittest

from phase11_math_preflight import (
    ALGORITHM, HEADER, MAGIC, MAX_I64, RATES, ContractError, Ratio,
    above_new_nyquist, atan_bounds, audit_rgsmsr1_structure, ceil_div_signed,
    even_reflect, floor_div_signed, floor_mod, kernel_shape, mapped_frame,
    mapped_interval, next_even, pi_bounds, plan,
)


class Phase11Q0MathTests(unittest.TestCase):
    def test_pi_certified_machin_enclosure(self):
        lo, hi = pi_bounds()
        self.assertLess(lo, hi)
        self.assertLess(hi - lo, Fraction(1, 10 ** 35))
        self.assertLess(lo, Fraction(22, 7))
        self.assertGreater(hi, Fraction(333, 106))
        for denominator in (5, 239):
            a, b = atan_bounds(denominator)
            self.assertLess(a, b)

    def test_signed_div_mod_and_absolute_half_open_map(self):
        for divisor in (1, 2, 7, 147, 160, 640):
            for a in (-1031, -640, -1, 0, 1, 147, 1031):
                q = floor_div_signed(a, divisor)
                c = ceil_div_signed(a, divisor)
                r = floor_mod(a, divisor)
                self.assertEqual(q * divisor + r, a)
                self.assertGreaterEqual(r, 0)
                self.assertLess(r, divisor)
                self.assertEqual(c, -floor_div_signed(-a, divisor))
                self.assertEqual(c, q + (r != 0))
        self.assertEqual(next_even(0), 0)
        self.assertEqual(next_even(512), 512)
        self.assertEqual(next_even(513), 514)
        with self.assertRaises(ContractError):
            next_even(-1)
        with self.assertRaises(ContractError):
            floor_div_signed(1, 0)
        for fin in RATES:
            for fout in RATES:
                ratio = Ratio.make(fin, fout)
                self.assertEqual(gcd(ratio.L, ratio.M), 1)
                self.assertEqual(ratio.L * fin, ratio.M * fout)
                for start in (-640, -147, -1, 0, 1, 31, 256):
                    for delta in (0, 1, 7, 511):
                        end = start + delta
                        mapped = mapped_interval(start, end, ratio)
                        exact_start = Fraction(start * ratio.L, ratio.M)
                        exact_end = Fraction(end * ratio.L, ratio.M)
                        self.assertEqual(mapped, (
                            -((-exact_start.numerator) // exact_start.denominator),
                            -((-exact_end.numerator) // exact_end.denominator)))
                        self.assertLessEqual(mapped[0], mapped[1])
        self.assertEqual(mapped_frame(-1, Ratio.make(44100, 192000)), -4)
        with self.assertRaises(ContractError):
            mapped_frame(MAX_I64, Ratio.make(44100, 192000))
        with self.assertRaises(ContractError):
            mapped_interval(2, 1, Ratio.make(44100, 44100))
        with self.assertRaises(ContractError):
            Ratio.make(352800, 44100)

    def test_kernel_shape_exact_order_and_canonical_fixed_anchors(self):
        # Independent frozen-ratio anchor expectations; NOT coefficient hashes.
        expected = {
            (160, 147): (512, 81921), (147, 160): (540, 79381),
            (320, 147): (512, 163841), (147, 320): (1078, 158467),
            (640, 147): (512, 327681), (147, 640): (2154, 316639),
            (80, 147): (910, 72801), (147, 80): (512, 75265),
            (40, 147): (1818, 72721), (147, 40): (512, 75265),
            (2, 1): (512, 1025), (1, 2): (990, 991),
            (4, 1): (512, 2049), (1, 4): (1980, 1981),
        }
        observed = {}
        for fin in RATES:
            for fout in RATES:
                if fin == fout:
                    with self.assertRaises(ContractError):
                        kernel_shape(Ratio.make(fin, fout))
                    continue
                ratio = Ratio.make(fin, fout)
                shape = kernel_shape(ratio)
                observed[(shape.L, shape.M)] = (shape.T, shape.N)
                self.assertEqual(shape.N, shape.L * shape.T + 1)
                self.assertEqual(shape.D, (shape.N - 1) // 2)
                self.assertEqual(shape.T % 2, 0)
                self.assertEqual(shape.frozen_bytes, HEADER.size + 8 * shape.N)
        self.assertEqual(observed, expected)
        self.assertEqual(sum(HEADER.size + 8 * n for _, n in expected.values()),
                         11440952)  # 14 * 52-byte headers + 11,440,224 payload

    def test_exact_all_15_alias_probes(self):
        expected_count = 0
        for fin in RATES:
            for fout in RATES:
                if fin <= fout:
                    with self.assertRaises(ContractError):
                        above_new_nyquist(fin, fout)
                    continue
                name, frequency = above_new_nyquist(fin, fout)
                self.assertEqual(frequency, Fraction(fin + fout, 4))
                self.assertEqual(name, f"rgsml.fx.reg.pipeline.src."
                                       f"{fin}_to_{fout}.above_new_nyquist.v1")
                self.assertTrue(Fraction(fout, 2) < frequency
                                < Fraction(fin, 2))
                expected_count += 1
        self.assertEqual(expected_count, 15)
        report = plan()
        self.assertEqual(len(report["pairs"]), 36)
        self.assertEqual(len(report["assets"]), 14)
        self.assertEqual(len(report["above_new_nyquist"]), 15)
        self.assertEqual(report["contract"], ALGORITHM)
        self.assertIn("NOT_RELEASE_QUALIFIED", report["status"])
        self.assertTrue(all(x["kernel_sha256"] is None
                            for x in report["assets"]))

    def test_duplicate_endpoint_reflection_not_legacy_reflect101(self):
        self.assertEqual([even_reflect(n, 3) for n in range(-7, 11)],
                         [0, 0, 1, 2, 2, 1, 0, 0, 1, 2, 2, 1, 0, 0, 1, 2, 2, 1])
        for n in range(-100, 101):
            self.assertEqual(even_reflect(n, 1), 0)
            self.assertTrue(0 <= even_reflect(n, 7) < 7)
        with self.assertRaises(ContractError):
            even_reflect(0, 0)

    def test_rgsmsr1_header_parser_is_structural_not_numeric_proof(self):
        shape = kernel_shape(Ratio.make(48000, 96000))
        h = HEADER.pack(MAGIC, 1, 1, shape.L, shape.M, shape.T, 0,
                        shape.N, shape.N, 8 * shape.N)
        # Zero coefficients are NOT a valid lowpass: acceptance here proves
        # only the separate HEADER/symmetry audit, never math/Q1-Q6 status.
        data = h + (b"\x00" * (8 * shape.N))
        self.assertEqual(audit_rgsmsr1_structure(data, shape),
                         hashlib.sha256(data).hexdigest())
        with self.assertRaises(ContractError):
            audit_rgsmsr1_structure(data[:-1], shape)
        broken = bytearray(data)
        broken[HEADER.size] = 1
        with self.assertRaises(ContractError):
            audit_rgsmsr1_structure(bytes(broken), shape)
        broken = bytearray(data)
        struct.pack_into("<d", broken, HEADER.size + 8 * (shape.N // 2),
                         float("inf"))
        with self.assertRaises(ContractError):
            audit_rgsmsr1_structure(bytes(broken), shape)
        broken = bytearray(data)
        broken[8] = 2
        with self.assertRaises(ContractError):
            audit_rgsmsr1_structure(bytes(broken), shape)


if __name__ == "__main__":
    unittest.main(verbosity=2)
