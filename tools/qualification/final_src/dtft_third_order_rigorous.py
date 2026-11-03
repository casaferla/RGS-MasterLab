#!/usr/bin/env python3
"""RGS Phase11 TRUE continuous DTFT interval proof, MPFR third order, NONRELEASE.

For exact frozen symmetric binary64 h, zero-phase H(f) = (h[D]+2 sum
h[D+u] cos(2*pi*f*u/(fin*L)))/L. Bound H, H', H'' at exact rational
midpoint by directed 256-bit MPFR; third derivative sup comes from exact
dyadic third moment. Lagrange remainder encloses ALL real frequencies in
the box, not an FFT grid. This does not authorize release or product SRC.
"""
from __future__ import annotations

from fractions import Fraction
from pathlib import Path
from hashlib import sha256
import argparse
import json

from exact_dyadic_zero_stuff_oracle import read_locked_kernel
from generate_rgsmsr1_mpfr import Engine, RNDD, RNDU
from mpfr_dtft_interval_preflight import (
    _mpfr_dyadic, _validate_dtft_input, PASS_TOL, STOP_TOL)
from phase11_math_preflight import ContractError, Ratio, pi_bounds


def exact_third_moment(taps):
    d = (len(taps) - 1)//2
    return sum((u**3*abs(taps[d+u]) for u in range(1,d+1)),
               Fraction(0))


def _directed_bounds(e, x):
    return (Fraction.from_float(e.fn["get_d"](x.lo.ref,RNDD)),
            Fraction.from_float(e.fn["get_d"](x.hi.ref,RNDU)))


def enclose_third_order(ratio,taps,lo,hi,*,bits=256):
    """Public API: cannot inject a fake/smaller third-derivative bound."""
    _validate_dtft_input(ratio,taps,bits)
    return _validated_enclosure(ratio,taps,lo,hi,
                                m3=exact_third_moment(taps),bits=bits)


def _validated_enclosure(ratio,taps,lo,hi,*,m3,bits):
    _validate_dtft_input(ratio,taps,bits)
    return _prevalidated_enclosure(ratio,taps,lo,hi,m3=m3,bits=bits)


def _prevalidated_enclosure(ratio,taps,lo,hi,*,m3,bits):
    """Only callable AFTER _validate_dtft_input; never a public oracle."""
    lo,hi=Fraction(lo),Fraction(hi)
    grid_hz=ratio.fin*ratio.L
    if lo<0 or hi<lo or hi>Fraction(grid_hz,2):
        raise ContractError("outside MPFR DTFT high-grid Nyquist")
    center,radius=(lo+hi)/2,(hi-lo)/2
    e=Engine(bits)
    pi=e.pi()
    two=e.fraction(Fraction(2))
    grid=e.fraction(Fraction(grid_hz))
    d=(len(taps)-1)//2
    value=_mpfr_dyadic(e,taps[d])
    slope=e.fraction(Fraction(0))
    curvature=e.fraction(Fraction(0))
    for u in range(1,d+1):
        h=taps[d+u]
        if h==0:
            continue
        cycles=(center*u/grid_hz)%1
        if cycles>Fraction(1,2):
            cycles-=1
        phase=two*pi*e.fraction(cycles)
        cosine=(phase+pi/two).sin()
        sine=phase.sin()
        term=two*_mpfr_dyadic(e,h)
        k=two*pi*e.fraction(Fraction(u))/grid
        value=value+term*cosine
        slope=slope-term*k*sine
        curvature=curvature-term*k*k*cosine
    L=e.fraction(Fraction(ratio.L))
    v_lo,v_hi=_directed_bounds(e,value/L)
    s_lo,s_hi=_directed_bounds(e,slope/L)
    c_lo,c_hi=_directed_bounds(e,curvature/L)
    # H''' <= 2/L * (2*pi/grid)^3 * sum u^3 |h[D+u]|.
    # pi_bounds()[1] is a proven exact-rational upper bound.
    sup3=16*pi_bounds()[1]**3*m3/(ratio.L*grid_hz**3)
    variation=(max(abs(s_lo),abs(s_hi))*radius
               +max(abs(c_lo),abs(c_hi))*radius**2/2
               +sup3*radius**3/6)
    return v_lo-variation,v_hi+variation


