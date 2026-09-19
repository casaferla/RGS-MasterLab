# L1-M10 foundation qualification spike (no project semantics)

## Scope and baseline

- Original `main`/HEAD: `234c4548e648f517516a790ecb7a941f20e3ea5a`.
- Original tree: `182f2435d0a8cadca8fc895edfce83de56bb3319`.
- Branch: `task/L1-M10-project-persistence`; no commit or merge in this spike.
- No `ProjectSnapshot`, `ProjectRepository`, project format, Open/Save UI,
  or M11 work was implemented.

## Vendor provenance and build profile

- nlohmann/json 3.12.0, annotated tag `v3.12.0`, peeled commit
  `55f93686c01528224f448c19128836e7df245f72`, MIT. Vendored single
  header and license text under `third_party/nlohmann_json/`; one upstream
  first-line trailing space in the license was normalized.
- minizip-ng 4.2.2, tag/commit
  `7b2387161c542fa9f427352dcdef76097d0d692b`, zlib-style. Vendored
  essential upstream build/core/Windows/POSIX files and verbatim license under
  `third_party/minizip-ng/`.
- `third_party/CMakeLists.txt` uses an INTERFACE SYSTEM include for JSON and
  scopes `BUILD_SHARED_LIBS=OFF`, `SKIP_INSTALL_ALL=ON`, and every MZ option
  specified in the packet to the vendor directory. `MINIZIP::minizip` is the
  upstream alias. Configure reports compression/fetch/encryption/compat/test
  features disabled. No configure/build fetch occurred.
- Generated Debug `rgsml_m10_store_tests.vcxproj` links `rgsml_core.lib`,
  `minizip-ng.lib`, `Qt6Testd.lib`, `Qt6Cored.lib`, `mpr.lib`, `userenv.lib`,
  and normal Windows SDK libraries. Release links their Release counterparts.
  No zlib or other compression codec library is linked. The upstream C source
  emits one non-fatal MSVC C4267 size-conversion warning; no RGSML warning
  policy was weakened.

## SHA and JSON proofs

- Generic SHA-256 implementation and `Sha256Digest`, `Sha256`, and both
  `sha256_hex` overloads are owned by `rgsml_core`.
  `CanonicalDecodedAudioHasher` and the RGSDAU1 format remain in Windows
  source and consume `core::Sha256`. Private Windows aliases preserve existing
  export/manual-test caller spelling without duplicate implementation.
- Focused SHA: empty, `abc`, NIST multi-block vector, irregular incremental
  chunks equal one-shot, lowercase 64-character hex: PASS Debug/Release.
- Existing decoded-audio checksum, export checksum and transactional export
  tests: PASS Debug/Release. No accepted hash or signed-zero semantic changed.
- Private JSON probe: RGSML-owned 1 MiB/100,000-event/64-container-depth
  bounded SAX first pass rejects duplicate object keys, non-finite float,
  overflowing integer-to-float coercion, syntax errors and malformed UTF-8.
  A strict DOM parse is called only after SAX success. No public JSON API or
  project schema is introduced.
- JSON cases PASS Debug/Release: valid UTF-8 JSON, duplicate keys (root and
  nested), comments/trailing comma/trailing garbage/malformed UTF-8 rejected,
  depth 64 accepted and 65 rejected, signed 64-bit min/max and unsigned max,
  integer overflow rejected, integer versus floating types kept distinct,
  difficult finite binary64 bit-exact dump/parse, NaN/Inf rejected before
  serialization, compact UTF-8 without BOM/newline, lexical key order and
  preserved array order.

## STORE/ZIP64 stream proof

`test_store_stream.cpp` provides in-memory `IResourceReader` and
`IResourceWriter` implementations and a private `mz_stream_vtbl` bridge.
It never passes an OS path to minizip. Low-level `mz_zip_*` creates and reads
two STORE entries, including a UTF-8 entry name. Payloads round-trip exactly.
`MZ_ZIP64_AUTO` produces the normal small archive without a ZIP64 extra;
`MZ_ZIP64_FORCE` creates a ZIP64 extra on a small entry, which the same
custom stream reads successfully. Both cases PASS Debug/Release.

## Security observability (future validator feasibility only)

This is a feasibility classification, **not** an implemented package
validator. A future bounded raw inspection may compare local, central and
EOCD fixed records/fields without becoming a broad ZIP parser.

| Frozen rejection property | Classification | Evidence/field |
| --- | --- | --- |
| Non-STORE compression | DIRECTLY_OBSERVABLE | `mz_zip_file.compression_method` |
| Encrypted/AES entry | DIRECTLY_OBSERVABLE | `flag & MZ_ZIP_FLAG_ENCRYPTED`, `aes_version`, AES extra |
| Directory entry | DIRECTLY_OBSERVABLE | `mz_zip_entry_is_dir` |
| Symlink entry | DIRECTLY_OBSERVABLE | `mz_zip_entry_is_symlink`, `external_fa` |
| Entry path/name | DIRECTLY_OBSERVABLE | `filename`, `filename_size`; future RGSML path policy |
| ZIP64 state | REQUIRES_RAW_CONTAINER_VALIDATION | ZIP64 local/central extra and EOCD markers; reader's `zip64` policy field alone is not authoritative |
| Extra-field size | DIRECTLY_OBSERVABLE | `extrafield_size`; raw bounds can additionally cross-check malformed records |
| Compressed/uncompressed size | DIRECTLY_OBSERVABLE | `compressed_size`, `uncompressed_size`; raw cross-check for mismatched records |
| Disk number / multi-disk indicators | REQUIRES_RAW_CONTAINER_VALIDATION | Entry `disk_number` and central disk getter exist; EOCD disk/count fields require bounded raw cross-check |
| Zipped central directory | DIRECTLY_OBSERVABLE | `mz_zip_reader_get_zip_cd` exposes this state; raw CD extra signature can cross-check |

Every listed rule can be enforced by the available metadata or bounded raw
fixed-field validation. No broad parser or security policy implementation was
added by this spike.

## Qualification and Android

- Fresh normalized-child-environment VS2022 x64 configure: PASS with MSVC
  19.44.35228.0, CMake 3.30.5 and Qt 6.8.3. The first discarded build-cache
  attempt inherited duplicate host `Path`/`PATH`; only the process/child
  environment map was normalized. No machine/global setting was changed.
- Clean-build-directory full Debug build: PASS; complete CTest **68/68**,
  including **27/27** dependency/public-header contracts and deploy smoke.
- Same clean build directory full Release build: PASS; complete CTest **68/68**,
  including **27/27** contracts and deploy smoke.
- No Android NDK/SDK environment variables, standard local SDK/NDK directories,
  or Qt Android arm64 kit were present. Result:
  `ANDROID_TOOLCHAIN_INPUT_MISSING`. This is non-blocking for Windows M10 and
  remains mandatory before first Android `rgsml_project` materialization.
