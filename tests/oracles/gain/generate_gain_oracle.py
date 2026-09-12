#!/usr/bin/env python3
"""Development-only independent Gain-v1 MPFR oracle; not run by CMake."""

import struct

import gmpy2


INPUT_BITS = (
    0x3FE0000000000000,
    0xBFD0000000000000,
    0x3FF4000000000000,
    0xC000000000000000,
    0x0000000000000010,
    0x8000000000000010,
)
GAINS_DB = (6, -12, -24, 24)


def from_bits(bits: int) -> float:
    return struct.unpack(">d", bits.to_bytes(8, "big"))[0]


def to_bits(value: gmpy2.mpfr) -> int:
    return int.from_bytes(struct.pack(">d", float(value)), "big")


def main() -> None:
    gmpy2.get_context().precision = 256
    print(f"gmpy2={gmpy2.version()} MPFR={gmpy2.mpfr_version()}")
    for gain_db in GAINS_DB:
        factor = gmpy2.exp10(gmpy2.mpfr(gain_db) / 20)
        print(f"gain={gain_db:+d} factor=0x{to_bits(factor):016x}")
        for input_bits in INPUT_BITS:
            output = gmpy2.mpfr(from_bits(input_bits)) * factor
            print(f"  0x{input_bits:016x} -> 0x{to_bits(output):016x}")


if __name__ == "__main__":
    main()
