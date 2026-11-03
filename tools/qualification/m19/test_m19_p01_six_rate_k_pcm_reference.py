#!/usr/bin/env python3
"""M19 P01 six-rate independently derived first-block K/PCM references.

Source authority: Phase-6 P01 Product Owner ACCEPTED/FROZEN 2026-10-05.
Exact-rational bank transform is separate from the C++ candidate's immutable
hex-double table; an earlier independent test checks its 6 SHA256 hashes.
This second oracle synthesizes ACTUAL target-Fs PCM and evaluates two causal
biquads with fresh zero state at the start of the logical scope. No product
QTP or Gate G status is inferred from these diagnostic comparisons.
"""
import math
from test_m19_p01_kbank_frozen_authority import (
    FROZEN_SHA256,SHELF_48,RLB_48,exact_p01_section)

CLOCKS=(44100,48000,88200,96000,176400,192000)
SINE_997=(-21.070566265290324,-21.071135026298090,
          -21.073279899827530,-21.073418250980165,
          -21.073936631402304,-21.073969594131775)
IMPULSE=(-39.34595955072482,-39.69495697662176,
         -42.239182752896575,-42.59771987346224,
         -45.19122952625317,-45.55454105285134)
DC=(-26.276870319591243,-26.276870319590760,
    -26.276870319591820,-26.276870319589516,
    -26.276870319592900,-26.276870319590760)

def one_section(x,coeff,state):
    b0,b1,b2,a0,a1,a2=coeff
    assert a0==1.0
    x1,x2,y1,y2=state
    y=b0*x+b1*x1+b2*x2-a1*y1-a2*y2
    return y,[x,x1,y,y1]

def measure_first_block(signal,fs):
    # A new logical AnalysisScope means two fresh all-zero stage histories.
    shelf=exact_p01_section(SHELF_48,fs)
    rlb=exact_p01_section(RLB_48,fs)
    z1=[0.0]*4
    z2=[0.0]*4
    squares=[]
    for sample in signal:
        x,z1=one_section(sample,shelf,z1)
        y,z2=one_section(x,rlb,z2)
        assert math.isfinite(y)
        squares.append(y*y)
    energy=math.fsum(squares)/len(squares)
    absolute=10**((-70.0+0.691)/10.0)
    if energy<=absolute:
        return None
    return -0.691+10.0*math.log10(energy)

def run():
    assert len(CLOCKS)==len(FROZEN_SHA256)
    for i,fs in enumerate(CLOCKS):
        n=(2*fs)//5
        assert n in (17640,19200,35280,38400,70560,76800)
        constant=measure_first_block([1.0]*n,fs)
        assert constant is not None and abs(constant-DC[i])<2e-8,(fs,constant)
        pulse=measure_first_block([1.0]+[0.0]*(n-1),fs)
        assert pulse is not None and abs(pulse-IMPULSE[i])<2e-8,(fs,pulse)
        tone=[0.125*math.sin(2*math.pi*997.0*j/fs) for j in range(n)]
        got=measure_first_block(tone,fs)
        assert got is not None and abs(got-SINE_997[i])<2e-8,(fs,got)
    assert set(CLOCKS)==set(FROZEN_SHA256)
    print("M19_P01_SIX_RATE_INDEPENDENT_K_PCM_REFERENCE_PASS_NOT_GATE_G")

if __name__=="__main__":
    run()
