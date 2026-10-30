#!/usr/bin/env python3
"""Resumable *real* Q0 continuous MPFR DTFT attempt, all 14 ratios.

Every accepted leaf is a genuine third-order directed-MPFR proof over its
entire rational frequency interval. UNRESOLVED leaves never count as proof.
Private candidate bytes stay offline. No release or product admission is issued.
"""
from __future__ import annotations

import argparse
from fractions import Fraction
from hashlib import sha256
import json
import os
from pathlib import Path
import tempfile

from build_candidate_manifest import canonical_pair_for_ratio
from dtft_third_order_rigorous import prove_window, PreparedThirdOrderProof
from exact_dyadic_zero_stuff_oracle import read_locked_kernel
from phase11_math_preflight import ContractError, Ratio
from verify_candidate_family import audit_candidate_family

SCHEMA = "rgsml.phase11.resumable-real14-dtft-continuum-nonrelease/1"
PROVEN = "CONTINUUM_SUBBAND_PROVEN_NONRELEASE"


def _exact_record(path: Path, expected: dict) -> dict | None:
    if path.is_symlink():
        raise ContractError("symlink not permitted in Q0 proof cache")
    if not path.exists():
        return None
    if not path.is_file() or path.stat().st_size > 5000:
        raise ContractError("invalid Q0 checkpoint file")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (ValueError, OSError, UnicodeError) as exc:
        raise ContractError("unreadable Q0 checkpoint") from exc
    if not isinstance(data, dict) or any(data.get(k) != v for k, v in expected.items()):
        raise ContractError("Q0 proof receipt does not match exact SHA/interval/engine")
    if any(data.get(k) is not False for k in ("release_qualified", "product_admission_authorized")):
        raise ContractError("Q0 receipt falsely claims product release")
    if data.get("status") not in ("PROVEN", "SPLIT_UNRESOLVED"):
        raise ContractError("Q0 receipt status invalid")
    result = data.get("proof_result")
    if not isinstance(result, dict) or result.get("release_qualified") is not False:
        raise ContractError("Q0 receipt lacks real proof result")
    if data["status"] == "PROVEN":
        if (result.get("status") != PROVEN or result.get("remaining_boxes") != 0
            or type(result.get("proven_boxes")) is not int
            or result["proven_boxes"] < 1):
            raise ContractError("false Q0 continuum proof receipt")
    elif result.get("status") != "UNRESOLVED":
        raise ContractError("split without actual unresolved proof attempt")
    return data


