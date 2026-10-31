#!/usr/bin/env python3
"""Install ONLY the frozen private Q0 originals for an explicit local Windows M17 FQ.
No network, hidden fallback, source-code embedding or change of release authority.
The ZIP and original FIR binaries must stay outside the PUBLIC repository.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import tempfile
from zipfile import ZipFile, BadZipFile

ORIGINAL_ZIP_SHA256 = "94d3340667ab5c86b2db5f2d0e191f58b5d05863413312fb3545faed51798ce4"
EXPECTED = {
    (1,2):("b97776527687180bea6dbf86a8da877c0bdaa528d305abe4b7beaf806ecaf48e",991),
    (1,4):("3c5db068904a059fae54ab3a96ae96ec3963fc31e649f5f4de9ecf5b6d17a947",1981),
    (2,1):("6fb38a7d62859283e4061a18d23f354a8308d5b22ff55623e8b1c9a245e8b00c",1025),
    (4,1):("f2563518411bc6525889eaf82c43b194f88ceadd59b0750071c613d1071de095",2049),
    (40,147):("7658f824ca67beab44a3dee35ccbdaae3aa0acdfc9257c327a9eaa7f3851928f",72721),
    (80,147):("42d55a83855e1f166819ce4c3ad6bd53293ddb76a51afd5b1733c520be7ec5f0",72801),
    (147,40):("fd57c100ca171a73a3bae4e61c4cc8faca64deff722b363375e4a6fae51f65ef",75265),
    (147,80):("38f23112566f59137383d9ba58a0e47a80c583b1232bbbf88315b2505ac02488",75265),
    (147,160):("697face6bce0cf299e80c0c28cae51bb49e13dbe435fb529fbf98cfb76fee991",79381),
    (147,320):("36e226baf02c67e6e9357c0c1d31c205b2f00ef89526512347487c27479d8936",158467),
    (147,640):("42f674e946c7aa816da52ef434f162caed917199ad752514ab771d3094860923",316639),
    (160,147):("6997f49435c08f031fd3df68405fbd4836f1918b3a6756aa6defb970241380cf",81921),
    (320,147):("4da7e0cbafc81837c159dab8139e605350f9c156d92b5f11bd02262b51872004",163841),
    (640,147):("5f5a3b1c9020f3404cb5b1f292396122c3d65368c61988809713ac94acfb08e1",327681),
}
class PrivateIntegrityError(ValueError):
    pass
def checked_files(source: Path) -> dict[str, bytes]:
    if not source.is_file() or source.is_symlink():
        raise PrivateIntegrityError("Private source ZIP not a regular file")
    if hashlib.sha256(source.read_bytes()).hexdigest()!=ORIGINAL_ZIP_SHA256:
        raise PrivateIntegrityError("Original private ZIP SHA-256 mismatch")
    files={f"RGSMSR1_L{L}_M{M}.bin":(sha,n) for (L,M),(sha,n) in EXPECTED.items()}
    try:
        with ZipFile(source) as z:
            info=z.infolist()
            names=[x.filename for x in info]
            if len(names)!=15 or len(set(names))!=15 or set(names)!=set(files)|{"CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json"}:
                raise PrivateIntegrityError("Original 14-file ZIP inventory mismatch")
            if sum(x.file_size for x in info)>12_000_000:
                raise PrivateIntegrityError("Private Q0 ZIP exceeds frozen size envelope")
            for item in info:
                if ("/" in item.filename or "\\" in item.filename
                    or item.is_dir()
                    or (item.create_system==3 and stat.S_IFMT(item.external_attr>>16) not in (0,stat.S_IFREG))):
                    raise PrivateIntegrityError("Unsafe ZIP member")
            manifest=json.loads(z.read("CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json"))
            if (manifest.get("expected_ratio_count")!=14 or
                manifest.get("observed_ratio_count")!=14 or
                manifest.get("product_admission_authorized") is not False or
                manifest.get("q1_q6_passed") is not False):
                raise PrivateIntegrityError("Private Q0 NONRELEASE manifest or inventory false")
            result={}
            for filename,(digest,n) in files.items():
                data=z.read(filename)
                if len(data)!=52+8*n or hashlib.sha256(data).hexdigest()!=digest:
                    raise PrivateIntegrityError("Original private FIR SHA/length mismatch")
                result[filename]=data
            return result
    except (BadZipFile,EOFError,KeyError,json.JSONDecodeError) as e:
        raise PrivateIntegrityError("Original private ZIP is incomplete or malformed") from e
def install(source:Path,target:Path)->Path:
    files=checked_files(source)
    if target.exists() or target.is_symlink() or not target.parent.is_dir() or target.parent.is_symlink():
        raise PrivateIntegrityError("Destination must be new, under an existing real directory")
    temporary=Path(tempfile.mkdtemp(prefix=".rgsml-original-q0-",dir=target.parent))
    try:
        for name,data in files.items():
            (temporary/name).write_bytes(data)
        for name,(sha,n) in ((f"RGSMSR1_L{L}_M{M}.bin",v) for (L,M),v in EXPECTED.items()):
            f=temporary/name
            if f.stat().st_size!=52+8*n or hashlib.sha256(f.read_bytes()).hexdigest()!=sha:
                raise PrivateIntegrityError("Private local staging write failed verification")
        if target.exists() or target.is_symlink():
            raise PrivateIntegrityError("Private destination occupied during installation")
        temporary.rename(target)  # atomic folder publish; never overwrite
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)
    return target
def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original-private-zip",required=True,type=Path)
    p.add_argument("--new-directory",required=True,type=Path)
    args=p.parse_args()
    dest=install(args.original_private_zip,args.new_directory)
    print(json.dumps({"status":"ORIGINAL_Q0_PRIVATE_FILES_VALIDATED_LOCALLY",
          "count":14,"directory":str(dest),
          "release_qualified":False,"physical_windows_fq_passed":False}))
if __name__=="__main__":
    main()
