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

## Windows WAV playback and playback-only rate adaptation

L1-M03 implements the frozen `IAudioPlaybackService` with one concrete
`WindowsAudioPlaybackService` in `rgsml_platform`. Its public header remains
standard/core-only. Qt Multimedia, `QAudioSink`, default-device discovery,
the bounded byte queue, device-format conversion, and lifecycle handling are
private implementation details. The authoritative path remains
`WindowsResourceReader -> WavReader -> AudioBuffer`; Qt does not decode WAV
data and the audio callback only drains the bounded queue.

`PlaybackSampleRateAdapter` belongs to `rgsml_audio` and is Qt-, Win32-, and
platform-free. Exact-rate playback bypasses it. The only non-identity policy is
the exact rational pair 44.1/48 kHz with the same channel layout, frozen
binary64 polyphase FIR assets, absolute frame mapping, even-reflect whole-track
boundaries, and bounded decode windows. It is ephemeral playback support, not
a module, render node, processing state, checksum input, cache, or persisted
artifact.

| Property | Playback SRC L1-M03 | Future Final Output SRC |
|---|---|---|
| Capability | `rgsml.audio.src.playback` | `rgsml.audio.src.final-output` |
| Placement | device audition path only | terminal MASTER/render path |
| Rate policy | identity plus `44100<->48000` | frozen future rate matrix |
| Quality profile | 120 dB design, beta `613263/50000`, T=192 | separate frozen 150 dB profile |
| Assets/manifest | playback-only, independently checksummed | separate and not materialized here |
| Qualification claim | L1-M03 functional listening only | no Q0-Q6/release claim from this work |

The production dependency direction is therefore
`rgsml_platform -> rgsml_audio -> rgsml_core`, with Qt Core and Qt Multimedia
private to the platform adapter. The reverse `audio -> platform` edge remains
forbidden.

## Source waveform overview

L1-M04 adds the Qt-free `rgsml::audio::WaveformSummary` as the only
authoritative waveform artifact. It is built in one forward pass from the
accepted `WindowsResourceReader -> WavReader -> AudioBuffer` path. The base
level contains at most 65,536 min/max buckets per channel, decode blocks contain
at most 4,096 frames, the complete pyramid contains at most 131,071 buckets per
channel, and presentation selects at most 4,096 ranges per channel. The
summary preserves decoded values, channel order, signed zero, and subnormals;
render-only clipping never changes its data.

The application owns the cancellable worker, current generation, stale-result
guard, and one-entry in-memory cache. The cache is ephemeral: it is neither a
project artifact nor a persisted analysis result. The UI receives shared
immutable summary ownership. C++ selects the bounded level and builds native
scene-graph geometry, while QML owns only layout, bindings, accessibility, and
retry intent. Bucket spans use shared physical-pixel boundaries so adjacent
ranges cover the viewport without rounding gaps. The playhead is read-only;
zoom, pan, seek-on-waveform, and region semantics remain outside L1-M04.

## Source waveform navigation and Audition Region

L1-M05 keeps `WaveformSummary` and its peak pyramid immutable while adding an
ephemeral C++ viewport expressed as an exact half-open Source-frame range.
Private UI math maps integer physical-pixel boundaries to Source frames with
checked, ties-to-even arithmetic. Zoom, pan, fit, hit testing, playhead,
region handles, and clipped bucket geometry share that mapping; each render
selects only the finest existing level whose visible window contains at most
4,096 peak ranges per channel. Resize and DPI changes do not alter the stored
Source-frame viewport, and navigation performs no decode, I/O, or summary
rebuild.

`AuditionRegionViewModel` is the single application-side region artifact. It
owns zero or one exact `core::FrameRange` over the current Source and exposes
formatted text plus high-level actions to QML. Region creation/editing and
exact decimal-time parsing remain in C++; QML performs no frame arithmetic.
The view-model delegates click seek and explicit loop transactions to the
existing `PlaybackTransportViewModel`, which in turn uses the frozen
`IAudioPlaybackService` port. Loop state is re-snapshotted from that service,
and failure preserves the previously committed region. The state is runtime
only: it is not project persistence, analysis/processing scope, DSP, or a
render contract.
