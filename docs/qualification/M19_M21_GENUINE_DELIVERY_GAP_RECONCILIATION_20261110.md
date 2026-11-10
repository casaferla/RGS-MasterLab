# RGS MasterLab — M19–M21 Actual Delivery Gap Reconciliation
**Date:** 2026-11-10
**Grounded candidate:** focused CI #53 green on Windows/Linux; `fbdfd06fa53364620c3f440b8e29b9b6e5b4518e`.
**Mandate:** end redundant hardening, reuse existing engine and owner, deliver qualified numeric Product UI only through a legitimate Phase-6 authority. This is a **read-only reconciliation report**, not a release gate or implementation authorization.

## 1. What already exists: DO NOT REIMPLEMENT

| Capability | Verified repository location | Status |
|---|---|---|
| Full-resource level analyzer M19 and tonal/stereo analyzer M20 | `analysis`, `app/src/internal/m21_product_owner_analysis.hpp` | Actual CPU analysis exists; the latter file is byte-identical to FQ #99 |
| Real M19/M20 typed conversion and Product claims | `app/src/internal/m21_real_product_metrics_port.hpp` | Already existed at FQ #99, byte-identical at #53 |
| Genuine SOURCE/PREPARED/PROCESSED RenderResult capture and live revalidation | `M21ProductOwnerAnalysis::capture` / `still_current` | Already existed at FQ #99 |
| Independent selected REFERENCE WAV and revalidation | `capture_gold`, `gold_still_current` | Already existed; limited to 128 MiB decoded PCM per worker call |
| Private delta engine, quality/state and three-column comparison | `m21_metrics_comparison_candidate.hpp` | Already existed at FQ #99 (supplementary validation appended later) |
| Qt-only private result inbox | `M21ProductPublicationInbox` | Already existed at FQ #99; now accepts authenticated async completion |
| Actual private asynchronous Product runtime | `m21_private_single_flight.hpp`, `m21_private_product_analysis_coordinator.hpp`, `app/src/main.cpp` | Implemented in focused CI #46, strengthened in subsequent CIs; status-only Qt |
| M19/M20/M21 persistent independent GUI panels, icons, source selectors | `ui/qml/AnalysisStatusEditor.qml`, `AnalysisApprovedContent.qml`, `DspEditorHost.qml` | Already integrated; vector icon CI #45, read-only status UI |
| No numeric Product authority | `M21ProductStatusViewModel::production_authorized() const noexcept {return false;}` | Intentionally not released |

Baseline comparisons checked: `m21_product_owner_analysis.hpp` and `m21_real_product_metrics_port.hpp` byte-for-byte unchanged between FQ #99 `5e4871d5849e4121b0612c2142d1e051d6c9a3d2` and #53. Do **not** recreate analyzers, M18 epochs, comparison algorithms, status DTOs or QML panels.

## 2. What specifically still blocks numerical Product display

**B1 — Canonical authority, not another private registry.** The frozen PO P01–P06 document dated 2026-10-05 declares `PHASE6_IMPLEMENTATION_AUTHORIZED_BY_THIS_DECISION=NO`, `GATE_G_EXECUTED_BY_THIS_DECISION=NO`. No qualified versioned `FeatureDefinitionRegistry` / official `FeatureStatus` proof has been shown for these Product outputs. A private `ProductMetricsOwnerClaim` with `external_product_owner_confirmed=true` does not by itself grant official Phase-6 status. Reconcile with existing Phase-6 canonical definitions and request one explicit implementation/governing gate authority before enabling numeric QML.

**B2 — Producer signatures, ownership and scope.** Bind canonical feature ID/version, immutable current audio realization and true owner generation, selected analysis scope, sample rate/rate policy, quality and producer configuration/dependencies. Preserve PREPARED, independent REFERENCE and PROCESSED separately. Never synthesize a numeric zero for missing/stale/unqualified data. M16 True Peak stays unavailable until M16 authority is qualified.

**B3 — Presentation contract.** Existing QML shows all three pages and status. Reuse the existing Product inbox and `M21ProductStatusViewModel` rather than a second numeric ViewModel/worker/registry. Bind only genuinely released, current, certified feature cells, PSD bands and delta. Preserve scope and independent REFERENCE; any noncurrent or unsupported feature must display `UNAVAILABLE` or `STALE`, not fabricated values. Keep original Qt geometry/7 core icons and persisted `GOLD` APIs.

