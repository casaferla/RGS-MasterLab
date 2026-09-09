# Architecture bootstrap

RGS MasterLab uses a shared C++20 core with platform-specific application shells.
The initial dependency direction is deliberately one-way:

```text
rgsml_app ------> rgsml_ui ------> Qt Quick / QML
      |               \---------> Qt Quick Dialogs
      |-------> rgsml_platform ---> Qt Core (private implementation detail)
      |-------> rgsml_audio ------> rgsml_core (Qt-free)
rgsml_platform -------------------> rgsml_core (Qt-free)
```

`rgsml_core` and `rgsml_audio` remain independent of Qt and operating-system UI
APIs. `rgsml_platform` may use Qt Core privately but exposes only core resource
contracts. QML is limited to presentation and interaction; business logic
belongs in C++. DSP, project persistence, rendering, and Android packaging
remain outside this bootstrap.

Dependencies are resolved locally by CMake. Ordinary configure operations must
not download packages or embed machine-specific installation paths.

Compiler warnings are configured only through `rgsml_apply_project_warnings`
on first-party targets. Dependency targets never inherit warnings-as-errors.
The generated `ToolchainManifest.json` describes only stable, detected
toolchain values; it excludes paths, timestamps, source-control state, and host
identity. Windows staging uses Qt's generated QML deployment script and remains
inside the ignored build tree.

## Core foundation primitives

L0-M03 places error/result, UUID/typed-ID, checked integer, exact rational,
frame/time, and processing-state primitives in `rgsml::core`. Their public
headers are self-contained and their implementation uses only the C++20
standard library; Qt, Win32, I/O, persistence, and platform services remain
outside `rgsml_core`.

Valid value objects preserve their invariants at construction: rational
denominators are positive and reduced, sample rates are positive, frame counts
are non-negative, typed IDs are non-nil, and frame ranges are half-open with
`end >= begin`. Expected validation and arithmetic failures are represented by
`Result`/`Status` with stable categorical error tokens. Frame-to-time mapping is
exact rational arithmetic; time-to-frame rounding and FrameMap semantics are
intentionally deferred.

## Platform-neutral resource and playback ports

L0-M04 adds inward-facing resource and playback contracts to `rgsml::core`.
`ResourceReference` carries a provider-owned opaque locator, read/write
permissions, and optional non-authoritative display metadata. Logical identity
is the provider identifier plus locator; a byte-level identity mismatch alone
does not prove that two locators resolve to different physical documents.

`IResourceReader` and `IResourceWriter` are blocking, sequential-first byte
ports. Capabilities explicitly describe optional seek, known-size, resize, and
flush support and remain separate from access permissions. Short transfers are
valid, EOF is a successful zero-byte read, and close is idempotent. Flush and
close do not imply transactional commit or authoritative publication.

`IAudioPlaybackService` is a resource-based control-plane port whose snapshot
uses the existing exact frame types. It defines no decoder, PCM buffer, device,
thread, or rendering authority. Concrete platform adapters remain downstream
of core and are not introduced at Level 0.

The reusable CMake dependency rule audits transitive target edges. Dedicated
positive and negative fixtures, standalone public-header probes, and a
forbidden include/type scan keep `rgsml_core` independent of Qt, UI, DSP, and
platform implementations.

## Canonical PCM buffer and WAV decode

L1-M01 introduces the single static `rgsml_audio` production target. It owns
`AudioFormat`, source/output frame-domain timebases, the move-only canonical
`AudioBuffer` and its const/mutable non-owning views, plus `WavReader` and
read-only stream metadata. Its only production dependency is `rgsml_core`.

Canonical runtime PCM is planar IEEE binary64 with one mono-C plane or ordered
stereo L/R planes. Every non-empty plane is at least 64-byte aligned. Buffers
and subviews carry an absolute half-open frame range and an exact sample-rate
timebase; local frame zero is therefore never used as a substitute for source
origin. Runtime samples preserve signed zero and subnormals. The future
`RGSDAU1` checksum view, normalization, sample-rate conversion, and DSP remain
outside this milestone.

`WavReader` owns one read-only, seek-capable `IResourceReader`. Its bounded
structural scan accepts the frozen RIFF/RF64 little-endian PCM16/24/32 and IEEE
binary32/binary64 mono/stereo matrix, including valid extensible formats and
unknown padded chunks. Decode is absolute-frame random access and uses bounded
staging so short reads and failures cannot publish partial output. It rejects
ambiguous layouts, malformed/truncated containers, unsupported encodings, and
non-finite float samples without clipping, normalization, or implicit channel
conversion. Playback, writing, persistence, and DSP remain downstream work.

## Windows read-only Source boundary

L1-M02 introduces the single `rgsml_platform` production target and the
`rgsml.windows.local-file` provider. `WindowsResourceReader` implements the
frozen blocking `IResourceReader` contract for canonical absolute local-drive
UTF-8 locators. Its public header exposes no Qt or Win32 types; Qt Core and
filesystem details stay inside the private implementation. The adapter grants
read permission only, reports seek and known-size capabilities, treats EOF as
a successful zero-byte read, preserves position on rejected seeks, and makes
close idempotent.

`SourceResource` belongs to `rgsml_audio`. It probes a reader through the
existing `WavReader`, then retains only the immutable `ResourceReference` and
`WavStreamInfo`; it owns no open stream, decoded PCM, checksum, revision, or
project state. The application-layer `SourceSelectionViewModel` composes the
Windows adapter with this audio aggregate and publishes presentation-ready
metadata. Selection is transactional: cancel is a no-op, a failed candidate
leaves the accepted Source intact, and a later valid candidate replaces it.

The QML shell uses a single-file WAV `FileDialog` and renders empty, ready, and
error states plus the explicit read-only badge. It exposes no writer, playback,
waveform, transport, or analysis control.
