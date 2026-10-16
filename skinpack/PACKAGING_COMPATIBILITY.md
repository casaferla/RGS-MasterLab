# SkinPack R1 host archive compatibility — 2026-10-16

**Status:** EXPERIMENTAL / NOT NATIVE Qt QUALIFIED.

Found during pre-Qt audit: `third_party/CMakeLists.txt` explicitly sets
`MZ_ZLIB OFF`, `MZ_BZIP2 OFF`, and other compressor backends OFF when
compiling minizip-ng. Original Kintsugi R1/R2 packages use ZIP compression
method 8 (DEFLATE) for every member. The original structural preflight
accepted method 8 but the statically linked minizip-ng runtime would not
be expected to decompress it.

**Experimental R1 resolution:** Require ZIP method 0 (STORE) for every
entry; do not touch the production third_party dependency configuration.
Retain ordinary PNG/WebP compression INSIDE image files. This avoids a
new third-party library and does not change texture pixels or hash values
declared by `manifest.json`.

Rebuild fixtures:
`python tools/skinpack/repack_store.py <Kintsugi_R1.rgsskin> <Kintsugi_R1_STORE.rgsskin>`

Recheck with:
`python -m unittest discover -s skinpack/tests -v`
and `SKINPACK_REAL_FIXTURE=/path/to/Kintsugi_R1_STORE.rgsskin` on Linux.
The v0.1 Python validator is structural and accepts both methods; it does
not represent the native STORE-only host policy.

**Migration contract still preliminary:** DEFLATE variants are rejected
by the experimental loader until a separate, audited support decision;
the named R1 and R2 source archives are preserved, not silently replaced.
The pending full Qt build and GUI runtime remain mandatory. Test
`powershell -ExecutionPolicy Bypass -File tools/skinpack/qt_r1_probe.ps1`
in a Windows 10/11 x64 environment with the pinned Qt 6.8.3 kit.
No PR, merge, production CI/FQ or M16 write is authorized.
