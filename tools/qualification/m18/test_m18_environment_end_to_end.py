#!/usr/bin/env python3
"""Independent delivered-file M18 FP environment matrix readback.

Exercises real WAVs from a native M18 executable. Parses/hashes all bytes,
then independently recomputes EVERY PCM24 addressed sample with Philox and
exact dyadic RN64/RNE, and Float32 with the Python struct converter.
No M17 product FQ or formal M18 Gate-G adoption is implied.
"""
from __future__ import annotations

import argparse
from fractions import Fraction
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile

from verify_m18_wav_candidate import CandidateError, audit_file

SEED=0x0123456789ABCDEF
ORIGIN=(1<<32)-8
RATES={44100,48000,88200,96000,176400,192000}
MASK=(1<<32)-1
PCM_BITS=(
    0x0000000000000000,0x8000000000000000,
    0x3E60000000000000,0xBE60000000000000,
    0x3E70000000000000,0xBE70000000000000,
    0x3FE8000000000000,0xBFE8000000000000,
    0x0000000000000001,0x8000000000000001,
    0x3FE0000000000000,0xBFE0000000000000,
    0x3FD0000000000000,0xBFD0000000000000,
)
FLOAT_BITS=(
    0x0000000000000000,0x8000000000000000,
    0x3FF0000000000000,0xBFF0000000000000,
    0x3FF000000FFFFFFF,0x3FF0000010000000,
    0x3FF0000010000001,0xBFF0000010000000,
    0x36A0000000000000,0xB6A0000000000000,
    0x3690000000000000,0xB690000000000000,
    0x3690000000000004,0xB690000000000004,
    0x3810000000000000,0xB810000000000000,
    0x47EFFFFFE0000000,0xC7EFFFFFE0000000,
    0x0000000000000001,0x8000000000000001,
)

def source(encoding:str,index:int,channel:int) -> float:
    cases=PCM_BITS if encoding=='PCM24' else FLOAT_BITS
    bits=cases[(index+7*channel)%len(cases)]
    return struct.unpack('<d',struct.pack('<Q',bits))[0]


def philox(seed:int,frame:int,channel:int) -> tuple[int,int,int,int]:
    ctr=[frame&MASK,(frame>>32)&MASK,channel,0x52475344]
    key=[seed&MASK,(seed>>32)&MASK]
    for i in range(10):
        a=ctr[0]*0xD2511F53
        b=ctr[2]*0xCD9E8D57
        ctr=[((b>>32)^ctr[1]^key[0])&MASK,b&MASK,
             ((a>>32)^ctr[3]^key[1])&MASK,a&MASK]
        if i<9:
            key[0]=(key[0]+0x9E3779B9)&MASK
            key[1]=(key[1]+0xBB67AE85)&MASK
    return tuple(ctr)


def expected_pcm24(sample:float,frame:int,channel:int)->bytes:
    a,b,c,d=philox(SEED,frame,channel)
    u0=(a<<21)|(b>>11)
    u1=(c<<21)|(d>>11)
    # Reconstruct the binary64 RN64 stage from exact rationals; this is
    # independent of the product's 1152-bit integer summation implementation.
    y=float(Fraction.from_float(sample)*(1<<23)
            +Fraction(u0-u1,1<<53))
    q=round(y)  # integer RNE (Python ties-to-even)
    if not -(1<<23)<=q<(1<<23):
        raise AssertionError(f'Unexpected PCM24 test overload {q}')
    return q.to_bytes(3,'little',signed=True)


def audit_row(root:Path,parts:list[str])->tuple[int,int,str]:
    assert len(parts)==10 and parts[0]=='M18_ENV_FILE', parts
    _,name,encoding,r,c,frames,start,seed,whole_sha,payload_sha=parts
    rate,channels,n,first,key=map(int,(r,c,frames,start,seed))
    assert rate in RATES and encoding in ('PCM24','FLOAT32')
    assert channels in (1,2) and n==(4223 if rate==48000 and channels==2 else 257)
    assert first==ORIGIN and key==SEED
    assert name.startswith(f'm18-env-{rate}-') and name.endswith('-m0-v0.wav')
    file=root/name
    details=audit_file(file,expected_encoding=encoding,
        expected_rate=rate,expected_channels=channels,expected_frames=n,
        expected_file_sha256=whole_sha,expected_payload_sha256=payload_sha)
    all_data=file.read_bytes()
    data=all_data[details['header_bytes']:]
    assert hashlib.sha256(all_data).hexdigest()==whole_sha
    assert hashlib.sha256(data).hexdigest()==payload_sha
    expected=bytearray()
    for f in range(n):
        for c in range(channels):
            sample=source(encoding,f,c)
            if encoding=='PCM24':
                expected.extend(expected_pcm24(sample,first+f,c))
            else:
                expected.extend(struct.pack('<f',sample))
    assert data==expected, (name, 'independent sample bytes differed')
    # Confirm both independent hash modes reject tampered bytes, not just an
    # accidental literal passing because the producer and checker share state.
    if rate==44100 and channels==1:
        mutated=bytearray(all_data)
        mutated[-1]^=0x01
        file.write_bytes(mutated)
        try:
            audit_file(file,expected_file_sha256=whole_sha,
                       expected_payload_sha256=payload_sha)
        except CandidateError:
            pass
        else:
            raise AssertionError('tampered WAV accepted')
        file.write_bytes(all_data)
    return rate,channels,encoding


def main() -> None:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('executable')
    args=ap.parse_args()
    with tempfile.TemporaryDirectory(prefix='m18-env-independent-') as d:
        root=Path(d)
        p=subprocess.run([args.executable,'--emit-fixtures',str(root)],
                         capture_output=True,text=True,check=True)
        rows=[line.split('\t') for line in p.stdout.splitlines()
              if line.startswith('M18_ENV_FILE\t')]
        assert len(rows)==24, len(rows)
        assert 'M18_DELIVERED_FP_ENV_INVARIANCE_PASS_NOT_PRODUCT_GATE' in p.stdout
        seen={audit_row(root,row) for row in rows}
        expected={(r,c,e) for r in RATES for c in (1,2)
                  for e in ('PCM24','FLOAT32')}
        assert seen==expected
        assert len(list(root.iterdir()))==24, 'Unexpected temp/final file'
    print('M18_FP_ENV_INDEPENDENT_24_REAL_WAV_ORACLE_PASS_NOT_GATE_G')

if __name__=='__main__':
    main()
