# CODEX TASK 012 / L1-M09 — Implementation report

## Outcome

`CODEX TASK 012 — READY FOR REVIEW`

The Task 012 packet is materialized without scope expansion. The audio layer
now provides deterministic bounded IEEE-F64 RIFF/RF64 writing over the frozen
core byte port. The Windows platform layer provides exclusive create-new file
I/O and a fail-closed transaction that validates, hashes, atomically publishes,
and reads back one new Render Preview WAV without replacing Source or an
existing destination. All required automated gates and the Manual Functional
Gate passed.

## Git and authority evidence

- Baseline and parent: `ae821ba9265f98cae9a6b0a08eafbde0c8e9a975`
- Baseline tree: `dd75f64446692a2042b08be67911fe630ab6a1fe`
- Task branch: `task/L1-M09-wav-writer-safe-export`
- Delivery subject: `CODEX TASK 012 / L1-M09`
- Packet SHA-256: `7D88BBD6B32F939CF7F865AD491D893EF6FB81E22BDC5B9589C12B4233E82B35`
- Historical untracked file SHA-256: `3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF`
- `main` remained at the authoritative baseline throughout implementation.

`PREEXISTING_WORKTREE_CHANGES` contains only
`RGS_MasterLab_CODEX_TASK_001-2.md`; it remained untracked, unstaged,
byte-identical, and untouched. Every other changed path is a
`TASK_012_CHANGE`.

## Changed paths

Production:

- `audio/CMakeLists.txt`
- `audio/include/rgsml/audio/wav_writer.hpp`
- `audio/src/wav_writer.cpp`
- `platform/CMakeLists.txt`
- `platform/windows/include/rgsml/platform/windows/windows_resource_writer.hpp`
- `platform/windows/include/rgsml/platform/windows/transactional_audio_exporter.hpp`
- `platform/windows/src/windows_resource_writer.cpp`
- `platform/windows/src/transactional_audio_exporter.cpp`
- `platform/windows/src/internal/sha256.hpp`
- `platform/windows/src/internal/sha256.cpp`
- `platform/windows/src/internal/transactional_audio_exporter_test_seam.hpp`

Tests:

- `tests/CMakeLists.txt`
- `tests/unit/audio/test_wav_writer.cpp`
- `tests/unit/platform/test_export_checksums.cpp`
- `tests/integration/platform/test_transactional_audio_exporter.cpp`
- `tests/manual/export/export_functional_gate.cpp`
- `tests/cmake/dependency_rules_fixture/CMakeLists.txt`
- `tests/cmake/dependency_rules_fixture/audio_allowed_consumer.cpp`
- `tests/cmake/dependency_rules_fixture/platform_allowed_consumer.cpp`

Documentation/evidence:

- `docs/architecture/README.md`
- `docs/implementation/task-log.md`
- `docs/implementation/telemetry/L1-M09.json`
- `docs/implementation/reports/CODEX_TASK_012_L1-M09.md`

No other path is changed.

## Exact public API mapping

`rgsml::audio::WavWriteSpec` stores the exact `AudioFormat`, declared
`FrameCount`, and `WavSampleFormat`. `rgsml::audio::WavWriter` exposes:

```cpp
static core::Result<std::unique_ptr<WavWriter>> open(
    std::unique_ptr<core::IResourceWriter> destination,
    WavWriteSpec spec);
const WavWriteSpec& spec() const noexcept;
WavContainerKind container_kind() const noexcept;
std::uint64_t expected_file_size_bytes() const noexcept;
core::FrameCount frames_written() const noexcept;
core::Status write_frames(AudioBufferView source);
core::Status finalize();
core::Status close();
```

`rgsml::platform::windows::WindowsResourceWriter` implements the accepted
`core::IResourceWriter` lifecycle and adds only `make_write_reference(...)` and
`open_create_new(...)`. It exposes the unchanged writer-port methods
`reference`, `capabilities`, `position_bytes`, `write`, `seek_bytes`,
`resize_bytes`, `flush`, and `close`.

`TransactionalAudioExportRequest::create(...)` owns Source/destination
references and borrows a read-only `AudioBufferView`; accessors expose those
values, `WavSampleFormat`, and the sole `FAIL_IF_EXISTS` collision policy.
`TransactionalAudioExporter::export_new_file(const
TransactionalAudioExportRequest&)` returns an owned
`TransactionalAudioExportResult` with exact destination, `WavContainerKind`,
`WavSampleFormat`, `AudioFormat`, `FrameCount`, file size, encoded SHA-256,
decoded-audio SHA-256, and `VERIFIED` status.

The frozen Task 011 `RenderRequest`, `RenderResult`, and `render_preview` APIs,
the frozen core writer port, and all Task 007–011 production surfaces are
unchanged.

