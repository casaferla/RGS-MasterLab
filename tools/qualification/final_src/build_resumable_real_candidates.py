#!/usr/bin/env python3
"""Resumable REAL MPFR/Ziv Phase-11 14-asset CANDIDATE workstation.

Each reduced ratio executes the SAME generator twice and checkpoints only
byte-identical, structurally valid results. Interrupted long-running batches
reuse immutable verified per-ratio checkpoints; final 14-asset publication
remains atomic through the existing assembler. No release, Q1-Q6, DTFT,
independent coefficient authority or product SRC admission is implied.

CLI: --workdir NEW_OR_EXISTING --stage [--fin F --fout F] [--max-precision N]
     --workdir EXISTING --finalize --out NEW_FOLDER
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import sys
import tempfile
from typing import Callable

from build_candidate_manifest import assemble_candidate, canonical_pair_for_ratio
from generate_rgsmsr1_mpfr import generate
from phase11_math_preflight import ContractError, Ratio, audit_rgsmsr1_structure, kernel_shape

RECEIPT = "DOUBLE_RUN_CHECKPOINT_NOT_FOR_RELEASE.json"
RECIPE = "PHASE11_MPFR_ZIV_RESUMABLE_CANDIDATE_ONLY_V1"
BACKEND = "REAL_MPFR_ZIV"
TEST_BACKEND = "INJECTED_TEST_ONLY_NOT_MPFR"


def _key(fin: int, fout: int) -> tuple[tuple[int, int], tuple[int, int]]:
    ratio = Ratio.make(fin, fout)
    key = (ratio.L, ratio.M)
    pair = canonical_pair_for_ratio().get(key)
    if pair != (fin, fout):
        raise ContractError("only canonical nonidentity rate pairs may be staged")
    return key, pair


def _slot(workdir: Path, L: int, M: int) -> Path:
    return workdir / f"ratio_L{L}_M{M}"


def _read_checkpoint(workdir: Path, fin: int, fout: int, *,
                     allow_test_generator: bool = False) -> tuple[bytes, int]:
    (L, M), _ = _key(fin, fout)
    slot = _slot(workdir, L, M)
    if slot.is_symlink() or not slot.is_dir():
        raise ContractError("ratio checkpoint missing, incomplete or symlink")
    filename = f"RGSMSR1_L{L}_M{M}.bin"
    if {p.name for p in slot.iterdir()} != {filename, RECEIPT}:
        raise ContractError("ratio checkpoint has missing or unexpected files")
    data = (slot / filename).read_bytes()
    receipt = json.loads((slot / RECEIPT).read_text(encoding="utf-8"))
    if not isinstance(receipt, dict):
        raise ContractError("ratio checkpoint receipt is invalid")
    shape = kernel_shape(Ratio.make(fin, fout))
    digest = sha256(data).hexdigest()
    precision = receipt.get("precision_bits")
    if (receipt.get("recipe") != RECIPE
        or receipt.get("fin") != fin or receipt.get("fout") != fout
        or receipt.get("L") != L or receipt.get("M") != M
        or receipt.get("T") != shape.T or receipt.get("N") != shape.N
        or receipt.get("bytes") != shape.frozen_bytes
        or receipt.get("sha256") != digest
        or receipt.get("same_generator_double_run") is not True
        or receipt.get("product_admission_authorized") is not False
        or receipt.get("release_qualified") is not False
        or type(precision) is not int or precision < 256
        or precision & (precision - 1) != 0
        or receipt.get("backend") not in (
            (BACKEND, TEST_BACKEND) if allow_test_generator else (BACKEND,))):
        raise ContractError("ratio checkpoint provenance/geometry/hash invalid")
    if audit_rgsmsr1_structure(data, shape) != digest:
        raise ContractError("ratio checkpoint exact-binary64 shape invalid")
    return data, precision


def stage_one(workdir: Path, fin: int, fout: int, *,
              max_precision: int = 1024,
              generator: Callable[[int, int, int], tuple[bytes, int]] | None = None
              ) -> tuple[bytes, int]:
    (L, M), _ = _key(fin, fout)
    if type(max_precision) is not int or max_precision < 256:
        raise ContractError("MPFR/Ziv budget must be >=256 bits")
    if workdir.is_symlink():
        raise ContractError("work directory may not be a symlink")
    workdir.mkdir(parents=True, exist_ok=True)
    slot = _slot(workdir, L, M)
    if slot.exists() or slot.is_symlink():
        data, bits = _read_checkpoint(
            workdir, fin, fout, allow_test_generator=generator is not None)
        if bits > max_precision:
            raise ContractError("cached proof precision exceeds requested budget")
        return data, bits

    backend = (lambda a,b,n: generate(a,b,max_precision=n)) if generator is None else generator
    first, bits1 = backend(fin, fout, max_precision)
    second, bits2 = backend(fin, fout, max_precision)
    if first != second or bits1 != bits2:
        raise ContractError("DOUBLE_RUN_REPRODUCTION_MISMATCH: checkpoint not published")
    if type(bits1) is not int or bits1 < 256 or bits1 > max_precision or bits1 & (bits1-1):
        raise ContractError("MPFR/Ziv double-run precision invalid")
    shape = kernel_shape(Ratio.make(fin, fout))
    digest = audit_rgsmsr1_structure(first, shape)
    if digest != sha256(first).hexdigest():
        raise ContractError("MPFR/Ziv checkpoint checksum mismatch")
    receipt = {
        "recipe": RECIPE, "backend": BACKEND if generator is None else TEST_BACKEND,
        "fin": fin, "fout": fout, "L": L, "M": M,
        "T": shape.T, "N": shape.N, "bytes": len(first),
        "sha256": digest, "precision_bits": bits1,
        "same_generator_double_run": True,
        "independent_generator_verified": False,
        "mpfr_dtft_oracle_passed": False, "q1_q6_passed": False,
        "release_qualified": False, "product_admission_authorized": False,
    }
    with tempfile.TemporaryDirectory(prefix=".incomplete-ratio-",dir=workdir) as tempdir:
        temp = Path(tempdir)
        (temp / f"RGSMSR1_L{L}_M{M}.bin").write_bytes(first)
        (temp / RECEIPT).write_text(
            json.dumps(receipt,indent=2,sort_keys=True)+"\n",encoding="utf-8")
        if slot.exists():
            raise ContractError("concurrent ratio checkpoint publication refused")
        temp.rename(slot)
    return _read_checkpoint(workdir,fin,fout,
                            allow_test_generator=generator is not None)


def stage_all(workdir: Path, *, max_precision: int = 1024,
              generator: Callable[[int,int,int],tuple[bytes,int]] | None = None,
              progress: Callable[[str], None] | None = None
              ) -> dict:
    """Durable ratio checkpoints; a later failed ratio cannot destroy earlier work."""
    pairmap = canonical_pair_for_ratio()
    done = []
    for (L,M),(fin,fout) in pairmap.items():
        if progress:
            progress(f"MPFR_RATIO_START L{L}/M{M} fin={fin} fout={fout}")
        data,bits=stage_one(workdir,fin,fout,
                            max_precision=max_precision,generator=generator)
        done.append({"L":L,"M":M,"bytes":len(data),"precision_bits":bits,
                     "sha256":sha256(data).hexdigest()})
        if progress:
            progress(f"MPFR_RATIO_CHECKPOINTED {len(done)}/14 L{L}/M{M} "
                     f"bits={bits} sha256={sha256(data).hexdigest()}")
    return {"status":"14_DOUBLE_RUN_CANDIDATES_CHECKPOINTED_NOT_RELEASE",
            "ratios":len(done),"assets":done,
            "product_admission_authorized":False}


def finalize(workdir: Path, destination: Path, *,
             max_precision: int = 1024,
             allow_test_fixture: bool = False,
             independent_screen: Callable[[bytes,int,int],dict] | None = None,
             dyadic_screen: Callable[[bytes,int,int],dict] | None = None,
             sampled_fft_screen: Callable[[bytes,int,int,str],dict] | None = None,
             exact_anchor_screen: Callable[[bytes,int,int],dict] | None = None
             ) -> dict:
    """Publish complete 14 ratio family atomically; no generation on finalize.

    The assembler reads immutable validated DOUBLE-COMPUTED checkpoint bytes
    twice, not two new MPFR executions. The original two runs are recorded
    only by each checkpoint receipt. No independent oracle is claimed.
    """
    pairmap=canonical_pair_for_ratio()
    if not workdir.is_dir() or workdir.is_symlink():
        raise ContractError("missing ratio staging workdir")
    expected={f"ratio_L{L}_M{M}" for L,M in pairmap}
    observed={p.name for p in workdir.iterdir()}
    if observed!=expected:
        raise ContractError("all 14 checkpoints required; no extra files allowed")
    verified={}
    for (_L,_M),(fin,fout) in pairmap.items():
        verified[(fin,fout)]=_read_checkpoint(
            workdir,fin,fout,allow_test_generator=allow_test_fixture)
    def from_checked_checkpoint(fin: int, fout: int, precision: int):
        data,bits=verified[(fin,fout)]
        if bits>precision:
            raise ContractError("checkpoint uses higher precision than assembler budget")
        return data,bits
    result=assemble_candidate(
        destination,generator=from_checked_checkpoint,
        max_precision=max_precision,
        independent_screen=independent_screen,
        dyadic_screen=dyadic_screen,
        sampled_fft_screen=sampled_fft_screen,
        exact_anchor_screen=exact_anchor_screen)
    if result["observed_ratio_count"]!=14 or not result["complete_ratio_family"]:
        raise ContractError("complete candidate manifest required")
    return result


def main() -> None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--workdir",type=Path,required=True)
    action=p.add_mutually_exclusive_group(required=True)
    action.add_argument("--stage",action="store_true")
    action.add_argument("--finalize",action="store_true")
    p.add_argument("--fin",type=int)
    p.add_argument("--fout",type=int)
    p.add_argument("--max-precision",type=int,default=1024)
    p.add_argument("--out",type=Path)
    p.add_argument("--screen-independent-mpmath",action="store_true")
    p.add_argument("--screen-exact-dyadic",action="store_true")
    p.add_argument("--screen-sampled-fft",action="store_true")
    p.add_argument("--screen-exact-spectral-anchors",action="store_true")
    a=p.parse_args()
    if a.stage:
        if (a.out is not None or (a.fin is None)!=(a.fout is None)
            or any((a.screen_independent_mpmath,a.screen_exact_dyadic,
                    a.screen_sampled_fft,a.screen_exact_spectral_anchors))):
            p.error("--stage takes both --fin/--fout or neither, without --out/--screen flags")
        if a.fin is None:
            result=stage_all(a.workdir,max_precision=a.max_precision,
                progress=lambda line: print(line,file=sys.stderr,flush=True))
        else:
            data,bits=stage_one(a.workdir,a.fin,a.fout,max_precision=a.max_precision)
            result={"status":"RATIO_DOUBLE_RUN_CHECKPOINTED_NOT_RELEASE",
                    "fin":a.fin,"fout":a.fout,"precision_bits":bits,
                    "bytes":len(data),"sha256":sha256(data).hexdigest(),
                    "product_admission_authorized":False}
    else:
        if a.out is None or a.fin is not None or a.fout is not None:
            p.error("--finalize requires --out, without --fin/--fout")
        independent=None
        if a.screen_independent_mpmath:
            from independent_mpmath_crosscheck import compare_candidate
            independent=compare_candidate
        dyadic=None
        if a.screen_exact_dyadic:
            from exact_dyadic_zero_stuff_oracle import screen_real_candidate_dyadic
            dyadic=screen_real_candidate_dyadic
        fft=None
        if a.screen_sampled_fft:
            from screen_candidate_spectral_response import screen_asset
            fft=screen_asset
        anchor=None
        if a.screen_exact_spectral_anchors:
            from exact_dyadic_spectral_anchors import screen_exact_spectral_anchors
            anchor=screen_exact_spectral_anchors
        result=finalize(
            a.workdir,a.out,max_precision=a.max_precision,
            independent_screen=independent,dyadic_screen=dyadic,
            sampled_fft_screen=fft,exact_anchor_screen=anchor)
    print(json.dumps(result,sort_keys=True))


if __name__=="__main__":
    main()
