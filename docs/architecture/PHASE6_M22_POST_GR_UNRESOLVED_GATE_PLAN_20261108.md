# RGS MasterLab — Phase-6 release eligibility / M22 generalized planner after qualified M17 GR HISTORY

**8 November 2026 · isolated architecture and nonrelease preintegration · not a PO release gate · NO MAIN MERGE.**

## Immutable green baseline

- M18–M22 UI/Product preintegration #7: Windows/Linux SUCCESS, SHA `3aca7bdf4d96269d86e6690617642b876dcb3cbc`, CI https://github.com/casaferla/RGS-MasterLab/actions/runs/42492719918.
- Rebased M17 GR HISTORY #2: Windows/Linux SUCCESS, SHA `c096637b01ba31b8ea83055d35bdb16a20e3c487`, CI https://github.com/casaferla/RGS-MasterLab/actions/runs/42497222058, PR #97 Draft.
- Last GR CI #2 artifact contains **four real Qt captures** of a synthetic GR fixture, including whole M17 editor at 25% and 75% (966 × 480). Text glyphs are boxes in the headless renderer, so only geometry/cursor/shape are claimed, **not** real font/physical PO acceptance.
- Branch created for the next independent single-writer lot: `staging/post-gr-phase6-m22-preintegration-no-ci-20261108`, based **exactly** on the GR HISTORY #2 green commit. All predecessors and candidates remain frozen. Do not start new CI until a coherent change set exists.

## Phase-6 authority — important stop-line

Drive frozen **Phase-6 Residual Semantic Closure P01–P06**, ID `1rBiwPOz5lMGDMGuKyFNVzw9d9SIBhS3Znx3aWVr-eE4`, authorizes **semantic decisions**, not product code release or Gate G. Explicit frozen tokens `PHASE6_IMPLEMENTATION_AUTHORIZED_BY_THIS_DECISION = NO`, `GATE_G_EXECUTED_BY_THIS_DECISION = NO`. A later live-repository reconciliation and a Product Owner implementation authorization are separately necessary.

Accepted binding choices to preserve before canonical implementation:
- P01: six target clock banks 44.1/48/88.2/96/176.4/192 kHz with exact bank identity/hash.
- P02: `MEAN_CHANNEL_POWER`, never summed-channel or per-channel substitute.
- P03: `CONTINUOUS_FFT_CELL_PSD_FIRST_MOMENT`.
- P04: `DIRECT_RATIONAL_200HZ_GRID`, current-frame-center events, next-power-of-two FFT, inclusive bin-center frequency membership, raw positive spectral flux, robust Z separately.
- P05: defer undefined macro-dynamics occupancy rather than fabricate a feature.
- P06: `LOUDNESS_GATED_ACTIVE_UNION` and canonical K-weighting scope-state origin.
- Include `rgsml.phase6.residual-semantic-closure/1.0.0` or individual policy IDs in **future official** signed feature dependencies. Do not rewrite historical GOLD persisted identifiers to REFERENCE.

**Current repository reality:** `app/src/m21_product_status_view_model.hpp` has `production_authorized() const noexcept { return false; }` and `M21ProductQtReadinessBridge` returns status/quality only, with all numeric payloads masked. The M19/M20 real worker and M21 Inbox can accept true numerical results, but that is NOT the canonical `FeatureDefinitionRegistry` nor its versioned `FeatureStatus`. No second registry or parallel persisted schema may be introduced. Do not turn `productionAuthorized` true or bind numerical QML data until genuine canonical registry, producer signature, active scope, epoch and release status are materialized and qualified.

## M22 exact implementation gap, not M18 success rewritten

- Genuine current Product path `M22ProductVerifiedDispatch` already reuses the *same* M17 fully verified binary64 Master, true epoch and M18 user-consented transactional PCM24/Float32 export with SHA/ACK. This is an actual working subset.
- `render/src/internal/m22_request_plan_candidate.hpp` is deliberately **PRIVATE metadata-only preflight**. `supported_clock` lists six rates; `validate()` explicitly rejects `processing_rate_hz != target_rate_hz`. This is NOT the target generalized Final SRC workflow.
- `render/src/internal/m22_canonical_chain_port_candidate.hpp` maps authentic Master MANUAL registry and typed bindings but still expects **external** frozen parameter fingerprints, active latency and factory proof; it never instantiates the real DSP executor.
- A generalized M22 planner must bind **real Q0 FIR kernel selection** for each supported target conversion, checked output length/timebase, exact source vs output frame mapping, immutable module factory bindings and latency, global-M17 terminal verification through true EOS, preview from full checked Master (never region-only limiter), and absolute Philox dither frame addressing. No helper may invent FIR identities, inferred output sample counts or M16 TP witness.
- Next safe code lot: build a *typed, fail-closed, read-only* provenance adapter and independent adversarial tests for rate/timebase, preview absolute window/EOS, stale source/chain epoch and source-vs-output frame domains; then bind to authoritative Final SRC assets and real Product execution without replacing checked M17/M18 backend. Do not mark the generalized scheduler complete from a metadata-only plan.

## Final release gates

- M21 Gate S technical status/epoch/numeric evidence, M21 Gate M physical Windows functionality. M19/M20 respective authority gates. Phase-6 canonical registry and Gate G release are NOT equivalent to CI passes.
- Physical PO M18 A01–A15, Windows 4K+FHD panels, playback/crossfade, full-integrated FQ deferred only until PO has PC; all code-side work continues meanwhile.
- PR #96, PR #97 Draft; no unapproved merge on `main`. Next CI should include M17 GR, public REFERENCE, M18–M22 Product tests and dedicated new Phase-6/M22 qualification in one focused gate.