## WAV layout and boundary evidence

Only canonical binary64 `IEEE_F64` is accepted. Mono `C` and stereo `L/R`
Source planes are emitted frame-major as little-endian 64-bit payloads. The
writer preserves all finite sample bit patterns, including signed zero and
subnormals, rejects NaN/infinity before publishing the affected frame block,
and uses bounded 4,096-frame interleave storage.

RIFF is exactly `RIFF/WAVE + fmt(18) + fact(4) + data`, with format tag 3,
64 bits/sample, no metadata chunks, and 58 bytes total overhead. RF64 is
exactly `RF64/WAVE + ds64(28) + fmt(18) + fact(4) + data`, with required
32-bit sentinels, exact 64-bit sizes/sample count, zero ds64 table entries, no
metadata chunks, and 94 bytes total overhead.

Virtual-boundary tests prove deterministic selection without allocating the
large payload:

| Layout | Last RIFF frame count | First RF64 frame count |
|---|---:|---:|
| Mono C | 536,870,905 | 536,870,906 |
| Stereo L/R | 268,435,452 | 268,435,453 |

Exact RIFF/RF64 chunk IDs, chunk sizes, padding, sample-rate/byte-rate/block
alignment, `fact` sample length, `ds64` fields, declared sizes, final file
size, and absence of extra chunks are checked byte-for-byte. Partition
invariance, one-byte short writes, zero-progress failure, exact frame
accounting, capability checks, overflow paths, and close/finalize lifecycle are
covered.

## Checksums and provenance

The platform-private SHA-256 implementation has no public API and no external
dependency. Known-answer tests cover empty input, `abc`, and the standard long
vector. Encoded SHA-256 covers the exact committed candidate bytes.

The decoded checksum is domain-separated and deterministic: magic
`RGSDAU1` plus a literal NUL; little-endian schema/sample-domain identifiers;
sample rate; mono/stereo layout and canonical channel identifiers; exact frame
and payload-byte counts; then frame-major little-endian binary64 samples.
Signed zeros canonicalize to positive zero for this decoded hash only;
subnormals are preserved and non-finite samples are rejected. Independent
Python `hashlib` known answers are:

- mono: `8fe558df3b0761b8f549b63c7659a2d25db155f3de5c2d75c2f1ae41213722ee`
- stereo: `6f9fa864eb5a59aded978b0394d0e5d9cc0ff018533f88b6e19036d57e836ef3`
- subnormal: `149851f5ce3acd3d0e9332fa0bad0bb25a8d71f4f05982e88dbe991563705173`

## Transaction and failure semantics

The state sequence is deterministic:

`PREFLIGHT -> CREATE_UNIQUE_SIBLING -> WRITE -> CLOSE -> HASH_ENCODED ->`
`DECODE_CANONICALLY -> BIT_COMPARE -> HASH_DECODED -> COMMIT_NEW_FILE ->`
`READBACK_HASH -> VERIFIED`.

Source and destination must be canonical Windows local-file references.
Case-insensitive path equality and Windows file identity reject Source itself,
aliases, and hardlinks. Any existing destination is rejected; the writer and
commit both use no-replace semantics. Publication occurs only after the
candidate is closed, structurally decoded through canonical `WavReader`,
matched to requested format/frame count/container, bit-compared to the payload,
and validated by encoded and decoded hashes. Destination bytes are hashed
again after the no-replace rename.

Failure injection covers candidate creation, writer I/O, validation, encoded
checksum, decoded checksum, commit/rename, destination readback, and cleanup.
Every injected pre-commit failure leaves no destination and no owned candidate;
the existing destination case remains byte-identical. The cleanup-failure seam
reports failure rather than claiming success. Relevant mappings use existing
`ErrorCode` values: `InvalidArgument`, `UnsupportedAudioEncoding`,
`UnsupportedAudioLayout`, `UnsupportedOperation`, `OutOfRange`,
`IntegerOverflow`, `InvalidAudioSample`, `InvalidState`, `ResourceNotFound`,
`AccessDenied`, `IoFailure`, `MalformedAudioContainer`, and
`TruncatedAudioData`. Stable message identifiers include
`wav_writer_*`, `destination_exists`, `source_collision`,
`candidate_create_failed`, `writer_io_failed`, `candidate_validation_failed`,
`encoded_checksum_mismatch`, `decoded_checksum_mismatch`, `commit_failed`,
`destination_readback_mismatch`, and `candidate_cleanup_failed`.

## Render round-trip and Source immutability

Integration tests export owned Task 011 `RenderResult` audio for identity and
negative Gain. The result is reopened by `WindowsResourceReader` and decoded by
canonical `WavReader`; format, frame count, channel order, sample bits, and
both checksums match. Source file SHA-256/size and Source `AudioBuffer` sample
bits are captured before export and are identical afterward on success,
collision rejection, and injected failure paths.

