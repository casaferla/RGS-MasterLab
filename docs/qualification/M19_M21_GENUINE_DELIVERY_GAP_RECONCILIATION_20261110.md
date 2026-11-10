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

### B2a — Identified producer-to-consumer gap in the real code (M19 → M20)

This is a **verified functional dependency**, not another validation-layer requirement:
- `analysis/src/internal/m19_basic_level_candidate.hpp::gate_complete_400ms_block_powers` computes final retained-block **count and integrated LUFS** from the 400-ms/100-ms loudness grid, but `BasicLevelPreproductionSnapshot` and the public `BasicLevelAnalysisResult` do not expose the actual retained block **identities/intervals**.
- `analysis/src/internal/m20_m19_phase6_gate_port.hpp` already implements `M19RetainedDisclosureCandidate`, comparing exact sample-rate, scope, channel count and P01 bank hashes; its `producer_disclosure_missing` branch correctly refuses to infer retained intervals from integrated LUFS/count.
- `analysis/include/rgsml/analysis/tonal_stereo_analyzer.hpp` already accepts `TonalStereoTrustedSupport` with `retained_width_blocks`, `spectral_frame_mask` and `authoritative_band_eligibility`, but `M21ProductOwnerAnalysis::measure_render_worker` and `measure_gold_worker` invoke `measure_tonal_stereo_scope` **without** such a producer-owned support (thus its default empty support is used).
- P06-B expressly freezes the broadband stereo active support as a **loudness-gated union** and forbids ALL_SCOPE fallback where the active gate is available. A M20 result computed with `width_all_scope_fallback=true` must therefore **not be mistaken for the canonical P06 width**. PSD / band eligibility is a separate producer certificate; do not manufacture its mask from LUFS or return a normalized shape dB from linear band fractions.
- Existing M20 external canonical identities (do not rename or duplicate): `rgsml.tonal.profile.normalized.fractional_octave_1_12`, `rgsml.tonal.profile.normalized.fractional_octave_1_3`, `rgsml.stereo.width_ratio`, `rgsml.stereo.correlation.series` (validated by `correct_m20_phase6_feature_identity`).

**First implementation-grade vertical slice after governing go:** augment the *existing* M19 gated block producer with **exact retained-block frame intervals**, carrying the frozen P01 bank ID/hash and scope/owner evidence. Feed those intervals through the already-existing M20 gate-port and `TonalStereoTrustedSupport` to compute certified P06 width without an all-scope substitute. The authoritative spectral-frame mask and per-band eligibility require their own explicit producer evidence; do not pretend that the retained interval list alone qualifies the PSD/profile. Test silence, no retained blocks, partial scope, six sample rates and reference-independent clocks against the existing engine before any number appears in QML. Do **not** create a second loudness gate, a new FFT, or another full PCM buffer.

### Workstream B2a candidate implementation — staged and NONRELEASE (10 Nov 2026)

The actual existing M19 `BasicLevelPreproduction::finalize()` now discloses the exact union of its **already computed** retained 400-ms/100-ms loudness blocks as absolute frame intervals; the strict -70/-10 linear-power gate, frozen P01 coefficient tables, integrated LUFS computation, per-scope K state and original analysis entrypoints remain unchanged. No second loudness gate was added. The public `BasicLevelAnalysisResult` carries the typed list and distinguishes AVAILABLE_EMPTY from an unavailable gate via `retained_block_disclosure_complete`.

The already existing Product worker `M21ProductOwnerAnalysis` now feeds those real intervals to the already existing `TonalStereoTrustedSupport::retained_width_blocks` for genuine PREPARED, PROCESSED and independent REFERENCE scopes. This prevents an unjustified P06 `width_all_scope_fallback` where M19 has real retained-gate evidence. No new M20 FFT, normalized band eligibility, spectral-frame mask, producer fingerprint, official FeatureStatus, Gate G, M16 TP qualification, or numeric QML binding has been introduced.

**Proof required before calling even this narrow bridge CI-validated:** `rgsml_basic_level_analyzer_tests` and `rgsml_tonal_stereo_analyzer_tests` MUST be built and **executed** on BOTH Windows and Linux, with real PCM six-rate cases, full and partial scope, silent/no active blocks, empty-gate no fallback and exact reference scope. The dedicated steps were added to the M23 focused workflow while retaining the immutable exact M23 JUnit inventory and FQ99 GUI tests. The bridge is staging-only until that CI passes; even a double green does not authorize Phase-6 numerical UI.

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

## 6. B4 status update — functional implementation and Product acceptance (10 Nov 2026)

The original B4 observation above remains historical context, **not** the state of the current candidate. The staged and focused candidate `d74451cc626f75737bc8f25bbe5ffa6288d61a8f` implements the formerly missing >128-MiB independent REFERENCE WAV measurement in `M21ProductOwnerAnalysis::measure_gold_worker`. Per PO feedback, its focused Windows/Linux CI was green. The original <=128-MiB owner worker remains unchanged in behavior; larger captures call `analysis::measure_streamed_reference_scope` using existing M19 and M20 kernels on bounded decoded PCM and the same authoritative M19 retained union for P06. Real WAV tests cover >128-MiB mono and stereo, 6-rate comparison cases, silence, short reads and correlation. The resource-staging memory is bounded by block/window size; retained outputs remain length-dependent.

A *distinct* genuine Product integration test has now been added to `tests/integration/app/test_project_session.cpp` on the no-CI writer branch: after real M18 Qt binding, it selects a physically separate sparse-backed 192-kHz stereo RIFF (>128 MiB of decoded PCM), captures a fresh owner epoch on Qt, measures it using `measure_gold_worker` on a worker thread, then publishes through the existing `M21ProductPublicationInbox`. It checks native P06 width/correlation values, independent rate, unavailable M16 TP, numeric QML blocked, and stale invalidation upon clearing REFERENCE. This additional Windows test is **not yet qualified** until the next focused CI passes; the historical M23 required-test inventory remains unchanged. No new DTO/registry/worker/GUI or duplicate analyzer has been created.

**Current remaining true blockers for Product numeric delivery:** governing Phase-6 implementation and Gate G authorization (the 2026-10-05 accepted/frozen P01–P06 document expressly says `PHASE6_IMPLEMENTATION_AUTHORIZED_BY_THIS_DECISION=NO` and `GATE_G_EXECUTED_BY_THIS_DECISION=NO`), versioned official FeatureStatus receipts, resulting actual QML number binding, and M24 physical/PO listening qualification. Do not replace that missing authority with private `external_product_owner_confirmed` claims or call a focused CI an official Gate G. M23 Gate M and M24 Gate M/L require actual Product Owner Windows sessions.
