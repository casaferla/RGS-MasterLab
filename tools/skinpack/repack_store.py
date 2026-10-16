#!/usr/bin/env python3
"""RGS SkinPack R1: convert previously validated .rgsskin ZIP to STORE.
This is NOT a security validator. Validate the input with the SkinPack validator.
Raw contents are preserved byte-for-byte; no QML, assets or hashes are modified.
Only use with the 0.1 candidate host built with MZ_ZLIB=OFF.
"""
from pathlib import Path
import argparse
import hashlib
import zipfile

ROOTS = {"manifest.json", "skin.json", "README.md", "preview.png"}
LIMIT = 20 * 1024 * 1024

def repack(source: Path, output: Path):
    if source.resolve() == output.resolve():
        raise ValueError("Input and output must differ")
    if source.stat().st_size > LIMIT:
        raise ValueError("Source archive is oversized")
    extracted = []
    with zipfile.ZipFile(source) as z:
        infos = z.infolist()
        if not 2 <= len(infos) <= 128:
            raise ValueError("Invalid entry count")
        normalized_names = set()
        total = 0
        for info in infos:
            name = info.filename
            parts = name.split("/")
            if (name not in ROOTS and not name.startswith("assets/textures/")) or any(
                not part or part in (".", "..") or not all(
                    c.isascii() and (c.isalnum() or c in "_.-") for c in part
                ) for part in parts
            ):
                raise ValueError("Unsafe path: " + name)
            if name.casefold() in normalized_names:
                raise ValueError("Duplicate path: " + name)
            normalized_names.add(name.casefold())
            if info.is_dir() or (info.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError("Directory or symbolic link")
            if info.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
                raise ValueError("Unsupported compression")
            if info.flag_bits & 1 or info.file_size > 16 * 1024 * 1024:
                raise ValueError("Encrypted or oversized member")
            total += info.file_size
            if total > LIMIT:
                raise ValueError("Expanded archive too large")
            payload = z.read(info)
            extracted.append((name, payload))
        if not {"manifest.json", "skin.json"}.issubset(normalized_names):
            raise ValueError("Missing required roots")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED, allowZip64=False) as z:
        for name, payload in extracted:
            member = zipfile.ZipInfo(name, date_time=(2026, 10, 16, 0, 0, 0))
            member.compress_type = zipfile.ZIP_STORED
            member.create_system = 0
            member.external_attr = 0
            z.writestr(member, payload)
    if output.stat().st_size > LIMIT:
        output.unlink()
        raise ValueError("STORE archive exceeds limit")
    with zipfile.ZipFile(output) as check:
        assert check.testzip() is None
        assert len(check.infolist()) == len(extracted)
        for name, original in extracted:
            assert check.getinfo(name).compress_type == zipfile.ZIP_STORED
            assert check.read(name) == original
    return hashlib.sha256(output.read_bytes()).hexdigest()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(repack(args.source, args.output))
