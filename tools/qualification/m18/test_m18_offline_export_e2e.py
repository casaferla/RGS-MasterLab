#!/usr/bin/env python3
"""Independent readback of 24 actual M18 WAVs from native isolated export.

Preproduction ONLY. Never trust a native receipt without re-reading file
bytes, independent WAV structure validation and numerical sample checks.
No M17 linkage, canonical .rgsmaster changes or production acceptance.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile

from test_m18_independent_reference import independent_quantize
from verify_m18_wav_candidate import audit_file, CandidateError

SEED=0x0123456789ABCDEF

def check_one(folder: Path, fields: list[str]) -> None:
    name, encoding, rate, ch, count, start, seed, fsha, psha=fields
    file=folder/name
    ch,count,start,seed,rate=map(int,(ch,count,start,seed,rate))
    if seed!=SEED or count!=4161 or start!=(1<<32)-4:
        raise AssertionError('unexpected test manifest')
    canonical=audit_file(file,expected_encoding=encoding,
        expected_rate=rate,expected_channels=ch,expected_frames=count,
        expected_file_sha256=fsha,expected_payload_sha256=psha)
    blob=file.read_bytes()
    step=3 if encoding=='PCM24' else 4
    for frame in {0,1,2,3,4,5,31,4095,4096,4097,4160,*range(0,count,97)}:
        absolute=start+frame
        for channel in range(ch):
            code=int(absolute%7)-3
            x=code*2.0**-25
            if channel:x=-x
            pos=canonical['header_bytes']+(frame*ch+channel)*step
            value=blob[pos:pos+step]
            if encoding=='PCM24':
                q=independent_quantize(x,seed,absolute,channel)
                expected=(q&0xffffff).to_bytes(3,'little')
            else:
                expected=struct.pack('<f',x)
            if value!=expected:
                raise AssertionError((name,frame,channel,value.hex(),expected.hex()))
    if hashlib.sha256(blob).hexdigest()!=fsha:
        raise AssertionError('bad native whole SHA')
    if hashlib.sha256(blob[canonical['header_bytes']:]).hexdigest()!=psha:
        raise AssertionError('bad native payload SHA')


def main(executable: str) -> None:
    with tempfile.TemporaryDirectory(prefix='rgs-m18-e2e-') as folder:
        root=Path(folder)
        run=subprocess.run([executable,'--emit-fixtures',folder],
                           capture_output=True,text=True,check=True)
        lines=[row.split('\t')[1:] for row in run.stdout.splitlines()
               if row.startswith('M18_FILE\t')]
        assert len(lines)==24, len(lines)
        assert 'M18_OFFLINE_EXPORT_PREPRODUCTION_PASS' in run.stdout
        for fields in lines:check_one(root,fields)
        assert not list(root.glob('*.rgsml-m18-private-*'))
        # Keep artifacts immutable; challenge only PRIVATE diagnostic copies.
        for fields in (lines[0],lines[1]):
            path=root/fields[0]
            original=path.read_bytes()
            corrupt=root/'corrupt-private.wav'
            tampered=bytearray(original)
            tampered[-1]^=1
            corrupt.write_bytes(tampered)
            try:
                audit_file(corrupt,expected_file_sha256=fields[7])
            except CandidateError:
                pass
            else:
                raise AssertionError('mutated WAV accepted')
            corrupt.write_bytes(original[:-1])
            try:audit_file(corrupt)
            except CandidateError:pass
            else:raise AssertionError('truncated WAV accepted')
            corrupt.unlink()
            assert path.read_bytes()==original
    print('M18_OFFLINE_24_WAV_INDEPENDENT_READBACK_PASS_NOT_PRODUCT_FQ')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('executable')
    args=parser.parse_args()
    main(args.executable)
