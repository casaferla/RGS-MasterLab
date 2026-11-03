#!/usr/bin/env python3
"""15 real downsampling anti-alias probe frequencies: NONCERTIFYING screen.

Numerically evaluate H(f_alias) for each of the 15 rate pairs with SOURCE
Hz > MASTER Hz, with original locked binary64 FIR and extended precision
cosine/summation. This is NOT an MPFR continuous DTFT proof and is NOT a
time-domain Qt source-to-master alias-conformance gate or release asset.
"""
from __future__ import annotations
import argparse
from hashlib import sha256
import json
from math import gcd
from pathlib import Path
import numpy as np

from phase11_math_preflight import ContractError
from verify_candidate_family import audit_candidate_family

RATES=(44100,48000,88200,96000,176400,192000)
STOP_DB=140.0


def numerical_probe(taps, fin: int, fout: int, L: int) -> float:
    """Return measured stopband dB for ONE frozen aliasProbeHz point only."""
    if (not np.finfo(np.longdouble).eps < np.finfo(np.float64).eps
        or fin<=fout or L<1 or taps.size<3 or taps.size%2!=1
        or not np.isfinite(taps).all()
        or not np.array_equal(taps,taps[::-1])):
        raise ContractError("extended precision or locked FIR geometry invalid")
    d=(taps.size-1)//2
    x=taps.astype(np.longdouble,copy=False)
    dc=(x[d]+2*np.sum(x[d+1:],dtype=np.longdouble))/np.longdouble(L)
    if not np.isfinite(dc) or abs(dc-1)>np.longdouble("0.000001"):
        raise ContractError("unqualified FIR DC normalization")
    probe=np.longdouble(fin+fout)/4
    pi=np.arccos(np.longdouble(-1))
    angle=2*pi*probe/np.longdouble(fin*L)
    indices=np.arange(1,d+1,dtype=np.longdouble)
    value=(x[d]+2*np.sum(
        x[d+1:]*np.cos(angle*indices),dtype=np.longdouble))/np.longdouble(L)
    if not np.isfinite(value):
        raise ContractError("nonfinite alias response")
    if value==0:
        return float("inf")
    return float(-20*np.log10(abs(value)))


def run_all(candidate_dir:Path) -> dict:
    audited=audit_candidate_family(candidate_dir,require_complete=True)
    if audited.get("complete") is not True:
        raise ContractError("expected all 14 genuine checked Q0 candidate kernels")
    manifest=json.loads(
        (candidate_dir/"CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json").read_text())
    if (manifest.get("status")!="CANDIDATE_NOT_RELEASE_QUALIFIED"
        or manifest.get("product_admission_authorized") is not False
        or manifest.get("observed_ratio_count") != 14):
        raise ContractError("NOT the genuine frozen NONRELEASE 14-candidate manifest")
    frozen={(e["L"],e["M"]):e for e in manifest["assets"]}
    results=[]
    for fin in RATES:
        for fout in RATES:
            if fin<=fout:
                continue
            g=gcd(fin,fout)
            L,M=fout//g,fin//g
            entry=frozen.get((L,M))
            if entry is None:
                raise ContractError("missing genuine reduced ratio")
            blob=(candidate_dir/entry["filename"]).read_bytes()
            if sha256(blob).hexdigest()!=entry["candidate_sha256"]:
                raise ContractError("tampered frozen Q0 ratio")
            taps=np.frombuffer(blob,dtype="<f8",offset=52)
            if taps.size!=entry["N"]:
                raise ContractError("wrong exact frozen candidate length")
            attenuation=numerical_probe(taps,fin,fout,L)
            results.append({
                "source_hz":fin,"master_hz":fout,
                "L":L,"M":M,
                "alias_probe_hz":(fin+fout)/4,
                "candidate_sha256":entry["candidate_sha256"],
                "attenuation_db":attenuation,
                "numerical_screen_pass":attenuation>=STOP_DB,
            })
    if len(results)!=15 or not all(r["numerical_screen_pass"] for r in results):
        # Preserve invalid observations via an explicit status, never PASS.
        state="FAIL_NUMERICAL_ALIAS_SCREEN_NONRELEASE"
    else:
        state="ALL_15_SAMPLED_ANTIALIAS_PROBES_PASS_NONRELEASE"
    return {
        "schema":"rgsml.phase11.real14.alias-probes-15.extended-float-screen/1",
        "status":state,"rows":results,"sampled_frequency_points":15,
        "requirement_stopband_attenuation_db":STOP_DB,
        "minimum_observed_attenuation_db":min(x["attenuation_db"] for x in results),
        "all_points_pass":state.startswith("ALL_15_"),
        "extended_precision_platform_required":True,
        "time_domain_product_alias_probe_verified":False,
        "rigorous_MPFR_continuous_stopband_proven":False,
        "q1_q6_release_qualified":False,
        "product_admission_authorized":False,
        "release_qualified":False,
    }


def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--candidate-dir",type=Path,required=True)
    p.add_argument("--report",type=Path,required=True)
    a=p.parse_args()
    if (a.report.exists() or a.report.is_symlink() or
        not a.report.parent.is_dir() or
        a.report.resolve().is_relative_to(a.candidate_dir.resolve())):
        raise ContractError("write a NEW nonrelease report outside frozen assets")
    result=run_all(a.candidate_dir)
    a.report.write_text(json.dumps(result,sort_keys=True,indent=2)+"\n")
    print(result["status"],result["minimum_observed_attenuation_db"])
    if not result["all_points_pass"]:
        raise SystemExit(1)


if __name__=="__main__":
    main()