**B4 — Real REF scope coverage.** The current `measure_gold_worker` refuses more than 128 MiB of decoded PCM: at high sample rates that excludes ordinary full-length tracks. A bounded streaming/canonical measurement path is required before declaring unrestricted independent REFERENCE analysis, with genuine PCM, no source fallback and deterministic full-scope calculations. Do not remove the limit without replacing the memory-safety protection.

**B5 — Validated Product delivery.** Dedicated tests must prove selected PREPARED/REFERENCE/PROCESSED role, same-rate and six-rate cases, malformed/stale/invalid scopes, source/project/chain/limiter changes mid-measurement, independent REF PCM/clock, numeric authenticity against the preexisting analysis engine, and real QML data gating. A focused CI is not Product Gate G, physical acceptance, integrated FQ or M18 export B01.

## 3. Reconciled invariants and over-hardening to stop

- M18 fanout `platform/windows/src/internal/m18_owner_mutation_fanout.hpp::status_admission` advances the owner epoch for standalone authorized mutations (including set_prepared/set_processed). SOURCE/Project batch children intentionally share exactly one enclosing epoch. Qt event-loop worker scheduling is not reentered during an ordinary synchronous SOURCE batch. Therefore same-epoch render substitution should be treated as an exceptional guarded branch, **not** evidence that every render requires a new epoch subsystem.
- `M21PrivateRenderAttempt` weak_ptr dedup was added after #46, but `M21ProductPublicationInbox` still holds `preparedCapture_` and `processedCapture_` with owning shared RenderResult references. Weak dedup alone does **not** prove release of all retired PCM. Avoid blanket memory claims and new wrappers until a real test demonstrates a lifetime problem.
- A genuine second completion on the same M18 epoch may be analyzed but `PrivatePublicationGuard::expect` refuses its non-increasing generation. Within the legitimate single SOURCE batch the worker does not run synchronously; avoid adding an arbitrary secondary generation to work around a scenario not demonstrated in Product.
- M19/M20 actual computation is unchanged since FQ #99; later #43→#53 totaled 68 commits, primarily worker dispatch, source validation, Qt notification and status-copy optimization. All CI green checks establish the particular tested source invariant, not that Phase-6 numerical GUI delivery is complete or the extra complexity was required.
- DO NOT add further speculative validation/DTO layers or another always-on polling observer. Measure genuine blockers and complete B1→B5.

## 4. ONE deliverable-oriented next macro-lot after governing authorization

**Workstream A (read-only first):** Map existing Phase-6 canonical registry/signature definitions and the exact P01–P06 feature set to actual M19/M20/M21 Product producer output. Generate an evidence table with each canonical ID, version, scope policy, owner token, dependency fingerprint, and currently certified/missing gate.

**Workstream B (implementation only once approved):** Reuse the preexisting authentic worker and inbox; emit canonical, versioned Product feature receipts for supported outputs, under live owner revalidation and fail-closed status, with no numeric QML until authorized. Keep M16 TP masked until verified. Bind validated existing QML displays in the same lot with Product tests. Do not bypass P01–P06 or Gate G.

**Workstream C (distinct, real functional limitation):** Plan/test a bounded streaming REFERENCE measurement that handles normal full-length WAV at six rates without unbounded PCM allocation. Do not silently mix with B's feature authority or claim complete REF support before this is solved.

**Workstream D:** Run focused Windows + Linux CI then official canonical Gate S/U/G, PO physical Windows M18 export/GUI tests and integrated FQ separately. PR #99 remains Draft, `main` untouched; no automatic merge.

## 5. Immediate decision / execution boundary

This reconciliation does not create a Gate G or imply PO accepted implementation of the **entire** Phase-6 canonical contract. No new code or CI should be launched solely to enhance private status. The next code change must have a Product-visible acceptance criterion (e.g. authentic certified LUFS/RMS readout, real spectrum, or bounded full-length REFERENCE) and prove all required authority first. Preserve Single-Writer Epoch and one final focused CI per genuine macro-lot.
