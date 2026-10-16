"""Native C++20 ZIP preflight tests; deliberately independent of Qt and GitHub Actions.

Run from checkout root: python -m unittest discover -s skinpack/tests -v
Optionally: SKINPACK_REAL_FIXTURE=/path/to/Kintsugi_Prestige_Prototype_v0.1.rgsskin
"""
import io
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def fixture(entries=None, compression=zipfile.ZIP_STORED):
    if entries is None:
        entries = [("manifest.json", b'{}'), ("skin.json", b'{}')]
    sink = io.BytesIO()
    with zipfile.ZipFile(sink, 'w', compression=compression) as z:
        for name, data in entries:
            if isinstance(name, zipfile.ZipInfo):
                z.writestr(name, data)
            else:
                z.writestr(name, data)
    return sink.getvalue()


def patch_one_name(data, old, new):
    assert len(old) == len(new)
    assert data.count(old) == 2
    return data.replace(old, new)


def patch_central(data, offset, fmt, value):
    b = bytearray(data)
    eocd = len(b)-22
    central = struct.unpack_from('<I', b, eocd+16)[0]
    assert b[central:central+4] == b'PK\1\2'
    struct.pack_into(fmt,b,central+offset,value)
    return bytes(b)


class ZipPreflightTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix='rgs_skinpack_zip_')
        cls.cli = Path(cls.tmp.name) / 'skinpack_preflight'
        subprocess.run([
            'g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2',
            '-I', str(ROOT/'app/src'),
            str(ROOT/'app/src/skinpack_zip_preflight.cpp'),
            str(ROOT/'skinpack/tests/native_zip_preflight_cli.cpp'),
            '-o', str(cls.cli),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def check(self, raw, allowed=False):
        p = Path(self.tmp.name) / 'case.rgsskin'
        p.write_bytes(raw)
        run = subprocess.run([str(self.cli), str(p)], capture_output=True, text=True)
        self.assertEqual(run.returncode == 0, allowed, run.stdout + run.stderr)

    def test_stored_ok(self):
        self.check(fixture(compression=zipfile.ZIP_STORED), True)

    def test_deflated_rejected_without_zlib(self):
        self.check(fixture(compression=zipfile.ZIP_DEFLATED))

    def test_real_kintsugi_package(self):
        p = os.getenv('SKINPACK_REAL_FIXTURE')
        if not p:
            self.skipTest('Set SKINPACK_REAL_FIXTURE to test actual Kintsugi file')
        self.check(Path(p).read_bytes(), True)

    def test_zip_slip(self):
        self.check(fixture([('manifest.json',b'{}'), ('skin.json',b'{}'), ('../evil.png',b'x')]))

    def test_filename_alias(self):
        self.check(fixture([('manifest.json',b'{}'), ('skin.json',b'{}'), ('MANIFEST.JSON',b'{}')]))

    def test_absolute_path(self):
        self.check(fixture([('manifest.json',b'{}'), ('skin.json',b'{}'), ('/hidden.png',b'x')]))

    def test_unknown_executable_path(self):
        self.check(fixture([('manifest.json',b'{}'), ('skin.json',b'{}'), ('evil.qml',b'Item {}')]))

    def test_symlink_entry(self):
        link = zipfile.ZipInfo('assets/textures/linked.png')
        link.create_system = 3
        link.external_attr = (0o120777 << 16)
        self.check(fixture([('manifest.json',b'{}'), ('skin.json',b'{}'), (link,b'/tmp/x')]))

    def test_archive_comment(self):
        self.check(fixture()+b'noise')

    def test_wrong_local_crc(self):
        b=bytearray(fixture())
        struct.pack_into('<I',b,14,0x12345678)
        self.check(bytes(b))

    def test_wrong_local_filename(self):
        b=bytearray(fixture())
        b[30:43]=b'manifest2json'
        self.check(bytes(b))

    def test_central_offset_inside_local_payload(self):
        self.check(patch_central(fixture(),42,'<I',5))

    def test_central_fake_uncompressed_length(self):
        self.check(patch_central(fixture(),24,'<I',17*1024*1024))

    def test_expanded_entry_limit(self):
        self.check(fixture([('manifest.json',b'{}'),('skin.json',b'{}'),('assets/textures/big.png',b'X' * (17*1024*1024))]))

    def test_129_entries(self):
        files = [('manifest.json',b'{}'),('skin.json',b'{}')]
        files += [(f'assets/textures/{i:03}.png',b'x') for i in range(127)]
        self.check(fixture(files))

    def test_missing_skin(self):
        self.check(fixture([('manifest.json',b'{}')]))

    def test_prepended_executable(self):
        self.check(b'MZ-unsafe!' + fixture())

    def test_unsupported_extra_field(self):
        extra=zipfile.ZipInfo('assets/textures/a.png')
        extra.extra=b'\x01\x00\x04\x00ABCD'
        self.check(fixture([('manifest.json',b'{}'),('skin.json',b'{}'),(extra,b'x')]))

if __name__ == '__main__':
    unittest.main()
