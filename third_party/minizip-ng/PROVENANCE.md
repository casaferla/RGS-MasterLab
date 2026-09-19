# minizip-ng provenance

- Version/tag: `4.2.2` / `4.2.2`.
- Resolved commit: `7b2387161c542fa9f427352dcdef76097d0d692b`.
- Origin: `https://github.com/zlib-ng/minizip-ng`.
- Vendored upstream files: root CMake/config templates, license, core
  `mz_*` stream/ZIP sources and headers, Windows/POSIX OS adapters, and the two
  CMake helpers referenced by upstream. The WZAES declaration header is
  retained because `mz_zip_rw.c` includes it unconditionally; its source is
  not built. Optional codec/encryption sources and upstream tests are omitted
  because all such build options are disabled.
- License: minizip-ng zlib-style; see the verbatim vendored `LICENSE`.
- Acquired from the exact resolved tag commit. No configure/build fetch.
