# Implementation task log

## L0-M02 — Build policy and deterministic toolchain

- Status: READY_FOR_REVIEW
- Baseline: `e032441a9df2e70a94b83c2a20d121f89baf5f97`
- Branch: `task/L0-M02-build-policy-toolchain`
- Delivery subject: `CODEX TASK 002 / L0-M02`
- Scope: clean Debug/Release presets, target-scoped warnings-as-errors,
  deterministic toolchain manifest, staged Qt/QML deployment, and automated
  deployment smoke.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_003_NOT_STARTED

## L0-M03 — Core result, ID, and exact frame/time primitives

- Status: READY_FOR_REVIEW
- Baseline: `f44d15bb5a187e7119151e940f0114dd1b5c5770`
- Branch: `task/L0-M03-core-primitives`
- Delivery subject: `CODEX TASK 003 / L0-M03`
- Scope: categorical errors and result/status, canonical UUID and typed IDs,
  checked signed arithmetic, exact rational values, strong frame/time values,
  and canonical processing state.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_004_NOT_PREPARED_OR_STARTED

## L0-M04 — Platform/resource/playback ports and dependency rules

- Status: READY_FOR_REVIEW
- Baseline: `84d97d5e80295893f20f39e81d733693b07071d4`
- Branch: `task/L0-M04-platform-contracts`
- Delivery subject: `CODEX TASK 004 / L0-M04`
- Scope: platform-neutral resource identity and permissions, capability-aware
  byte reader/writer ports, resource-based playback control, additive error
  categories, and positive/negative dependency-rule contracts.
- Level 0: implementation complete; closure remains subject to review and
  explicit acceptance.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_005_NOT_PREPARED_OR_STARTED

## L1-M01 — Canonical PCM buffer and WAV PCM/float decode

- Status: READY_FOR_REVIEW
- Baseline: `04306e664b9c1add58645cbbc681e725bddc485f`
- Branch: `task/L1-M01-wav-decode-audio-buffer`
- Delivery subject: `CODEX TASK 005 / L1-M01`
- Scope: Qt-free canonical binary64 planar audio storage and views, exact
  source timebase/ranges, bounded seekable RIFF/RF64 parsing, bit-exact
  PCM16/24/32 and IEEE binary32/binary64 decode, and audio dependency gates.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_006_NOT_PREPARED_OR_STARTED

## L1-M02 — Windows Source picker, read-only resource, and metadata panel

- Status: READY_FOR_REVIEW
- Baseline: `8a366ccd1f2c29fd07e48dae6eafd8a4da7cc621`
- Branch: `task/L1-M02-windows-source-resource`
- Delivery subject: `CODEX TASK 006 / L1-M02`
- Scope: canonical read-only Windows local-file provider, `SourceResource` WAV
  metadata probe, transactional Qt Source selection, WAV-only file dialog,
  read-only metadata panel, and platform/source/UI dependency contracts.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_007_NOT_PREPARED_OR_STARTED

## L1-M03 — Windows WAV playback and playback-rate adaptation

- Status: READY_FOR_REVIEW
- Baseline: `d6b265f96a2d50440fb0631914c6b372878aa4e9`
- Branch: `task/L1-M03-windows-wav-playback`
- Delivery subject: `CODEX TASK 007 / L1-M03`
- Scope: bounded `WavReader` playback through the frozen playback port,
  private Qt Multimedia output, deterministic exact-rate-first negotiation,
  playback-only `44100<->48000` SRC, minimal transport UI, and two-output
  Windows listening evidence.
- RF-1: MATERIALIZED; decision `KEEP_BASELINE`; frozen estimates unchanged.
- R12-01: MITIGATED_FOR_WINDOWS_L1-M03_44100_48000; residual risk remains
  open for other rates, devices, drivers, Android, and qualification levels.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_008_NOT_PREPARED_OR_STARTED

## L1-M04 — Real Source waveform overview and minimal peak pyramid

