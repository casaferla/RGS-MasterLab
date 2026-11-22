# L3 AutoMatch / P2 — performance and placement integration audit (2026-11-22)

Status: writer-only implementation audit, updated with the **explicit PO decision on 2026-11-22**: AutoMatch SHALL optimize P2. This changes the 2026-11-13 provisional *with/without P2 eligibility* choice; it does not silently rewrite the frozen L3 numerical search contract, certify the current Product M13 implementation, or authorize release.

## Evidence grounded in the current writer

- PO target: a useful verified 5-minute offline AutoMatch in ~20–30 minutes is acceptable, **not yet proven**. M10 has a frozen maximum of 800 whole-resource render/analysis attempts; 800 is a cap, not a mandatory iteration count. No implicit early stop, altered solver policy, omitted analysis, or approximate replacement for a scored candidate.
- Physical Windows research on 300 s stereo 48 kHz: 100 fresh full five-module/P2-active Product render previews summed to 1,137.224 s (~18m57s), mean 11.372 s per candidate. This excludes canonical 27-feature analysis, L3 solver, M13 verification, and UI overhead. Standalone P2 accounted for ~63–65% of full chain cost in the associated isolated marginal study; these are research measurements, not an end-to-end L3 SLA.
- P2 batched callback: opt-in 64-frame safety quanta, benchmarked against exact PCM and EOS oracle. It improves a researched implementation path, not a substitute for actual physical stress of expensive fallback/projection paths.
- Product `MasteringPreviewController`: worker-only, PCM-bit-exact cold-oracle verified four-module prefix LRU, bounded to 256 MiB. Eligibility is exactly Gain/EQ/Compressor/Stereo M-S/P2 and requires Compressor **and** Stereo M-S bypassed. It does not establish general stateful-stage checkpointing or automatically serve the L3 M10 evaluator. A full 300 s, 48 kHz, stereo float64 prefix uses 230.4 MB (~219.7 MiB), so the limit holds at most one such exact prefix.
- L3 M10 `ExactCompletedEvaluationCache` caches only complete authoritative scores for **identical** execution keys, not intermediate PCM or a changed candidate; its adapter explicitly does not implement the Product worker. M09 proxy can order/preselect proposals but must not be passed off as authoritative M10 render/analysis.

## Architecture gap requiring explicit closure

- The current P2 Product preview is appended as fifth **MANUAL** processor. M12 explicitly prohibits P2 as an auto-created REF_LINKED node. M13 `probe_fresh_m13_full_native_master_once` processes **PREPARED -> protected MANUAL -> REF_LINKED**. Thus an active MANUAL P2 followed by active REF_LINKED artistic nodes would execute P2 **before** AutoMatch corrections, not as the final musical limiter. An independent M16 True Peak check afterwards is necessary but does not prove P2 was in the intended musical signal position.
- Before claiming complete M13 verification with P2 active, implement and test a single product-authorized logical graph preserving all MANUAL settings and user ownership while evaluating artistic corrections before the streaming limiter, with exact DSP order, latency/EOS and cache-key identity. Do not silently reorder persisted nodes, rewrite frozen L3 semantics or conflate P2 with M17.
- **Superseding PO decision 2026-11-22:** P2 is an **eligible and mandatory optimization component** of the completed AutoMatch search path, not merely a permanently manual post-effect or an optional research alternative. Optimize its product-exposed typed **Ceiling and Drive** within validated admissible ranges; preserve the current margin policy (2 or 5 percent) as a separately owned fixed/configured setting unless and until an explicit solver policy authorizes varying it. No promise that every trial changes both coordinates; all genuine candidate scores must reflect P2 processing at the exact chosen settings. The 2026-11-13 *whether to include P2* A/B debate is closed; comparisons with a held-fixed/bypassed P2 now serve only regression, performance attribution, and evaluation of musical benefit, not an eligibility decision. M17 WAV Export remains outside the solver.

## Narrow implementation sequence and gates (no speculative full rewrite)

1. Complete current M12/M13 build repairs and the next coherent focused macro gate; retain single writer and writer-vs-CI separation.
2. Make the logical audio order explicit, exercise active P2+active REF_LINKED candidates at 44.1/48/96k, bypass/parameters and tails; compare one-pass canonical output to staged execution exactly. M16 remains actual pre-export PCM safety analyzer. M17 stays independent WAV Export and is not an M10 DSP coordinate.
3. Attach exact Product-owned M10 candidate evaluation to existing in-memory whole-song render and all canonical feature measurements, not GUI and not M17. Analyze PREPARED and REFERENCE only when dependency changes. Preserve 800 cap/policy and independent fresh M13 verification.
4. Reuse verified immutable stage-prefix caching **where eligible** across coordinate sweeps (with full stage state, timebase, latency/EOS, sidecars), budget RAM, invalidate strictly on upstream mutation. Never reuse a P2 result when its input PCM or configuration changes. Exact completed-score cache hits cost no evaluation attempt.
5. Measure 20/50/100 completed **real** L3 evaluations with **P2 Ceiling/Drive participating in the candidate search**, plus a held-fixed/bypassed diagnostic control; record cache-hit ratio, elapsed render/analysis/solver/verification, peak RAM, audible quality and fallbacks on PO Windows. Only then assess the 20–30 minute objective or propose an explicit revised solver policy.
6. P2's current manual-only registry placement, opt-in/persistence limitations, M12 forbiddance and the M13 MANUAL-before-REF_LINKED ordering MUST be reconciled by a typed, owner-safe integration. Until done, do not claim real Product AutoMatch optimizes P2. Keep the manual chain unchanged on rejected/stale/cancelled plans; do not mutate M17 ceiling or force its export stage during M10 iterations.

