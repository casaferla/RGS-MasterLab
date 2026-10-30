#!/usr/bin/env python3
"""Archive/restore private MPFR continuum checkpoint receipts, never FIR bytes.

NONRELEASE transport only. Original independently SHA-pinned Q0 ZIP plus the
local exact MPFR proof-engine SHA are required. Restores into a NEW directory
atomically; incomplete/altered/symlinked ZIP entries must fail closed.
"""
from __future__ import annotations
import argparse
from fractions import Fraction
from hashlib import sha256
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
from zipfile import ZipFile, BadZipFile, ZIP_DEFLATED

from build_candidate_manifest import canonical_pair_for_ratio
from phase11_math_preflight import ContractError
from prepare_real_q1_36_native_rehearsal import CANDIDATE_ZIP_SHA
from run_resumable_14_mpfr_continuum import _exact_record, SCHEMA

ARCHIVE_SCHEMA = "rgsml.phase11.real14-mpfr-continuum-checkpoint-transport/1"
NAME = re.compile(r"L([1-9][0-9]*)_M([1-9][0-9]*)_(passband|stopband)_r([01]{0,24})\.json")
MAX_COUNT = 200000
MAX_RECEIPT = 5000
MAX_MANIFEST = 30_000_000


def _sha(b: bytes) -> str:
    return sha256(b).hexdigest()


def _authority(q0_zip: Path) -> tuple[str, dict, str]:
    if not q0_zip.is_file() or q0_zip.is_symlink():
        raise ContractError("original Q0 ZIP missing or unsafe")
    actual = _sha(q0_zip.read_bytes())
    if actual != CANDIDATE_ZIP_SHA:
        raise ContractError("frozen original Q0 ZIP SHA mismatch")
    try:
        with ZipFile(q0_zip) as z:
            records = json.loads(z.read("CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json"))
    except (BadZipFile, KeyError, ValueError, OSError) as e:
        raise ContractError("frozen Q0 manifest unreadable") from e
    rows = records.get("assets") if isinstance(records, dict) else None
    pairs = canonical_pair_for_ratio()
    if not isinstance(rows, list) or len(rows) != 14:
        raise ContractError("frozen Q0 family not complete")
    pins = {}
    for row in rows:
        if not isinstance(row, dict):
            raise ContractError("invalid frozen Q0 row")
        pair = (row.get("L"),row.get("M"))
        digest=row.get("candidate_sha256")
        if pair not in pairs or pair in pins or not isinstance(digest,str) or not re.fullmatch(r"[a-f0-9]{64}",digest):
            raise ContractError("frozen Q0 ratio/SHA not canonical")
        pins[pair] = digest
    if set(pins) != set(pairs):
        raise ContractError("frozen Q0 ratios incomplete")
    engine = Path(__file__).with_name("dtft_third_order_rigorous.py")
    if not engine.is_file():
        raise ContractError("MPFR proof engine source missing")
    return actual,pins,_sha(engine.read_bytes())