## Automated acceptance

| Gate | Result |
|---|---|
| Fresh configure / inventory | PASS / 62 tests |
| Clean Debug build / complete CTest | PASS / 62 of 62 |
| Debug tests invalidated by Release-only test correction | PASS / 2 of 2 |
| Clean Release build / complete CTest | PASS / 62 of 62 |
| Focused Task 012 Release tests | PASS / 3 of 3 |
| Writer RIFF/RF64/golden/boundary matrix | PASS |
| Checksum/readback/transaction/collision/failure matrix | PASS |
| Dependency/public-header contracts, Debug | PASS / 27 of 27 |
| Dependency/public-header contracts, Release | PASS / 27 of 27 |
| Tests-off configure and clean Release build | PASS; 0 tests, no test tree or harness |
| Staged deploy smoke | PASS / exit 0 |
| ToolchainManifest build/install byte comparison | PASS |
| `git diff --check` | PASS |

The ToolchainManifest SHA-256 is unchanged:
`49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11`.
Build and staged-install copies are byte-identical. No dependency was
downloaded or installed.

## Manual Functional Gate

The Release harness was invoked with the process-local Qt runtime and the
tracked fixture:

```powershell
$env:Path = 'C:\Qt\6.8.3\msvc2022_64\bin;' + $env:Path
.\build\windows-msvc\tests\Release\rgsml_export_functional_gate.exe `
  .\tests\audio_golden\playback_src\listening_stereo_48000_pcm16.wav `
  "$env:TEMP\rgsml-task012-manual-b2f7a704bdd24b12a98d293f3dfb33d8.wav"
```

Result: `MANUAL_FUNCTIONAL_GATE=PASS`, exit 0. Evidence:

- Source SHA-256: `a4ba63513146ae032c29b3e5ca065a908e9d5c96f4ef45b53f21dc33725f480a`
- Source size: 1,344,044 bytes
- output: IEEE_F64 / RIFF / 48,000 Hz / stereo L/R / 336,000 frames
- exact file size: 5,376,058 bytes
- encoded SHA-256: `c573104a317272f0f82bd15880fe001430951f94a190815981e5be7a7729dee2`
- decoded RGSDAU1 SHA-256: `e9a7fd8aae9cae5d8041b240c2a4fe4f2c7861e098db54e1addae91eec6ad086`
- Source collision: rejected
- existing destination: rejected and unchanged
- Source after gate: unchanged
- successful candidate leak: none

The temporary successful destination was removed after evidence capture.

## Scope, protected surfaces, and findings

The final diff is confined to the packet allow-list. Core public contracts,
Task 001–011 evidence, Task 011 Gain/Render APIs and production behavior, DSP,
playback, app, UI, project, analysis, Reference Match, DNA, third-party,
ToolchainManifest inputs, phase/delta source documents, and Task 013+ artifacts
are unchanged. Input remains `WAV_ONLY`; output is only IEEE_F64 RIFF/RF64.

No SRC, PCM24, Float32 output, dither, quantizer, normalization, clipping,
metadata/provenance chunk, UI, persistence, Gold, Reference Match, recovery
journal, progress/cancellation scheduler, new input format, third-party
dependency, or new public architecture was introduced.

Two qualification-test defects were corrected without changing product
semantics: the independent checksum oracle now encodes the required literal
NUL domain separator, and Release test helpers no longer place required side
effects inside disabled `Q_ASSERT` expressions. Product defects found: none.

The first Manual Functional Gate launch inherited no Qt runtime in its isolated
process `PATH` and exited before test execution. A bounded retry normalized
only that child-process environment and passed. No repository, machine, Qt, or
antivirus setting was modified. No Avast detection or quarantine occurred.
There are no deviations from the packet and no residual blocker.

## Final status

After the delivery commit, `git status --short` is expected to contain only:

```text
?? RGS_MasterLab_CODEX_TASK_001-2.md
```

The historical file remains untracked, unstaged, untouched, and byte-identical
at SHA-256
`3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF`.

- R12-01: `INHERITED_UNCHANGED`
- R12-02: `MITIGATED_FOR_WINDOWS_L1-M05 / RESIDUAL_MONITORING_OPEN`
- RF-1: `MATERIALIZED; FROZEN ESTIMATES UNCHANGED`
- RF-2: `MATERIALIZED_AT_L1-M08; FROZEN PHASE-12 ESTIMATES UNCHANGED`
- `MAIN — UNCHANGED / NOT MERGED`
- `INPUT FORMAT SCOPE — WAV_ONLY`
- `CODEX TASK 013 — NOT PREPARED / NOT STARTED`
