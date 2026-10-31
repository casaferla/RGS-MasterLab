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
PUBLIC_PLAYBACK_PINS={
    "playback_src_kernel_44100_48000.bin":
        "ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4",
    "playback_src_kernel_48000_44100.bin":
        "37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22",
}
PUBLIC_PLAYBACK_RELATIVE="resources/fir/playback"
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



def verify_archive(archive: Path, *, maximum_bytes=120_000_000) -> dict:
    """Fail closed on incomplete, altered, extra, duplicate and unsafe Qt bundles."""
    if not archive.is_file() or archive.suffixes[-2:] != [".tar", ".gz"]:
        raise ContractError("Qt native archive missing or invalid")
    try:
        with tarfile.open(archive,"r:gz") as tar:
            members=tar.getmembers()
            names=[m.name for m in members]
            if len(names)!=len(set(names)):
                raise ContractError("duplicate Qt tar member")
            root="rgsml_offline_qt_native"
            files={root+"/rgsml_playback_src_tests",root+"/PROVENANCE_NONRELEASE.json",root+"/RUN_OFFLINE.txt"}
            public_files={root+"/"+PUBLIC_PLAYBACK_RELATIVE+"/"+name
                          for name in PUBLIC_PLAYBACK_PINS}
            files.update(public_files)
            dirs={root,root+"/lib",root+"/resources",root+"/resources/fir",
                  root+"/"+PUBLIC_PLAYBACK_RELATIVE}
            mapping={m.name:m for m in members}
            if not files.issubset(mapping) or not dirs.issubset(mapping):
                raise ContractError("Qt bundle incomplete")
            if any(not mapping[d].isdir() for d in dirs) or any(not m.isfile() for m in members if m.name not in dirs):
                raise ContractError("unsafe Qt bundle member")
            provenance=mapping[root+"/PROVENANCE_NONRELEASE.json"]
            if provenance.size>50000:
                raise ContractError("oversized Qt manifest")
            manifest=json.loads(tar.extractfile(provenance).read())
            if not isinstance(manifest,dict) or not isinstance(manifest.get("dependencies"),dict):
                raise ContractError("invalid Qt provenance")
            libs=manifest["dependencies"]
            if manifest.get("external_playback_fir_sha256")!=PUBLIC_PLAYBACK_PINS:
                raise ContractError("Qt native playback FIR pin inventory mismatch")
            if not libs or any(not isinstance(k,str) or not re.fullmatch(r"lib[A-Za-z0-9_+.-]+\.so(?:\.[0-9]+)*",k)
                               or not isinstance(h,str) or not re.fullmatch(r"[a-f0-9]{64}",h)
                               for k,h in libs.items()):
                raise ContractError("unsafe Qt dependency entry")
            if set(names)!=files|dirs|{root+"/lib/"+k for k in libs}:
                raise ContractError("Qt archive has missing or extra members")
            if (manifest.get("schema")!="rgsml.phase11.qt-native-test-exe-portable-nonrelease/1"
                or any(manifest.get(k) is not False for k in ("native_archive_contains_private_q0_q1",
                    "native_complete_14_q0_36_q1_test_executed","product_admission_authorized","release_qualified"))
                or not any(k.startswith("libQt6Core.so") for k in libs)
                or not any(k.startswith("libQt6Test.so") for k in libs)):
                raise ContractError("Qt bundle provenance/Qt libraries invalid")
            if not mapping[root+"/rgsml_playback_src_tests"].mode & 0o111:
                raise ContractError("Qt ELF execute bit missing")
            expected={root+"/rgsml_playback_src_tests":manifest.get("native_elf_sha256")}
            expected.update({root+"/lib/"+k:h for k,h in libs.items()})
            expected.update({root+"/"+PUBLIC_PLAYBACK_RELATIVE+"/"+k:h
                             for k,h in PUBLIC_PLAYBACK_PINS.items()})
            total=0
            for filename,want in expected.items():
                if not isinstance(want,str) or not re.fullmatch(r"[a-f0-9]{64}",want):
                    raise ContractError("missing valid Qt SHA256")
                total+=mapping[filename].size
                if total>maximum_bytes:
                    raise ContractError("Qt bundle oversized")
                f=tar.extractfile(mapping[filename])
                if f is None:
                    raise ContractError("Qt bundle unreadable")
                h=sha256()
                for block in iter(lambda:f.read(1048576),b""): h.update(block)
                if h.hexdigest()!=want:
                    raise ContractError("Qt member checksum mismatch")
            return manifest
    except (OSError,tarfile.TarError,ValueError,UnicodeError,TypeError,KeyError) as exc:
        raise ContractError("invalid or incomplete Qt archive") from exc

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
    # Only the two already-public, frozen playback FIRs are permitted here.
    # No private original 14 Final SRC files may enter the CI archive.
    playback_source=build_dir/"generated/rgsml/fir/playback"
    for name,want in PUBLIC_PLAYBACK_PINS.items():
        p=playback_source/name
        if not p.is_file() or digest(p)!=want:
            raise ContractError("portable Qt frozen playback FIR absent/tampered: "+name)
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
        playback_dest=package/PUBLIC_PLAYBACK_RELATIVE
        playback_dest.mkdir(parents=True)
        for name in PUBLIC_PLAYBACK_PINS:
            shutil.copy2(playback_source/name,playback_dest/name)
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
            "external_playback_fir_sha256":dict(PUBLIC_PLAYBACK_PINS),
        }
        total=target.stat().st_size+sum(
            (playback_dest/name).stat().st_size for name in PUBLIC_PLAYBACK_PINS)
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
            "TEST EXECUTABLE + TWO PUBLIC FROZEN PLAYBACK FIRs; no private Q0/Q1 shipped.\n"
            "For native Qt invocation, set RGSML_PLAYBACK_FIR_ROOT=$PWD/resources/fir/playback "
            "and LD_LIBRARY_PATH=$PWD/lib; "
            "and supply original two private SHA-locked ZIPs to "
            "tools/qualification/final_src/run_phase11_private_m17_bulk_gate.py.\n"
            "This package does NOT certify Q0 full DTFT or Q1-Q6 release.\n")
        with tarfile.open(output,"w:gz") as tar:
            tar.add(package,arcname="rgsml_offline_qt_native")
        checked=verify_archive(output,maximum_bytes=maximum_bytes)
        if checked["native_elf_sha256"]!=manifest["native_elf_sha256"] or checked["dependencies"]!=manifest["dependencies"]:
            raise ContractError("Qt postpack provenance changed")
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
    p.add_argument("--build-dir",type=Path)
    p.add_argument("--verify-only",action="store_true")
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    if a.verify_only:
        record=verify_archive(a.output)
        print(json.dumps({"status":"QT_NONRELEASE_INTEGRITY_PASS","dependencies":len(record["dependencies"]),"native_elf_sha256":record["native_elf_sha256"],"release_qualified":False},sort_keys=True))
        return
    if a.build_dir is None:
        p.error("--build-dir required for packing")
    record=pack(a.build_dir,a.output)
    print(json.dumps({
        "schema":record["schema"],
        "native_elf_sha256":record["native_elf_sha256"],
        "bundle_tar_gz_sha256":record["bundle_tar_gz_sha256"],
        "dependency_count":len(record["dependencies"]),
        "product_admission_authorized":False},sort_keys=True))


if __name__=="__main__":
    main()