- Status: READY_FOR_REVIEW
- Baseline: `4469677b3f9b6511dfa84148fa63738c68d9b2e0`
- Branch: `task/L1-M04-source-waveform-overview`
- Delivery subject: `CODEX TASK 008 / L1-M04`
- Scope: Qt-free bounded min/max peak pyramid from canonical WAV decode,
  cancellable application-owned generation and ephemeral one-entry cache,
  bounded C++ scene-graph rendering, truthful Source/RAW states, and a
  read-only playback playhead.
- Manual gate: PASS after correction of a viewport-dependent vertical seam
  artifact with shared physical-pixel bucket boundaries.
- R12-01: inherited unchanged.
- R12-02: MITIGATION_MATERIALIZED_AT_L1-M04; residual remains open for L1-M05.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_009_NOT_PREPARED_OR_STARTED

## L1-M05 — Waveform navigation and single Audition Region

- Status: READY_FOR_REVIEW
- Baseline: `17c36204846560d247602e786b1851f624c255fd`
- Branch: `task/L1-M05-waveform-navigation-audition-region`
- Delivery subject: `CODEX TASK 009 / L1-M05`
- Scope: exact Source-frame waveform zoom/pan/seek, one half-open Audition
  Region, explicit loop orchestration, definitive segmented Region time
  editor, and the targeted private playback seek/loop forward correction.
- Manual gate: PASS with Product Owner clarification; `Ctrl++` verified,
  non-US `Ctrl+=` alias not required, and `Alt+F4` during the extreme
  mouse-captured gesture window accepted deferred/non-blocking.
- BC12-L1M05-EXPLICIT-SEEK-LOOP-001:
  RESOLVED_BY_FD12-L1M05-EXPLICIT-SEEK-LOOP-001.
- R12-01: inherited unchanged.
- R12-02: MITIGATED_FOR_WINDOWS_L1-M05; residual monitoring remains open.
- RF-1: MATERIALIZED; frozen estimates unchanged.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_010_NOT_PREPARED_OR_STARTED

## L1-M07 — Canonical Module Registry, Module Instance, and Dynamic Mastering Chain

- Status: READY_FOR_REVIEW
- Baseline: `830b8b9b54ce4a46d0b41a2705f87e6446f37c55`
- Branch: `task/L1-M07-module-registry-dynamic-chain`
- Delivery subject: `CODEX TASK 010 / L1-M07`
- Scope: static Qt/platform-free `rgsml_dsp`, exact `IModule` and immutable
  descriptor contracts, one canonical eleven-entry descriptor-only registry,
  stable module-instance identity, and atomic structural chain mutations.
- Baseline conflict:
  `BASELINE_CONFLICT_DISCOVERED — RESOLVED_BY_TARGETED_FORWARD_DELTA`;
  `TARGETED_DELTA_REQUIRED — MATERIALIZED`.
- DSP package: 11 descriptors; 0 production factories; 0 production
  processors; no DSP mathematics.
- Manual gate: NOT_REQUIRED_C_PLUS_U.
- R12-01: inherited unchanged.
- R12-02: MITIGATED_FOR_WINDOWS_L1-M05; residual monitoring remains open.
- RF-1: MATERIALIZED; frozen estimates unchanged.
- Alt+F4 active-capture residual: PO_ACCEPTED_DEFERRED / NON-BLOCKING.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_011_NOT_PREPARED_OR_STARTED

## L1-M08 — Gain and first chunked identity/Gain Render Preview

- Status: READY_FOR_REVIEW
- Baseline: `2e1bca98c589264c3a1b51e5367f19b425bd2975`
- Branch: `task/L1-M08-gain-render-preview`
- Delivery subject: `CODEX TASK 011 / L1-M08`
- Authority: `MISSING_AUTHORITATIVE_DSP_INPUT — RESOLVED_BY_L1-M08_GAIN_FORWARD_DELTA`.
- Scope: production immutable Gain-v1, registry transition to 11 descriptors / 1
  factory / 1 processor, Qt-free bounded same-rate chunked Render Preview,
  independent binary64 oracle, Source immutability, and a tests-only Windows
  listening harness.
