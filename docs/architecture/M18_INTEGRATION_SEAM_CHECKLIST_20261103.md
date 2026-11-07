# RGS MasterLab — M18 isolated implementation-to-product reconciliation map

**Date:** 2026-11-03 · **Authority:** no new frozen product semantics · **Status:** hold for accepted M17, post-M17 CI reset and explicit PO integration approval.

## What is operationally complete without M17

The isolated M18 C++ preproduction lane has a single deterministic terminal codec (Philox4x32-10 TPDF/RN64 PCM24 or no-dither IEEE Float32), six-rate mono/stereo RIFF/RF64 planning, buffered byte-stream SHA-256, typed delivery identity, readback verification, and no-overwrite staged file publication. Its `M18MasterFrameProvider` accepts immutable **final OUTPUT/Master-clock binary64** frames at explicit absolute addresses. No independent SOURCE, MASTER or DAC engine is created. The native CI tests exact oracles, 87 proposed statistics, low-level decorrelation, 24 on-disk cross-matrix WAV files and a post-decoded True Peak numerical seam reusing **the unmodified M16 FIR implementation**.

A green M18 preproduction CI does **not** ship this service in the application. The existing Windows transactional publishing and Audio WavWriter remain product authorities.

## Exact live reconciliation targets (read-only inspection)

| Obligation | Existing repository owner / frozen boundary | Required post-M17 integration |
|---|---|---|
| Immutable terminal Master PCM | Accepted M17 prepared terminal MASTER ownership, not the DEVICE-rate monitoring buffer or SOURCE window | Bind the existing read-only Master frame owner to `M18MasterFrameProvider::read_exact`; prove absolute frame, channels, selected MASTER Fs and full-resource ownership |
| Terminal order | Final Output SRC → M17 True Peak Limiter → M18 Dither → Quantizer → Encoding | Do not insert an SRC or correction after M18; do not replay M17 for a second render |
| Project output state | `project/include/rgsml/project/project_document.hpp` (`ProjectDocument::output` as `OpaqueJsonValue`); `project/src/master_output_rate_preference.cpp` recognizes `targetMasterOutputRateHz` only | Formal schema authority for output PCM24/Float32 and exact `uint64 ditherSeed`; preserve all unknown JSON fields; do NOT silently write unsupported keys from preproduction |
| Render/evidence identity | `render/include/rgsml/render/render_request.hpp` is PREVIEW-only; `render_result.hpp` has live realization identity; M18 current `m18_delivery_identity.hpp` is test-only typed receipt | Map to the actual full terminal render/evidence owner, preserve exact seed and algorithm/quantizer versions, record committed WAV/payload SHA on success |
| WAV container & destination | `audio/include/rgsml/audio/wav_writer.hpp` is currently F64 WavWriter with `IResourceWriter`; Windows `TransactionalAudioExporter` owns OS publication | Adapt frozen PCM24/F32 byte producer to existing canonical writer/export infrastructure, **not** register a competing public exporter; real Windows flush/durability/ACL/link checks |
| Error contracts | `core/include/rgsml/core/error.hpp` defines stable `ErrorCode` families (InvalidAudioSample, IntegerOverflow, IoFailure, InvalidState, etc.) | Deliberate error classification; never collapse clipping, cancellation, destination collision and bad file into silent retry or successful render |
| Post-delivery TP | `analysis/src/true_peak_measurement.cpp` wraps the existing four-phase 12-tap M16 FIR and 11 analyzer-only EOS zero frames | Feed independently decoded, committed delivered WAV sample values to canonical M16 ownership; record before/after TP and a truthful exact-ceiling outcome, without hidden postquant correction |
| Qualification & physical UI | M18 Draft #63 M18-only CI, mathematical pre-audit proposed 87 checks, existing M17 branch PR #61 | Adopt final Gate G bounds, build Windows product FQ, test six rates and formats, true WAV/RF64, listening/cancel/progress, then separate PO authorization for merge |

## Gating and safety

1. M17 accepted with actual Windows FQ and physical/listening proof; do not infer this from green GitHub checks.
2. Explicit post-M17 CI architecture audit/reset, dual-OS green, original 16 FIR package verification and preserved qualification coverage.
3. Advance accepted M17 forward into M18 with exact-SHA ancestry reconciliation; retain M18's isolated test & codec history; **never** merge unqualified M18 into M17, PR #61 or `main`.
4. Bind existing immutable Master owner and **existing** writer/project/error interfaces; no alternate public engine, project schema, SOURCE/DEVICE routing or user-facing dither-Off control.
5. Run full M18 independent math, exact-byte, edge/overflow/spectral, decoded M16 TP and physical FQ. A separate PO Go is required for product integration/merge.

## Strict exclusions from independent completion claims

Preproduction non-overwrite hard-link staging is **not** a certified Windows durable transaction. Memory-bounded native test fixtures do **not** establish a physical multi-GiB RF64 export. The actual qualified M16 final-delivery TP method, external JSON seed persistence, Windows app GUI, audio FQ and user listening **cannot truthfully be asserted delivered until those owned integration tests run**. No existing M17 source or full-resource witness has been altered or qualified by this M18 lane.