def _commit_receipt(path: Path, receipt: dict) -> None:
    # Never replace a concurrent writer's evidence. Atomic new-file hardlink
    # avoids declaring a truncated JSON file to be a successful checkpoint.
    fd, scratch = tempfile.mkstemp(prefix=".phase11-proof-", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(receipt, stream, sort_keys=True, separators=(",", ":"))
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        try:
            os.link(scratch, path)
        except FileExistsError as exc:
            raise ContractError("concurrent proof receipt detected; re-audit") from exc
    finally:
        Path(scratch).unlink(missing_ok=True)


def walk_ratio(ratio: Ratio, taps: tuple[Fraction, ...], candidate_sha256: str,
               checkpoints: Path, *, max_attempts: int, boxes: int,
               bits: int, max_depth: int, engine_sha256: str,
               prover=prove_window) -> dict:
    """One ratio, two entire bands. Callable injection is test-only; no release."""
    if (type(max_attempts) is not int or max_attempts < 0 or
        type(boxes) is not int or not 1 <= boxes <= 1000000 or
        type(bits) is not int or not 256 <= bits <= 4096 or
        type(max_depth) is not int or not 1 <= max_depth <= 24):
        raise ContractError("invalid offline MPFR campaign bounds")
    if not checkpoints.is_dir() or checkpoints.is_symlink():
        raise ContractError("safe existing proof checkpoint directory required")
    if not isinstance(candidate_sha256, str) or len(candidate_sha256) != 64:
        raise ContractError("missing independent candidate SHA")
    common = min(ratio.fin, ratio.fout)
    bands = (("passband", Fraction(0), Fraction(12*common, 25)),
             ("stopband", Fraction(common, 2), Fraction(ratio.fin*ratio.L, 2)))
    attempted=proven=reused=blocked=0
    session=None
    remaining=0
    for kind, first, last in bands:
        stack = [(first, last, "r", 0)]
        while stack:
            lo,hi,route,depth = stack.pop()
            filename=f"L{ratio.L}_M{ratio.M}_{kind}_{route}.json"
            expected=dict(schema=SCHEMA, fin=ratio.fin, fout=ratio.fout,
                          L=ratio.L, M=ratio.M, kind=kind,
                          lo_hz=str(lo), hi_hz=str(hi),
                          candidate_sha256=candidate_sha256,
                          engine_sha256=engine_sha256, bits=bits)
            record=_exact_record(checkpoints/filename, expected)
            if record is not None:
                reused+=1
                state=record["status"]
            else:
                if attempted >= max_attempts:
                    remaining+=len(stack)+1
                    break
                if prover is prove_window:
                    if session is None:
                        session=PreparedThirdOrderProof(ratio,taps,bits=bits)
                    result=session.prove_window(lo,hi,kind=kind,max_boxes=boxes)
                else:
                    result=prover(ratio,taps,lo,hi,kind=kind,max_boxes=boxes,bits=bits)
                attempted+=1
                if (not isinstance(result,dict) or result.get("release_qualified") is not False
                    or result.get("status") not in (PROVEN,"UNRESOLVED")):
                    raise ContractError("invalid MPFR continuum proof result")
                if result["status"]==PROVEN:
                    if result.get("remaining_boxes") != 0 or result.get("proven_boxes",0)<1:
                        raise ContractError("incomplete MPFR boxes falsely proved")
                    state="PROVEN"
                else:
                    if depth >= max_depth or lo == hi:
                        blocked+=1
                        continue  # Never persist unresolved as a qualifying leaf.
                    state="SPLIT_UNRESOLVED"
                _commit_receipt(checkpoints/filename,
                                dict(expected,status=state,proof_result=result,
                                     release_qualified=False,
                                     product_admission_authorized=False))
            if state=="PROVEN":
                proven+=1
            else:
                if depth>=max_depth or lo==hi:
                    raise ContractError("cached split exceeds configured depth")
                mid=(lo+hi)/2
                stack.extend(((mid,hi,route+"1",depth+1),
                              (lo,mid,route+"0",depth+1)))
    return dict(attempted=attempted,proven_leaves=proven,
                replayed_receipts=reused,blocked_leaves=blocked,
                pending_nodes=remaining)


def run_campaign(candidate_dir: Path, checkpoints: Path, *,
                 max_attempts: int=8, boxes: int=128, bits: int=256,
                 max_depth: int=20) -> dict:
    if not candidate_dir.is_dir() or candidate_dir.is_symlink():
        raise ContractError("original private candidate directory required")
    if (not checkpoints.is_dir() or checkpoints.is_symlink()
        or checkpoints.resolve().is_relative_to(candidate_dir.resolve())
        or candidate_dir.resolve().is_relative_to(checkpoints.resolve())):
        raise ContractError("private assets and receipt trees must be disjoint")
    proof_source=Path(__file__).with_name("dtft_third_order_rigorous.py")
    engine_sha256=sha256(proof_source.read_bytes()).hexdigest()
    audit=audit_candidate_family(candidate_dir,require_complete=True)
    if audit.get("complete") is not True or audit.get("ratios")!=14:
        raise ContractError("fourteen original SHA-locked candidates required")
    manifest=json.loads((candidate_dir/"CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json").read_text())
    entries={(row["L"],row["M"]):row for row in manifest["assets"]}
    pairs=canonical_pair_for_ratio()
    if set(entries)!=set(pairs):
        raise ContractError("complete frozen ratio mapping required")
    total=dict(attempted=0,proven_leaves=0,replayed_receipts=0,
               blocked_leaves=0,pending_nodes=0)
    for key, (fin,fout) in pairs.items():
        row=entries[key]
        ratio=Ratio.make(fin,fout)
        payload=(candidate_dir/row["filename"]).read_bytes()
        digest=sha256(payload).hexdigest()
        if digest!=row["candidate_sha256"]:
            raise ContractError("real Q0 SHA drift during proof")
        _,taps=read_locked_kernel(payload,ratio,digest)
        remaining=max(0,max_attempts-total["attempted"])
        progress=walk_ratio(ratio,taps,digest,checkpoints,
                            max_attempts=remaining,boxes=boxes,bits=bits,
                            max_depth=max_depth,engine_sha256=engine_sha256)
        for key2 in total:
            total[key2]+=progress[key2]
    complete=(total["pending_nodes"]==0 and total["blocked_leaves"]==0)
    return dict(schema=SCHEMA,
                status=("COMPLETE_CONTINUUM_CANDIDATE_NONRELEASE" if complete
                        else "UNRESOLVED_MORE_MPFR_PROOF_REQUIRED"),
                candidate_ratios=14, required_full_bands=28,
                continuous_coverage_observed=complete,
                q1_q6_passed=False, product_admission_authorized=False,
                release_qualified=False, engine_sha256=engine_sha256,
                **total)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--candidate-dir",type=Path,required=True)
    p.add_argument("--checkpoint-dir",type=Path,required=True)
    p.add_argument("--max-attempts",type=int,default=8)
    p.add_argument("--boxes",type=int,default=128)
    p.add_argument("--bits",type=int,default=256)
    p.add_argument("--max-depth",type=int,default=20)
    a=p.parse_args()
    print(json.dumps(run_campaign(a.candidate_dir,a.checkpoint_dir,
          max_attempts=a.max_attempts,boxes=a.boxes,bits=a.bits,
          max_depth=a.max_depth),sort_keys=True),flush=True)

if __name__=="__main__":
    main()