Source references:
- https://docs.google.com/document/d/1O_1nXiROO-vAA8w0eOk7DP3wysx5bIG80XONJZwtGKY/edit
- https://docs.google.com/document/d/1iYhmqTtwuSdEwZ25pv8Cgp3eGs_C2rD2p3hd5nTBHM4/edit
- https://docs.google.com/document/d/1zong-l4lYDpVU-nHR97r08GrI4tdHMhztYbp_p6v7yA/edit

## Explicit owner requirement — 2026-11-22 (supersedes earlier optionality only)

- **Product intent:** the normal AutoMatch candidate path must be capable of searching the P2 streaming limiter **Ceiling [−12, 0] dBTP** and **Drive [−24, +24] dB**, subject to future frozen L3 admissible search bounds, verified safety and music-quality constraints. These are the existing typed P2 validation limits, *not* permission for an arbitrary dense search over all values. The solver cannot simply leave P2 manual because it is costly.
- **Fixed vs owned:** preserve pre-existing user MANUAL nodes, bypass and user control. Bind selected P2 settings to an AutoMatch-owned REF_LINKED logical correction (or an explicitly approved equivalent single-owner final-stage configuration) without applying inconsistent separate P2 processors. Preserve user adjustment after AutoMatch and invalidate stale verified output when changed.
- **Processing order:** PREPARED → protected fixed MANUAL DSP → REF_LINKED artistic corrections including Dynamic EQ/Transient Shaper → exactly one effective P2 **as the final streaming DSP** → M16 measurement of the actual generated source-rate PCM. M17 is exclusively final WAV Export, with its own independent ceiling, outside M10.
- **Performance:** no bypass or stale-cached PCM is counted as an actual P2-active objective; recompute P2 when its input or typed settings change, but reuse verified immutable upstream prefixes and completed scores where exact dependency keys permit. No extra GUI/render publication for each M10 evaluation; do fresh M13 full verification of the winner.
- **Traceability:** this is an owner decision with implementation gaps, not an assertion that P2 optimization currently works. Change code, tests, benchmarks and any required L3 frozen-authority delta coherently before release.

## Implementation receipts (writer, nonrelease)

- `37ef2586`: exact typed `make_p2_scalar_trial` for Ceiling or Drive with Product graph snapshot validation and manual Margin preservation; new `rgsml_l3_p2_typed_trial_tests`.
- `900bb0d9`: the guarded fresh M13 probe now fails closed if an active P2 lacks an explicit typed M10 winner. It renders the protected MANUAL prefix, then REF_LINKED and only then the genuine P2 suffix. M16 measures the actual P2-processed output, not an M17 export result.
- `c68681ef`: native synthetic regression tests for active P2, altered Drive, wrong Margin, output difference, segment count and no false VERIFIED/export grant. Same existing M13 test target.
- These changes are incremental. The canonical M10 whole-resource evaluator, accepted P2 winner receipt/ownership in persistent REF_LINKED Product, and full physical timing budget are NOT yet completed or qualified. A single CI gate will cover the staged compilation and tests when the macro-lot is promoted.

## M10 typed P2 bridge / writer checkpoint (2026-11-22)

The completed CI #35 `685ffbef` is GREEN on Windows and Linux. Since that gate, the writer has accumulated a coherent nonrelease M10/P2 integration lot:

- `l3_m10_p2_canonical_coordinate_port.hpp`: both exact canonical P2 field IDs are required in the frozen full-coordinate inventory. The real inverse transform is provided by the authoritative M09 owner, *not* invented as a dB normalization here. Unknown/duplicate/missing/nonfinite P2 coordinates reject before rendering.
- Effective typed P2 bindings may equal the protected MANUAL settings when other artistic coordinates change; those valid full candidates are not spuriously rejected.
- `l3_m10_p2_exact_cache_evaluator.hpp`: candidate-specific, correctly typed Ceiling/Drive go through existing M10 attempted/complete/cache-hit accounting. Every hit still requires current Product owner preflight; no 801st full execution. The full canonical M07-M11 key remains mandatory, with a length-delimited codec-verified P2 payload appended to avoid cross-parameter cache aliasing.
- `l3_m12_p2_exact_winner_preflight.hpp` and `l3_m13_p2_owned_full_master_probe.hpp`: exact nonpublishing winner guard binds the P2 typed settings, winner ticket/checksum, plan, M11 frozen MANUAL snapshot and current scope to the fresh M13 full PCM probe, which executes P2 after REF_LINKED and runs real M16. This guard is **NOT** an actual Qt Product M12 committed Project receipt.
- Added QT tests (synthetic fixture only for M10 cache/coordinate adapter, fresh genuine Product DSP PCM for M13) to the existing focused macro suite. No claim yet of an L3 runtime 27-feature worker, verified Product persistence, 20–30-minute end-to-end proof, release authority or M17 intervention.

Remaining must-do: the real M09 inverse/field registry, full Product M10 in-memory 27-feature worker, explicit approved P2 owned effective/REF_LINKED persistence after manual changes, M12 committed receipt + independent final M13 verified publication, and physical 5-minute benchmark. Do not close these tasks on the basis of the focused gate.
