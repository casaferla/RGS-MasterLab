# L3 AutoMatch / P2 — performance and placement integration audit (2026-11-22)

Status: writer-only audit. This is **not** a changed frozen L3 contract, a P2 eligibility decision, a Product M13 acceptance, or release qualification. Authoritative PO 2026-11-13 priority decision and 2026-11-10 dual-stage research direction prevail.

## Evidence grounded in the current writer

- PO target: a useful verified 5-minute offline AutoMatch in ~20–30 minutes is acceptable, **not yet proven**. M10 has a frozen maximum of 800 whole-resource render/analysis attempts; 800 is a cap, not a mandatory iteration count. No implicit early stop, altered solver policy, omitted analysis, or approximate replacement for a scored candidate.
- Physical Windows research on 300 s stereo 48 kHz: 100 fresh full five-module/P2-active Product render previews summed to 1,137.224 s (~18m57s), mean 11.372 s per candidate. This excludes canonical 27-feature analysis, L3 solver, M13 verification, and UI overhead. Standalone P2 accounted for ~63–65% of full chain cost in the associated isolated marginal study; these are research measurements, not an end-to-end L3 SLA.
- P2 batched callback: opt-in 64-frame safety quanta, benchmarked against exact PCM and EOS oracle. It improves a researched implementation path, not a substitute for actual physical stress of expensive fallback/projection paths.
- Product `MasteringPreviewController`: worker-only, PCM-bit-exact cold-oracle verified four-module prefix LRU, bounded to 256 MiB. Eligibility is exactly Gain/EQ/Compressor/Stereo M-S/P2 and requires Compressor **and** Stereo M-S bypassed. It does not establish general stateful-stage checkpointing or automatically serve the L3 M10 evaluator. A full 300 s, 48 kHz, stereo float64 prefix uses 230.4 MB (~219.7 MiB), so the limit holds at most one such exact prefix.
- L3 M10 `ExactCompletedEvaluationCache` caches only complete authoritative scores for **identical** execution keys, not intermediate PCM or a changed candidate; its adapter explicitly does not implement the Product worker. M09 proxy can order/preselect proposals but must not be passed off as authoritative M10 render/analysis.

## Architecture gap requiring explicit closure

- The current P2 Product preview is appended as fifth **MANUAL** processor. M12 explicitly prohibits P2 as an auto-created REF_LINKED node. M13 `probe_fresh_m13_full_native_master_once` processes **PREPARED -> protected MANUAL -> REF_LINKED**. Thus an active MANUAL P2 followed by active REF_LINKED artistic nodes would execute P2 **before** AutoMatch corrections, not as the final musical limiter. An independent M16 True Peak check afterwards is necessary but does not prove P2 was in the intended musical signal position.
- Before claiming complete M13 verification with P2 active, implement and test a single product-authorized logical graph preserving all MANUAL settings and user ownership while evaluating artistic corrections before the streaming limiter, with exact DSP order, latency/EOS and cache-key identity. Do not silently reorder persisted nodes, rewrite frozen L3 semantics or conflate P2 with M17.
- Original PO decision deliberately leaves open whether AutoMatch adjusts P2: compare **A** P2 Ceiling/Drive eligible under an explicit later contract vs **B** P2 excluded from automatic coordinates (remaining manually controllable). Require comparable real-music proximity, pumping/GR, TP and full-run CPU evidence before choosing. No automatic claim that A is always superior.

## Narrow implementation sequence and gates (no speculative full rewrite)

1. Complete current M12/M13 build repairs and the next coherent focused macro gate; retain single writer and writer-vs-CI separation.
2. Make the logical audio order explicit, exercise active P2+active REF_LINKED candidates at 44.1/48/96k, bypass/parameters and tails; compare one-pass canonical output to staged execution exactly. M16 remains actual pre-export PCM safety analyzer. M17 stays independent WAV Export and is not an M10 DSP coordinate.
3. Attach exact Product-owned M10 candidate evaluation to existing in-memory whole-song render and all canonical feature measurements, not GUI and not M17. Analyze PREPARED and REFERENCE only when dependency changes. Preserve 800 cap/policy and independent fresh M13 verification.
4. Reuse verified immutable stage-prefix caching **where eligible** across coordinate sweeps (with full stage state, timebase, latency/EOS, sidecars), budget RAM, invalidate strictly on upstream mutation. Never reuse a P2 result when its input PCM or configuration changes. Exact completed-score cache hits cost no evaluation attempt.
5. Measure 20/50/100 completed **real** L3 evaluations, with/without P2 as candidate coordinate, cache-hit ratio, elapsed render/analysis/solver/verification, peak RAM, quality and fallbacks on PO Windows. Only then assess the 20–30 minute user objective or propose an explicit revised solver policy.

Source references:
- https://docs.google.com/document/d/1O_1nXiROO-vAA8w0eOk7DP3wysx5bIG80XONJZwtGKY/edit
- https://docs.google.com/document/d/1iYhmqTtwuSdEwZ25pv8Cgp3eGs_C2rD2p3hd5nTBHM4/edit
- https://docs.google.com/document/d/1zong-l4lYDpVU-nHR97r08GrI4tdHMhztYbp_p6v7yA/edit