class PreparedThirdOrderProof:
    """Reuse the exact third moment and validated tap geometry across boxes.

    Constructor is required: no external caller can provide a smaller moment.
    Existing public prove_window remains a fully checked one-shot wrapper.
    """
    def __init__(self,ratio,taps,*,bits=256):
        _validate_dtft_input(ratio,taps,bits)
        self.ratio=ratio
        self.taps=taps
        self.bits=bits
        self._m3=exact_third_moment(taps)

    def prove_window(self,start,end,*,kind,max_boxes=128):
        if (kind not in ("passband","stopband")
            or type(max_boxes) is not int or not 1<=max_boxes<=1_000_000):
            raise ContractError("invalid MPFR third-order proof parameters")
        first,last=Fraction(start),Fraction(end)
        if first<0 or last<first or last>Fraction(self.ratio.fin*self.ratio.L,2):
            raise ContractError("invalid proof frequencies")
        stack=[(first,last)]
        visited=accepted=0
        while stack:
            if visited>=max_boxes:
                return {"status":"UNRESOLVED","visited_boxes":visited,
                        "proven_boxes":accepted,"remaining_boxes":len(stack),
                        "release_qualified":False}
            lo,hi=stack.pop()
            lower,upper=_prevalidated_enclosure(
                self.ratio,self.taps,lo,hi,m3=self._m3,bits=self.bits)
            visited+=1
            good=((lower>=1-PASS_TOL and upper<=1+PASS_TOL)
                  if kind=="passband" else
                  (lower>=-STOP_TOL and upper<=STOP_TOL))
            if good:
                accepted+=1
            elif lo==hi:
                return {"status":"UNRESOLVED","visited_boxes":visited,
                        "proven_boxes":accepted,"remaining_boxes":len(stack)+1,
                        "release_qualified":False}
            else:
                mid=(lo+hi)/2
                stack.extend(((mid,hi),(lo,mid)))
        return {"status":"CONTINUUM_SUBBAND_PROVEN_NONRELEASE",
                "visited_boxes":visited,"proven_boxes":accepted,
                "remaining_boxes":0,"release_qualified":False}


def prove_window(ratio,taps,start,end,*,kind,max_boxes=128,bits=256):
    return PreparedThirdOrderProof(ratio,taps,bits=bits).prove_window(
        start,end,kind=kind,max_boxes=max_boxes)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("asset",type=Path)
    p.add_argument("--fin",type=int,required=True)
    p.add_argument("--fout",type=int,required=True)
    p.add_argument("--pinned-sha256",required=True)
    p.add_argument("--lo",type=Fraction,required=True)
    p.add_argument("--hi",type=Fraction,required=True)
    p.add_argument("--kind",choices=("passband","stopband"),required=True)
    p.add_argument("--max-boxes",type=int,default=128)
    p.add_argument("--report",type=Path,required=True)
    a=p.parse_args()
    if (a.report.exists() or a.report.is_symlink()
        or a.report.resolve().is_relative_to(a.asset.resolve().parent)):
        raise ContractError("report must be a new external file")
    if a.fin==a.fout:
        raise ContractError("identity must not load a FIR")
    r=Ratio.make(a.fin,a.fout)
    if ((a.kind=="passband" and
         (a.lo<0 or a.hi>Fraction(12*min(a.fin,a.fout),25)))
        or (a.kind=="stopband" and
            (a.lo<Fraction(min(a.fin,a.fout),2)
             or a.hi>Fraction(a.fin*r.L,2)))):
        raise ContractError("outside frozen bands")
    data=a.asset.read_bytes()
    if sha256(data).hexdigest()!=a.pinned_sha256:
        raise ContractError("real Q0 SHA does not match frozen independent pin")
    _,taps=read_locked_kernel(data,r,a.pinned_sha256)
    result=prove_window(r,taps,a.lo,a.hi,
                        kind=a.kind,max_boxes=a.max_boxes)
    result.update(schema="rgsml.phase11.mpfr-thirdorder-continuum-subband/1",
                  fin=a.fin,fout=a.fout,kind=a.kind,
                  lo_hz=str(a.lo),hi_hz=str(a.hi),
                  candidate_sha256=a.pinned_sha256,
                  full_passband_and_stopband_proven=False,
                  q1_q6_passed=False,product_admission_authorized=False,
                  release_qualified=False)
    a.report.write_text(json.dumps(result,sort_keys=True,indent=2)+"\n")
    print(json.dumps(result,sort_keys=True))


if __name__=="__main__":
    main()