def _receipt(name: str, content: bytes, pins: dict, engine_sha: str) -> dict:
    if len(content) > MAX_RECEIPT or not content:
        raise ContractError("checkpoint too large/empty")
    match = NAME.fullmatch(name)
    if match is None:
        raise ContractError("unsafe/unexpected checkpoint filename")
    L,M,kind,route=int(match[1]),int(match[2]),match[3],match[4]
    pairs=canonical_pair_for_ratio()
    pair=(L,M)
    if pair not in pins or pair not in pairs:
        raise ContractError("checkpoint references unknown frozen Q0 ratio")
    fin,fout=pairs[pair]
    common=min(fin,fout)
    lo,hi=((Fraction(0),Fraction(12*common,25)) if kind=="passband"
           else (Fraction(common,2),Fraction(fin*L,2)))
    for bit in route:
        mid=(lo+hi)/2
        if bit=="0": hi=mid
        else: lo=mid
    expected=dict(schema=SCHEMA,fin=fin,fout=fout,L=L,M=M,kind=kind,
                  lo_hz=str(lo),hi_hz=str(hi),candidate_sha256=pins[pair],
                  engine_sha256=engine_sha,bits=256)
    try:
        r=json.loads(content)
    except (UnicodeError,ValueError) as e:
        raise ContractError("invalid receipt JSON") from e
    if not isinstance(r,dict) or any(r.get(k)!=v for k,v in expected.items()):
        raise ContractError("checkpoint frequency interval/source/engine SHA mismatch")
    if r.get("release_qualified") is not False or r.get("product_admission_authorized") is not False:
        raise ContractError("release forgery in checkpoint")
    result=r.get("proof_result")
    if not isinstance(result,dict) or result.get("release_qualified") is not False:
        raise ContractError("checkpoint proof result missing")
    if r.get("status")=="PROVEN":
        if result.get("status")!="CONTINUUM_SUBBAND_PROVEN_NONRELEASE" or result.get("remaining_boxes")!=0 or type(result.get("proven_boxes")) is not int or result["proven_boxes"]<1:
            raise ContractError("false complete continuum checkpoint")
    elif r.get("status")=="SPLIT_UNRESOLVED":
        if result.get("status")!="UNRESOLVED":
            raise ContractError("invalid unresolved split")
    else:
        raise ContractError("unexpected checkpoint status")
    return r


def _validated_files(checkpoints: Path, pins: dict, engine: str) -> dict[str,bytes]:
    if not checkpoints.is_dir() or checkpoints.is_symlink():
        raise ContractError("private checkpoints folder missing/unsafe")
    entries=list(checkpoints.iterdir())
    if not entries or len(entries)>MAX_COUNT:
        raise ContractError("checkpoint count outside supported bounds")
    result={}
    for p in entries:
        if not p.is_file() or p.is_symlink():
            raise ContractError("unexpected checkpoint member/symlink")
        content=p.read_bytes()
        _receipt(p.name,content,pins,engine)
        result[p.name]=content
    return result


def package(checkpoints:Path, original_q0_zip:Path, output:Path) -> dict:
    if output.exists() or output.is_symlink() or not output.parent.is_dir() or output.parent.is_symlink():
        raise ContractError("checkpoint transport requires NEW ZIP destination")
    if output.resolve().is_relative_to(checkpoints.resolve()):
        raise ContractError("checkpoint transport must be outside checkpoint root")
    original,pins,engine=_authority(original_q0_zip)
    files=_validated_files(checkpoints,pins,engine)
    manifest={"schema":ARCHIVE_SCHEMA,"original_q0_zip_sha256":original,
              "mpfr_engine_sha256":engine,
              "receipts_sha256":{k:_sha(v) for k,v in sorted(files.items())},
              "count":len(files),"release_qualified":False,
              "product_admission_authorized":False,"full_band_proof_certified":False}
    fd,temp=tempfile.mkstemp(prefix=".rgs-q0-proof-",suffix=".zip",dir=output.parent)
    os.close(fd)
    try:
        with ZipFile(temp,"w",ZIP_DEFLATED) as z:
            z.writestr("MANIFEST_NONRELEASE.json",json.dumps(manifest,sort_keys=True,indent=2)+"\n")
            for name,content in sorted(files.items()):
                z.writestr("receipts/"+name,content)
        # Verify before exposing a user-visible archive; no private FIR bytes.
        verify(Path(temp),original_q0_zip)
        try: os.link(temp,output)
        except FileExistsError as exc:
            raise ContractError("concurrent ZIP publication: destination exists") from exc
    finally:
        Path(temp).unlink(missing_ok=True)
    return manifest


