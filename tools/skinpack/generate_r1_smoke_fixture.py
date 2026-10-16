#!/usr/bin/env python3
"""Deterministic, stdlib-only STORE SkinPack fixtures for a native Qt R1 smoke.

Synthetic host-contract probes, NOT Kintsugi artwork or qualification assets.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile
import zlib


def png_rgba_4x4():
    def chunk(tag, body):
        return (struct.pack('>I', len(body)) + tag + body
                + struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff))
    raw = b''.join(b'\0' + bytes((245, 241, 235, 255)) * 4 for _ in range(4))
    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 4, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, level=9))
            + chunk(b'IEND', b''))


def generate(folder: Path):
    folder.mkdir(parents=True, exist_ok=True)
    image = png_rgba_4x4()
    image_path = 'assets/textures/transport-marble.png'
    manifest = {
        'format': 'rgsml.skinpack', 'formatVersion': '0.1',
        'hostContract': 'rgsml.skin.host/0.1', 'entry': 'skin.json',
        'name': 'SkinPack R1 Native Smoke', 'geometry': 'inherit-host',
        'trust': {'executableCode': False, 'networkAccess': False},
        'assetIndex': {'marbleTile': {'path': image_path,
            'sha256': hashlib.sha256(image).hexdigest(), 'bytes': len(image)}}
    }
    skin = {'$schema': 'rgsml.skin.definition/0.1',
            'geometry': 'inherit-host', 'semanticColors': 'inherit-host',
            'materials': {'marbleTransport': {
                'kind': 'texture', 'fit': 'tile-x', 'asset': 'marbleTile',
                'baseColor': '#F5F1EB', 'borderColor': '#B58A45'}},
            'profiles': {'surface': {'slots': {
                'transport.background': {'material': 'marbleTransport'}}}}}
    files = {
        'manifest.json': json.dumps(manifest, separators=(',', ':'), sort_keys=True).encode(),
        'skin.json': json.dumps(skin, separators=(',', ':'), sort_keys=True).encode(),
        image_path: image,
    }
    valid = folder / 'r1-valid.rgsskin'
    with zipfile.ZipFile(valid, 'w', compression=zipfile.ZIP_STORED,
                         allowZip64=False) as z:
        for name, data in files.items():
            info = zipfile.ZipInfo(name, date_time=(2026, 10, 16, 0, 0, 0))
            info.compress_type = zipfile.ZIP_STORED
            info.create_system = 0
            z.writestr(info, data)
    with zipfile.ZipFile(valid) as z:
        assert z.testzip() is None
        assert set(z.namelist()) == set(files)
        assert all(z.getinfo(name).compress_type == zipfile.ZIP_STORED for name in files)
    corrupted = bytearray(valid.read_bytes())
    corrupted[-22] ^= 0x01  # damage EOCD signature; fail closed in preflight
    invalid = folder / 'r1-invalid.rgsskin'
    invalid.write_bytes(corrupted)

    def negative_archive(filename, *, change=None, compression=zipfile.ZIP_STORED,
                         malicious_path=False):
        """Keep the ZIP container valid while altering one hostile contract field."""
        m = json.loads(json.dumps(manifest))
        sk = json.loads(json.dumps(skin))
        payload = image
        if change == 'hash':
            m['assetIndex']['marbleTile']['sha256'] = '0' * 64
        elif change == 'schema':
            sk['semanticColors'] = 'override'
        elif change == 'host':
            m['hostContract'] = 'rgsml.skin.host/unsupported'
        elif change == 'trust':
            m['trust']['executableCode'] = True
        elif change == 'png':
            payload = b'not-a-valid-png'
            m['assetIndex']['marbleTile']['sha256'] = hashlib.sha256(payload).hexdigest()
            m['assetIndex']['marbleTile']['bytes'] = len(payload)
        entries = {
            'manifest.json': json.dumps(m, separators=(',', ':'), sort_keys=True).encode(),
            'skin.json': json.dumps(sk, separators=(',', ':'), sort_keys=True).encode(),
            image_path: payload,
        }
        if malicious_path:
            entries['assets/textures/../unsafe.png'] = b'unsafe'
        target = folder / filename
        with zipfile.ZipFile(target, 'w', compression=compression,
                             allowZip64=False) as z:
            for name, data in entries.items():
                info = zipfile.ZipInfo(name, date_time=(2026, 10, 16, 0, 0, 0))
                info.compress_type = compression
                info.create_system = 0
                z.writestr(info, data)
        with zipfile.ZipFile(target) as z:
            assert z.testzip() is None
        return target

    negative_archive('r1-bad-hash.rgsskin', change='hash')
    negative_archive('r1-bad-schema.rgsskin', change='schema')
    negative_archive('r1-bad-host.rgsskin', change='host')
    negative_archive('r1-bad-trust.rgsskin', change='trust')
    negative_archive('r1-bad-png.rgsskin', change='png')
    negative_archive('r1-deflate.rgsskin', compression=zipfile.ZIP_DEFLATED)
    negative_archive('r1-unsafe-path.rgsskin', malicious_path=True)
    print(f'SKINPACK_SMOKE_FIXTURES_CREATED valid={valid} invalid={invalid}'
          ' additional_rejected=7')


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('folder', type=Path)
    generate(ap.parse_args().folder)
