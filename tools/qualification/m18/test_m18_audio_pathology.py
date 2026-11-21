#!/usr/bin/env python3
"""Independent spectral/coherence screen of seven real M18 low-level WAVs.

The windows and numeric guards here are deliberately broad preproduction
falsification tests, NOT PO-adopted musical Gate G or listening evidence.
No Numpy/third-party packages, no use of the C++ implementation for analysis.
"""
from __future__ import annotations

import argparse
import hashlib
import math
from pathlib import Path
import subprocess
import tempfile

from verify_m18_wav_candidate import audit_file, CandidateError

N=65536
PI=math.pi


def codes(payload: bytes) -> list[int]:
    assert len(payload)%3==0
    return [int.from_bytes(payload[i:i+3], 'little', signed=True)
            for i in range(0,len(payload),3)]


def inspect_one(root: Path, info: list[str]) -> dict:
    filename, name, rate, channels, frames, sha, payload_sha=info
    rate,channels,frames=map(int,(rate,channels,frames))
    assert frames==N and channels==(2 if name=='identical_stereo_ambience' else 1)
    result=audit_file(root/filename,expected_encoding='PCM24',
        expected_rate=rate,expected_channels=channels,expected_frames=N,
        expected_file_sha256=sha,expected_payload_sha256=payload_sha)
    binary=(root/filename).read_bytes()
    data=codes(binary[result['header_bytes']:])
    assert len(data)==N*channels
    if name=='sine_025lsb':
        t=[0.25*math.sin(2*PI*997*i/N) for i in range(N)]
        error=[data[i]-t[i] for i in range(N)]
        mean=math.fsum(error)/N
        second=math.fsum(v*v for v in error)/N
        fundamental=2*math.fsum(error[i]*math.sin(2*PI*997*i/N)
                              for i in range(N))/N
        # Undithered 0.25-LSB sine quantizes to zero and therefore
        # has a coherent fundamental error amplitude exactly -0.25 LSB.
        # Proper TPDF removes this deterministic birdie without shaping.
        assert abs(fundamental)<0.035, fundamental
        assert abs(mean)<0.018, mean
        assert abs(second-0.25)<0.018, second
        return {'name':name,'rate':rate,'fundamental':fundamental,
                'mean':mean,'second':second,'payload_sha256':payload_sha}
    if name=='sine_2lsb':
        t=[2*math.sin(2*PI*997*i/N) for i in range(N)]
        residual=[data[i]-t[i] for i in range(N)]
        second=math.fsum(v*v for v in residual)/N
        assert abs(second-0.25)<0.02, second
        for harmonic in (1,2,3,5,7):
            bin_id=997*harmonic
            amplitude=2*math.hypot(
                math.fsum(residual[i]*math.cos(2*PI*bin_id*i/N)
                          for i in range(N)),
                math.fsum(residual[i]*math.sin(2*PI*bin_id*i/N)
                          for i in range(N)))/N
            assert amplitude<0.035,(harmonic,amplitude)
        return {'name':name,'second':second}
    if name=='identical_stereo_ambience':
        left=data[0::2];right=data[1::2]
        diff=sum(a!=b for a,b in zip(left,right))/N
        assert diff>0.12, diff
        t=[0.2*math.sin(2*PI*997*i/N)+0.16*math.sin(2*PI*113*i/N)
           for i in range(N)]
        cross=math.fsum((left[i]-t[i])*(right[i]-t[i])
                        for i in range(N))/N
        assert abs(cross)<0.025,cross
        return {'name':name,'difference_ratio':diff,'error_cross':cross}
    if name=='sparse_tail':
        # Most frames are absolute zero input, but output must not gate
        # noise or reset Philox at the sparse impulses.
        silent=[data[i] for i in range(N) if i%2048!=0]
        nonzero=sum(q!=0 for q in silent)/len(silent)
        assert 0.20<nonzero<0.30, nonzero
        return {'name':name,'silent_nonzero':nonzero}
    if name in ('fade_16lsb','low_frequency_decay'):
        assert any(v!=0 for v in data[:4096])
        assert len(set(data[-4096:]))>1
        return {'name':name,'tail_unique_codes':len(set(data[-4096:]))}
    raise AssertionError(name)


def main(executable: str):
    with tempfile.TemporaryDirectory(prefix='m18-pathology-') as temp:
        root=Path(temp)
        run=subprocess.run([executable,'--emit-fixtures',temp],
            capture_output=True,text=True,check=True)
        lines=[line.split('\t')[1:] for line in run.stdout.splitlines()
               if line.startswith('M18_AUDIO\t')]
        assert len(lines)==7, len(lines)
        assert 'M18_SEVEN_LOWLEVEL_WAV_PROGRAMS_PASS_NOT_LISTENING_FQ' in run.stdout
        observations=[inspect_one(root,fields) for fields in lines]
        lows=[x for x in observations if x['name']=='sine_025lsb']
        assert len(lows)==2 and lows[0]['payload_sha256']==lows[1]['payload_sha256'],(
            lows[0]['payload_sha256'],lows[1]['payload_sha256'])
        assert not list(root.glob('*.rgsml-m18-private-*'))
    print('M18_SEVEN_LOWLEVEL_SPECTRAL_AND_STEREO_SCREEN_PASS_NOT_FORMAL_GATE_G')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('executable')
    main(parser.parse_args().executable)
