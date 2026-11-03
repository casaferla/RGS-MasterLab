#!/usr/bin/env python3
"""M19 P01 independent 2026-10-05 ACCEPTED/FROZEN K-bank byte audit.

This test parses the actual private C++ immutable table and:
(1) packs its hexadecimal literals as native IEEE binary64 *little-endian*;
(2) verifies each member's exact, externally frozen SHA256 digest;
(3) independently recomputes the P01 B1 BLT transform using exact rational
    arithmetic over printed 48-kHz decimal anchors and rounds ONCE to float.
The Python oracle never imports runtime candidate code and never creates
production bank assets. It is preproduction verification, not official Gate G.
"""
from fractions import Fraction
from pathlib import Path
import hashlib
import re
import struct

P01_ID = "rgsml.kweight.bs1770-5.anchor48-blt.v1"
FROZEN_SHA256 = {
    44100: "7fd4ee385758b5a93cd7f9cb90cf6cf53a427923ff5e3169afa6b088c1f0c3a3",
    48000: "d770751cb4aec9759a6fd27f277672b3ddd147ce156002ad0f013d42372875bd",
    88200: "a7be030ffe9de49fa0851243d56d3a0e80e40caea972dee522a742483c64725d",
    96000: "6679b495d85e09150f82a285521987a072fe4dc22fd0ba2d85bb8188f673c95d",
    176400: "e5d17a6f3fa27b991fcb438c03ee17e19b0ed07dfa56769c8e06761ef04cf56b",
    192000: "956e5c393292be0bfe46a87011c3353540ef5305a4bd6ead11a52da57e7e94dc",
}
SHELF_48 = ["1.53512485958697","-2.69169618940638",
            "1.19839281085285","1",
            "-1.69065929318241","0.73248077421585"]
RLB_48 = ["1","-2","1","1","-1.99004745483398",
          "0.99007225036621"]

def exact_p01_section(initial_printed_decimal, fs):
    b = [Fraction(s) for s in initial_printed_decimal[:3]]
    a = [Fraction(s) for s in initial_printed_decimal[3:]]
    u = 48000 - fs
    v = 48000 + fs
    def coefficients(p):
        p0,p1,p2 = p
        return (p0*v*v+p1*u*v+p2*u*u,
                2*p0*u*v+p1*(u*u+v*v)+2*p2*u*v,
                p0*u*u+p1*u*v+p2*v*v)
    numerator = coefficients(b)
    denominator = coefficients(a)
    d0 = denominator[0]
    assert d0 != 0
    result = tuple(float(c/d0) for c in numerator) + (
        1.0, float(denominator[1]/d0), float(denominator[2]/d0))
    return result

def run():
    filename=Path(__file__).resolve().parents[3]/"analysis/src/internal/m19_k_weighting_frozen_banks.hpp"
    source=filename.read_text(encoding="utf-8")
    assert P01_ID in source
    marker=source.index("kFrozenKWeightingBanksCandidate{{")
    # Parse only the table body, not illustrative literals in comments.
    table=source[marker:source.index("\n}};",marker)]
    pattern=re.compile(
       r'\{\s*(\d+)U\s*,\s*\{([^{}]+)\}\s*,\s*\{([^{}]+)\}\s*,'
       r'\s*"([^"]+)"\s*,\s*"([0-9a-f]{64})"\s*\}',re.S)
    matches=list(pattern.finditer(table))
    assert len(matches)==6, f"found {len(matches)} frozen member rows, expected 6"
    verified=set()
    for match in matches:
        fs=int(match.group(1))
        assert fs in FROZEN_SHA256 and fs not in verified
        shelf_strings=[s.strip() for s in match.group(2).split(",")]
        rlb_strings=[s.strip() for s in match.group(3).split(",")]
        assert len(shelf_strings)==len(rlb_strings)==6
        assert all(re.fullmatch(r"-?0x[0-9a-f.]+p[+-]\d+",s)
                   for s in shelf_strings+rlb_strings)
        actual=tuple(float.fromhex(s) for s in shelf_strings+rlb_strings)
        assert all(shelf_strings[i].count("p")==1 for i in range(6))
        assert match.group(4)==f"{P01_ID}.fs{fs}"
        packed=struct.pack("<12d",*actual)
        sha=hashlib.sha256(packed).hexdigest()
        assert sha==match.group(5)==FROZEN_SHA256[fs],(fs,sha)
        expected=exact_p01_section(SHELF_48,fs)+exact_p01_section(RLB_48,fs)
        assert actual==expected, (fs, [x.hex() for x in actual],
                                  [x.hex() for x in expected])
        assert actual[3]==1.0 and actual[9]==1.0
        verified.add(fs)
    assert verified==set(FROZEN_SHA256)
    print("M19_P01_SIX_FROZEN_BANKS_SHA256_AND_EXACT_RATIONAL_PASS_NOT_GATE_G")

if __name__=="__main__":
    run()
