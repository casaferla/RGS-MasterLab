#!/usr/bin/env python3
"""Fail-closed read-only inventory of the PRIVATE Work Phase3 Q0 handoff.

Verifies archive provenance and declared NONRELEASE evidence. This is NOT an
independent mathematical proof, official Q0 signoff, SRC loader or product gate.
No private candidate coefficient bytes are read from or written to GitHub.
"""
import argparse
import hashlib
import io
import json
from fractions import Fraction
from pathlib import Path, PurePosixPath
from zipfile import ZipFile

PHASE3_SHA = "b6dea3257aa2b4202f297094750e59d0f29d4bd871b106bae092c80417c40251"
Q0_SHA = "94d3340667ab5c86b2db5f2d0e191f58b5d05863413312fb3545faed51798ce4"
PHASE2_SHA = "dbc5d6ba083aa54274b041e39885cce04333d767192de7c69e4ba476456fadb6"
RATIOS = {(1,2),(1,4),(2,1),(4,1),(40,147),(80,147),(147,40),
          (147,80),(147,160),(147,320),(147,640),(160,147),(320,147),(640,147)}
P = "phase3/"

class AuditError(ValueError):
    pass

def require(ok, why):
    if not ok:
        raise AuditError(why)

def sha(b):
    return hashlib.sha256(b).hexdigest()

def parsed(b):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "duplicate JSON key")
            result[key] = value
        return result
    def refuse(x):
        raise AuditError("nonfinite JSON value")
    try:
        return json.loads(b, object_pairs_hook=unique, parse_constant=refuse)
    except AuditError:
        raise
    except (UnicodeError, ValueError) as e:
        raise AuditError("invalid JSON") from e

def no_release(obj):
    for name in ("release_qualified","product_admission_authorized",
                 "canonical_q0_approved","q1_q6_passed",
                 "old_canonical_receipts_modified","canonical_44_receipts_modified"):
        if name in obj:
            require(obj[name] is False, name+" is not false")

