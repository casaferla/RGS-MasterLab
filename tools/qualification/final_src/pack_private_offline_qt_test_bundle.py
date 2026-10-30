#!/usr/bin/env python3
"""Package the *real GitHub-built Qt test binary* for private offline verification.

Distributes NO Q0/Q1 coefficients or audio. Bundle is NOT a product build or
release. Exact Linux elf+non-glibc ldd dependencies and SHA256 manifest go
into a tar.gz; the original compiled test is later run with PRIVATE data.
"""
from __future__ import annotations
import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile

from phase11_math_preflight import ContractError

EXE_NAME="rgsml_playback_src_tests"
SKIP_SYSTEM=re.compile(
    r"^(?:linux-vdso|ld-linux|libc\.|libm\.|libpthread\.|"
    r"librt\.|libdl\.|libresolv\.|libgcc_s\.|libstdc\+\+\.)"
)


def digest(path: Path) -> str:
    h=sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024*1024),b""):
            h.update(block)
    return h.hexdigest()


def parse_ldd(text: str)->list[Path]:
    if "not found" in text or "statically linked" in text:
        raise ContractError("Qt native test dependencies are missing")
    found={}
    for line in text.splitlines():
        match=re.match(r"^\s*([^\s]+)\s+=>\s+(/[^\s]+)",line)
        if not match:
            continue
        name,path=match.groups()
        if SKIP_SYSTEM.match(name):
            continue
        p=Path(path)
        if not p.is_file():
            raise ContractError("native Qt dependency path is not a file")
        if name in found and found[name]!=p:
            raise ContractError("native Qt library basename collision")
        found[name]=p
    if not any(name.startswith("libQt6Core.so") for name in found):
        raise ContractError("actual Qt6Core dependency not present")
    if not any(name.startswith("libQt6Test.so") for name in found):
        raise ContractError("actual Qt6Test dependency not present")
    return list(found.values())


def pack(build_dir:Path,output:Path,*,maximum_bytes=120_000_000)->dict:
    if output.exists() or output.is_symlink() or not output.parent.is_dir():
        raise ContractError("Qt archive path must be new")
    if output.suffixes[-2:]!=[".tar",".gz"]:
        raise ContractError("Qt bundle must preserve ELF permissions in tar.gz")
    if type(maximum_bytes) is not int or maximum_bytes<1_000_000:
        raise ContractError("native offline bundle size guard invalid")
    executables=list(build_dir.rglob(EXE_NAME))
    executables=[p for p in executables if p.is_file() and os.access(p,os.X_OK)]
    if len(executables)!=1:
        raise ContractError("expected exactly one compiled native Qt SRC test")
    exe=executables[0]
    result=subprocess.run(
        ["ldd",str(exe.resolve())],
        capture_output=True,text=True,check=False,timeout=20)
    if result.returncode!=0:
        raise ContractError("actual Qt SRC test dynamic library audit failed")
    libs=parse_ldd(result.stdout)
    scratch=Path(tempfile.mkdtemp(prefix=".rgsml-private-native-test-",dir=output.parent))
    try:
        package=scratch/"rgsml_offline_qt_native"
        package.mkdir()
        target=package/EXE_NAME
        shutil.copy2(exe,target)
        target.chmod(target.stat().st_mode|0o111)
        libdir=package/"lib"
        libdir.mkdir()
        manifest={
            "schema":"rgsml.phase11.qt-native-test-exe-portable-nonrelease/1",
            "native_elf_sha256":digest(target),
            "runtime_platform":platform.platform(),
            "focused_ci_sha":os.environ.get("GITHUB_SHA","UNQUALIFIED_OFFLINE"),
            "focused_ci_run_id":os.environ.get("GITHUB_RUN_ID","UNQUALIFIED_OFFLINE"),
            "native_archive_contains_private_q0_q1":False,
            "native_complete_14_q0_36_q1_test_executed":False,
            "product_admission_authorized":False,
            "release_qualified":False,
            "dependencies":{},
        }
        total=target.stat().st_size
        for source in libs:
            dest=libdir/source.name
            if dest.exists():
                if digest(source)!=digest(dest):
                    raise ContractError("duplicate Qt native dependency collision")
                continue
            shutil.copy2(source,dest)
            total+=dest.stat().st_size
            if total>maximum_bytes:
                raise ContractError("Qt native bundle exceeds bounded size")
            manifest["dependencies"][dest.name]=digest(dest)
        (package/"PROVENANCE_NONRELEASE.json").write_text(
            json.dumps(manifest,sort_keys=True,indent=2)+"\n")
        (package/"RUN_OFFLINE.txt").write_text(
            "TEST EXECUTABLE ONLY: no private FIR coefficients shipped.\n"
            "Run from this directory with LD_LIBRARY_PATH=$PWD/lib set, "
            "and supply original two private SHA-locked ZIPs to "
            "tools/qualification/final_src/run_phase11_private_m17_bulk_gate.py.\n"
            "This package does NOT certify Q0 full DTFT or Q1-Q6 release.\n")
        with tarfile.open(output,"w:gz") as tar:
            tar.add(package,arcname="rgsml_offline_qt_native")
        manifest["bundle_tar_gz_sha256"]=digest(output)
        manifest["bundle_uncompressed_bytes"]=total
        return manifest
    except BaseException:
        # A half-written transport archive is never an acceptable receipt.
        output.unlink(missing_ok=True)
        raise
    finally:
        shutil.rmtree(scratch,ignore_errors=True)


def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build-dir",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    record=pack(a.build_dir,a.output)
    print(json.dumps({
        "schema":record["schema"],
        "native_elf_sha256":record["native_elf_sha256"],
        "bundle_tar_gz_sha256":record["bundle_tar_gz_sha256"],
        "dependency_count":len(record["dependencies"]),
        "product_admission_authorized":False},sort_keys=True))


if __name__=="__main__":
    main()