- Human Listening Gate: PASS on `Altoparlanti (High Definition Audio Device)`
  with physically connected headphones; 0 dB, +6 dB, and -12 dB were accepted
  with no audible click, dropout, coloration, or chunk-boundary artifact.
- R12-01: inherited unchanged.
- R12-02: MITIGATED_FOR_WINDOWS_L1-M05; residual monitoring remains open.
- RF-1: MATERIALIZED; frozen estimates unchanged.
- RF-2: MATERIALIZED_AT_L1-M08; frozen Phase-12 estimates unchanged.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_012_NOT_PREPARED_OR_STARTED

## L1-M09 — IEEE-F64 WAV writer and safe Render Preview export

- Status: READY_FOR_REVIEW
- Baseline: `ae821ba9265f98cae9a6b0a08eafbde0c8e9a975`
- Branch: `task/L1-M09-wav-writer-safe-export`
- Delivery subject: `CODEX TASK 012 / L1-M09`
- Scope: deterministic bounded IEEE-F64 RIFF/RF64 `WavWriter`, Windows
  create-new writer adapter, fail-closed new-file transactional export,
  encoded-file SHA-256, canonical decoded `RGSDAU1` SHA-256, canonical
  `WavReader` validation/readback, and deterministic failure injection.
- Automated qualification: Debug 62/62; Release 62/62; focused Task 012
  Release 3/3; dependency/public-header contracts 27/27 in both configurations;
  tests-off and staged deploy smoke PASS.
- Manual Functional Gate: PASS using the tracked 48 kHz stereo listening WAV;
  Source collision and existing destination were rejected, Source remained
  byte-identical, and no successful candidate leaked.
- Output scope: IEEE_F64 RIFF/RF64 only; input scope remains WAV_ONLY.
- New external dependencies: NONE.
- ToolchainManifest: unchanged and byte-identical, SHA-256
  `49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11`.
- R12-01: inherited unchanged.
- R12-02: MITIGATED_FOR_WINDOWS_L1-M05; residual monitoring remains open.
- RF-1: MATERIALIZED; frozen estimates unchanged.
- RF-2: MATERIALIZED_AT_L1-M08; frozen Phase-12 estimates unchanged.
- Integration: NOT_MERGED_TO_MAIN
- Next task: TASK_013_NOT_PREPARED_OR_STARTED

## L1-M06 — Gold Reference Picker and truthful audition routing

- Status: READY_FOR_REVIEW.
- Baseline: `87605710b66d7dc8342f3e96db809d371fbb9e9f`.
- Branch: `task/L1-M06-gold-audition-routing`.
- Scope: app-owned PREPARED/PROCESSED/GOLD routing, independent Gold picker,
  two cue domains, strong Windows same-file rejection, and a private borrowed
  PCM input for the existing Windows playback engine.
- Authorized targeted forward delta: the existing playback SRC now consumes
  either WavReader-backed or borrowed immutable PCM-backed frames through one
  private bounded input seam. Mathematics, coefficients, rate/output policy,
  public `IAudioPlaybackService`, render/audio/DSP contracts, WAV-only input,
  and ToolchainManifest inputs are unchanged.
- Automated qualification: Debug 65/65; Release 65/65; focused L1-M06 5/5;
  dependency/public-header contracts 27/27; tests-off 0 tests; staged deploy
  smoke PASS.
- Material PO finding resolved: a 44.1 kHz WAV was playable as GOLD but not as
  PREPARED/Source because PCM playback disabled the paired candidate and the
  SRC input was WavReader-only. The permanent eight-case WAV/PCM exact/paired
  44.1/48 matrix is PASS with identical downstream SRC output.
- Material findings resolved: missing app/render link, missing QuickDialogs2
  QML composition, and incomplete private-source constructor placement.
- Human M+L gate: PASS, including minimal 44.1 kHz Source retest; Play available,
  correct playback, no evident click/dropout.
- Integration: NOT_MERGED_TO_MAIN.