def verify(archive:Path, original_q0_zip:Path) -> tuple[dict,dict[str,bytes]]:
    original,pins,engine=_authority(original_q0_zip)
    if not archive.is_file() or archive.is_symlink() or archive.stat().st_size>200_000_000:
        raise ContractError("checkpoint ZIP absent/oversized/unsafe")
    try:
        with ZipFile(archive) as z:
            infos=z.infolist()
            names=[i.filename for i in infos]
            if len(names)!=len(set(names)) or "MANIFEST_NONRELEASE.json" not in names:
                raise ContractError("duplicate/missing checkpoint ZIP manifest")
            if z.getinfo("MANIFEST_NONRELEASE.json").file_size>MAX_MANIFEST:
                raise ContractError("oversized checkpoint manifest")
            manifest=json.loads(z.read("MANIFEST_NONRELEASE.json"))
            if not isinstance(manifest,dict):raise ContractError("malformed archive manifest")
            hashes=manifest.get("receipts_sha256")
            if (manifest.get("schema")!=ARCHIVE_SCHEMA
                or manifest.get("original_q0_zip_sha256")!=original
                or manifest.get("mpfr_engine_sha256")!=engine
                or manifest.get("release_qualified") is not False
                or manifest.get("product_admission_authorized") is not False
                or manifest.get("full_band_proof_certified") is not False
                or not isinstance(hashes,dict) or not hashes
                or len(hashes)>MAX_COUNT or manifest.get("count")!=len(hashes)
                or set(names)!={"MANIFEST_NONRELEASE.json"}|{"receipts/"+n for n in hashes}):
                raise ContractError("incomplete or falsely certified checkpoint ZIP")
            files={}
            for name,digest in hashes.items():
                if not isinstance(name,str) or NAME.fullmatch(name) is None or not isinstance(digest,str) or not re.fullmatch(r"[a-f0-9]{64}",digest):
                    raise ContractError("unexpected checkpoint receipt name/hash")
                entry=z.getinfo("receipts/"+name)
                if (entry.is_dir() or entry.file_size>MAX_RECEIPT or (entry.external_attr>>16)&0o170000==0o120000):
                    raise ContractError("unsafe/incomplete checkpoint ZIP member")
                blob=z.read(entry)
                if _sha(blob)!=digest:raise ContractError("modified checkpoint ZIP SHA")
                _receipt(name,blob,pins,engine)
                files[name]=blob
            return manifest,files
    except (BadZipFile,KeyError,OSError,ValueError,UnicodeError) as e:
        raise ContractError("checkpoint ZIP corrupt/incomplete") from e


def restore(archive:Path,original_q0_zip:Path,output:Path)->dict:
    if output.exists() or output.is_symlink() or not output.parent.is_dir() or output.parent.is_symlink():
        raise ContractError("restore needs NEW checkpoint directory")
    manifest,files=verify(archive,original_q0_zip)
    tmp=Path(tempfile.mkdtemp(prefix=".rgs-q0-restore-",dir=output.parent))
    try:
        for name,data in files.items():
            (tmp/name).write_bytes(data)
        _validated_files(tmp,_authority(original_q0_zip)[1],manifest["mpfr_engine_sha256"])
        if output.exists() or output.is_symlink():
            raise ContractError("concurrent checkpoint directory appeared")
        tmp.rename(output)
    finally:
        if tmp.exists():shutil.rmtree(tmp)
    return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("action",choices=("package","verify","restore"))
    p.add_argument("--checkpoint-dir",type=Path)
    p.add_argument("--original-q0-zip",required=True,type=Path)
    p.add_argument("--archive",required=True,type=Path)
    p.add_argument("--new-checkpoint-dir",type=Path)
    a=p.parse_args()
    if a.action=="package":
        if not a.checkpoint_dir:p.error("--checkpoint-dir required")
        result=package(a.checkpoint_dir,a.original_q0_zip,a.archive)
    elif a.action=="restore":
        if not a.new_checkpoint_dir:p.error("--new-checkpoint-dir required")
        result=restore(a.archive,a.original_q0_zip,a.new_checkpoint_dir)
    else:result=verify(a.archive,a.original_q0_zip)[0]
    print(json.dumps({"status":"PRIVATE_NONRELEASE_CHECKPOINT_TRANSPORT_VALIDATED",
                      "count":result["count"],"full_band_proof_certified":False,
                      "product_admission_authorized":False},sort_keys=True))

if __name__=="__main__":main()