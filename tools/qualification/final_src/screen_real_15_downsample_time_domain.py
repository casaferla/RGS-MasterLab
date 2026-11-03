#!/usr/bin/env python3
"""True 1-second 15-case alias-sine time-domain NUMERICAL screen, NONRELEASE.

Uses an independent scipy.signal.resample_poly polyphase implementation,
not Qt / RGS product AudioBuffer. Original SHA-pinned MPFR/Ziv candidate
assets, real SOURCE Fs, exact frozen rates, amplitude .25 / phase 0, and
aliasProbeHz=(fin+fout)/4. No continuous response or Q1-Q6 authority.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from math import ceil, gcd, log10
from pathlib import Path
import numpy as np

from phase11_math_preflight import ContractError
from screen_real_15_downsample_alias_probe import RATES, STOP_DB, run_all


def check_time_domain(candidate_dir: Path) -> dict:
    # First mandatory audit: all 14 genuine FIRs and 15 frequency-response
    # probes must pass. Do NOT permit this script to bypass source provenance.
    frequency=run_all(candidate_dir)
    if not frequency["all_points_pass"]:
        raise ContractError("15-source genuine Q0 alias response screen failed")
    from scipy.signal import resample_poly

    rows=[]
    for ref in frequency["rows"]:
        fin, fout = ref["source_hz"],ref["master_hz"]
        g=gcd(fin,fout)
        L,M=fout//g,fin//g
        candidate=candidate_dir/f"RGSMSR1_L{L}_M{M}.bin"
        raw=candidate.read_bytes()
        if sha256(raw).hexdigest()!=ref["candidate_sha256"]:
            raise ContractError("genuine immutable FIR was modified")
        taps=np.frombuffer(raw,dtype="<f8",offset=52)
        # 1 second exact SOURCE duration, 0.25-amplitude phase-0 sine;
        # deterministic output frame count EXACTLY 1 second of MASTER.
        f=(fin+fout)/4
        source=np.float64(.25)*np.sin(
            (2*np.pi*np.float64(f)/np.float64(fin))
            *np.arange(fin,dtype=np.float64))
        # scipy uses h*=L internally. h/L preserves original FIR gain.
        # symmetric boundary mode matches even endpoint reflection; trim
        # >filter support so we only measure settled interior output.
        out=resample_poly(source,L,M,window=taps/L,padtype="symmetric")
        if out.size!=fout or not np.isfinite(out).all():
            raise ContractError("wrong master sample map or nonfinite output")
        transient_out=ceil(((taps.size-1)/2)/L*fout/fin)+20
        trim=max(ceil(.03*fout),transient_out)
        if 2*trim>=out.size:
            raise ContractError("alias-screen settled region is empty")
        interior=out[trim:out.size-trim]
        amplitude=float(np.sqrt(2*np.mean(interior*interior)))
        if not np.isfinite(amplitude) or amplitude<=0:
            raise ContractError("invalid alias output RMS amplitude")
        attenuation=-20*log10(amplitude/.25)
        delta=abs(attenuation-ref["attenuation_db"])
        rows.append({
            "source_hz":fin,"master_hz":fout,
            "L":L,"M":M,
            "alias_probe_hz":f,
            "candidate_sha256":ref["candidate_sha256"],
            "source_frames":fin,"master_frames":int(out.size),
            "trim_each_end_frames":trim,
            "source_amplitude":0.25,
            "measured_output_peak_amplitude_from_rms":amplitude,
            "output_alias_attenuation_db":round(attenuation,6),
            "dtft_one_point_screen_db":round(ref["attenuation_db"],6),
            "difference_db":round(delta,6),
            "numeric_pass":attenuation>=STOP_DB and delta<10.0,
        })
    passed=len(rows)==15 and all(x["numeric_pass"] for x in rows)
    return {
        "schema":"rgsml.phase11.real14.alias-probe-15.scipy-time-domain/1",
        "status":("ALL_15_REAL_SOURCE_TO_MASTER_ALIAS_TIME_DOMAIN_PASS_NONRELEASE"
                  if passed else "FAIL_NUMERICAL_15_ALIAS_TIME_DOMAIN_NONRELEASE"),
        "checked_pairs":len(rows),
        "source_seconds_per_case":1,
        "source_sine_amplitude":0.25,
        "stop_attenuation_threshold_db":STOP_DB,
        "minimum_measured_stop_attenuation_db":
            min(x["output_alias_attenuation_db"] for x in rows),
        "all_points_pass":passed,
        "reference_backend":"SciPy resample_poly; float64 PCM and genuine frozen binary64 FIR",
        "source_master_product_qt_engine_exercised":False,
        "frozen_CI_native_15_alias_gate_passed":False,
        "rigorous_MPFR_continuous_stopband_proven":False,
        "Q1_Q6_complete":False,"product_admission_authorized":False,
        "release_qualified":False,"rows":rows,
    }


def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--candidate-dir",type=Path,required=True)
    p.add_argument("--report",type=Path,required=True)
    a=p.parse_args()
    if (a.report.exists() or a.report.is_symlink()
        or not a.report.parent.is_dir()
        or a.report.resolve().is_relative_to(a.candidate_dir.resolve())):
        raise ContractError("new nonrelease report must be outside frozen assets")
    report=check_time_domain(a.candidate_dir)
    a.report.write_text(json.dumps(report,sort_keys=True,indent=2)+"\n")
    print(report["status"],report["minimum_measured_stop_attenuation_db"])
    if not report["all_points_pass"]:
        raise SystemExit(1)


if __name__=="__main__":
    main()
