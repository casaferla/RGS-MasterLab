# RGS MasterLab — P2 batching integration gate (L1 staging preview), 2026-11-14

**Status: Product-preview OPT-IN candidate. Not released, not M24 physical-qualified and not an L3 performance SLA.**

## Explicit scope

- Only the already opt-in `ModuleRegistry::with_streaming_true_peak_preview` Product registry path enables batching. Both its untyped factory and its *typed* `StreamingTruePeakLimiterParameters` construction select `Configuration::batch_safety=true`.
- `Configuration::batch_safety` defaults to **false**. All ordinary `CallbackEngine`, `EpochSession`, research test and unregistered production paths keep the original per-sample behavior unless independently opted in.
- `CallbackEngine::process` delegates to `process_batched` only when the prepared immutable flag is true **and** the callback contains at least 64 input frames. Smaller callbacks execute the unmodified `process` algorithm.
- `process_batched` is defined consistently for every compile unit, avoiding a macro-dependent module ABI/ODR divergence. The benchmark's `RESEARCH_P2_BATCHED_CALLBACK` macro remains a test-target build marker only, not a Product ABI switch.
- The pinned `PredictiveMusic` and `Stream` kernels, ITU 4x verification, EOS and latency mathematics are untouched. The batched path feeds original `Stream::submit` with up to 64 post-predictive frames, drains output after each quantum, preflights each full callback before history changes, allocates no heap memory or I/O in callbacks.
- There is **no new persisted parameter** or QML control. Project P2 non-persistence/fail-closed save guard remains in force. No changes to M17 terminal TP, M18 final format, Final SRC, canonical analyzers, `main` or release packaging.

## Qualification trail

- Focused CI [#43632611322](https://github.com/casaferla/RGS-MasterLab/actions/runs/43632611322): 7/7 GREEN. Original versus isolated batched candidate exact for 336 cases, EOS included. On short synthetic workloads, median process-time reduction for block size 64–4096: approx 6.2% Linux, 9.4% Windows focused; 1-frame callbacks sometimes slower.
- CI [#43634270152](https://github.com/casaferla/RGS-MasterLab/actions/runs/43634270152): 6/7 GREEN; Windows PowerShell parser rejected near-ceiling TSV rows because six-place rounding raised a valid value. **Not a safety failure**; original and batched PCM compared correctly in the native benchmark.
- CI [#43635422076](https://github.com/casaferla/RGS-MasterLab/actions/runs/43635422076): 7/7 GREEN; full 588 cases and independent, non-tolerant M16 ITU 4x post-gain ceiling meter passed, including 11 final FIR tail zeros and EOS. Logged Windows `P2_BATCHED_588_CASES_PCM_BIT_EXACT_AND_M16_TP_SAFE=PASS`.

## New integrated verification

Run the actual `ModuleRegistry::with_streaming_true_peak_preview` factories (both typed and default) through the RGS `IModule`/planar host bridge, and compare all PCM+EOS with a **separately configured original** `EpochSession`. The `r1d_p2_real_imodule_tests` additionally test an explicitly batched module at all six rates, mono/stereo, 7/64/512 frame blocks, with existing allocation guards and nonzero origin, versus original `EpochSession`.

Re-run full integrated L1/P2 focused CI on both Windows and Linux. A green CI proves *synthetic* exactness, source/registry wiring, compile and screen/startup regressions but does **not** prove physical audio callback deadlines, real-music long renders, audible quality, UI issue resolution, M17 near-ceiling final-export qualification or M24/FQ.

## Decision

If CI is green: keep batching **opt-in in staging preview** for the next PO Windows full-song comparison with original `P2_BATCHED_SAFETY_5MIN.tsv` standalone runner. Do not turn it into a release default or declare an AutoMatch SLA. If any factory or PCM/TP check fails, revert batch selector to off while preserving the unmodified original path.

Overall programme priority: **finish L1 M24 physical/evidence matrix** while continuing P2 integration, then *minimum* L2 handoff and rapid L3 AutoMatch. Do not initiate another speculative kernel profiling cycle without an identified large, measurable opportunity.