def audit(data, pin=PHASE3_SHA):
    require(sha(data)==pin, "external Phase3 ZIP pin mismatch")
    require(len(data)<10_000_000, "unexpected large ZIP")
    try:
        with ZipFile(io.BytesIO(data)) as z:
            infos=z.infolist()
            require(len(infos)<=600 and sum(i.file_size for i in infos)<20_000_000,
                    "oversized ZIP payload")
            names=[i.filename for i in infos]
            require(len(set(names))==len(names), "duplicate member")
            for i in infos:
                p=PurePosixPath(i.filename)
                require(i.filename.startswith(P) and not i.is_dir() and
                        not p.is_absolute() and ".." not in p.parts and
                        str(p)==i.filename and "\\" not in i.filename and
                        ((i.external_attr>>16)&0o170000)!=0o120000 and
                        not (i.flag_bits&1), "unsafe member")
            tree={i.filename:z.read(i.filename) for i in infos}
    except AuditError:
        raise
    except Exception as e:
        raise AuditError("invalid ZIP or CRC") from e
    def doc(n):
        return parsed(tree[P+n])
    name=P+"PHASE3_PACKAGE_MANIFEST.json"
    require(name in tree,"missing package manifest")
    m=doc("PHASE3_PACKAGE_MANIFEST.json")
    no_release(m)
    require(m.get("schema")=="rgsml.work.phase3.delivery/1" and
            m.get("release_qualified") is False, "package is not NONRELEASE")
    require(m.get("original_asset_zip_sha256")==Q0_SHA and
            m.get("phase2_zip_sha256")==PHASE2_SHA, "source archive pins mismatch")
    files=m.get("files")
    require(type(files) is dict and
            set(tree)=={name}|{P+n for n in files}, "member inventory mismatch")
    for n,h in files.items():
        require(type(h) is str and len(h)==64 and sha(tree[P+n])==h,
                "member SHA mismatch: "+n)
    candidate=doc("Q0_CANONICAL_REVIEW_CANDIDATE_NONRELEASE.json")
    imported=doc("IMPORTED_Q0_CANONICAL_PREPARATION_NONRELEASE.json")
    coeff=doc("INDEPENDENT_COEFFICIENT_AUDIT.json")
    matrix=doc("Q0_Q6_MATRIX.json")
    for obj in (candidate,imported,coeff,matrix):
        no_release(obj)
    require(candidate.get("canonical_q0_approved") is False and
            candidate.get("product_admission_authorized") is False and
            candidate.get("release_qualified") is False, "not a pending Q0 review")
    for obj in (candidate,imported):
        require(obj.get("original_q0_zip_sha256")==Q0_SHA and
                obj.get("phase2_zip_sha256")==PHASE2_SHA, "broken Q0/Phase2 provenance")
    for key,n in (
        ("imported_spectral_receipt_sha256","IMPORTED_Q0_CANONICAL_PREPARATION_NONRELEASE.json"),
        ("coefficient_audit_sha256","INDEPENDENT_COEFFICIENT_AUDIT.json"),
        ("formal_error_review_sha256","FORMAL_ERROR_REVIEW.json"),
        ("guarded_replay_sha256","GUARDED_REPLAY_SUMMARY.json")):
        require(candidate.get(key)==sha(tree[P+n]), "candidate receipt pin mismatch: "+key)
    bands=imported.get("bands")
    require(type(bands) is list and imported.get("band_count")==28 and
            len(bands)==28 and candidate.get("spectral_band_proofs")==28,
            "incomplete spectral inventory")
    found=set()
    for row in bands:
        key=(row.get("L"),row.get("M"))
        kind=row.get("kind")
        require(key in RATIOS and kind in ("passband","stopband") and
                row.get("status")=="VERIFIED_NONRELEASE" and
                (key,kind) not in found, "bad/duplicate spectral band")
        try:
            lo,hi=Fraction(row["lo_hz"]),Fraction(row["hi_hz"])
        except (ValueError,KeyError,ZeroDivisionError) as e:
            raise AuditError("invalid band frequency") from e
        require(0<=lo<hi, "invalid band endpoints")
        found.add((key,kind))
    require(found=={(k,t) for k in RATIOS for t in ("passband","stopband")},
            "28 complete ratio/band keys required")
    runs=coeff.get("reproductions")
    require(coeff.get("ratios")==14 and coeff.get("precision_runs")==28 and
            coeff.get("original_assets_never_replaced") is True and
            candidate.get("independent_coefficient_ratios")==14 and
            candidate.get("independent_coefficient_precisions")==[256,512] and
            type(runs) is list and len(runs)==28, "incomplete coefficient inventory")
    seen=set(); assets={}
    for row in runs:
        key=(row.get("L"),row.get("M"))
        bits=row.get("bits")
        require(key in RATIOS and bits in (256,512) and
                (key,bits) not in seen, "duplicate/invalid coefficient run")
        seen.add((key,bits))
        h=row.get("asset_sha256")
        require(type(h) is str and len(h)==64 and
                (key not in assets or assets[key]==h), "coefficient SHA disagreement")
        assets[key]=h
        n=P+f"coefficient_reproduction/L{key[0]}_M{key[1]}_{bits}.json"
        require(n in tree and row.get("proof_receipt_sha256")==sha(tree[n]),
                "coefficient receipt not pinned")
        receipt=parsed(tree[n]);no_release(receipt)
        require((receipt.get("L"),receipt.get("M"),receipt.get("bits"))==(key[0],key[1],bits)
                and receipt.get("asset_sha256")==h
                and receipt.get("expected_sha256")==h
                and receipt.get("payload_bitwise_equal") is True
                and receipt.get("header_independent_regeneration") is True
                and receipt.get("all_binary64_rounding_cells_proven") is True,
                "incomplete rounding-cell receipt")
    require(seen=={(k,b) for k in RATIOS for b in (256,512)},
            "not all 14 assets reproduced at both precisions")
    replay=doc("GUARDED_REPLAY_SUMMARY.json").get("ratios")
    require(type(replay) is list and len(replay)==9 and all(
        r.get("full_numeric_bounds_identical") is True and
        r.get("all_taylor_coefficients_identical") is True for r in replay),
        "guarded producer replay incomplete")
    gates=matrix.get("gates")
    require(matrix.get("schema")=="rgsml.phase11.gate-preparation-matrix/1"
            and matrix.get("qtp_profile_state")=="PROVISIONAL"
            and matrix.get("release_qualified") is False
            and type(gates) is list and len(gates)==7
            and {g.get("gate") for g in gates}=={"Q0","Q1","Q2","Q3","Q4","Q5","Q6"}
            and all(g.get("status")=="OPEN" for g in gates),
            "Q0-Q6 status forged")
    return dict(result="INTEGRITY_ONLY_NONRELEASE",spectral_bands=28,
                source_kernel_ratios=14,coefficient_precision_runs=28,
                guarded_replays=9,canonical_q0_approved=False,
                product_admission_authorized=False,all_q0_q6_gates="OPEN")

if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase3_zip",type=Path)
    args=parser.parse_args()
    print(json.dumps(audit(args.phase3_zip.read_bytes()),sort_keys=True))
