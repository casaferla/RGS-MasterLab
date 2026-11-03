# M17 FQ96 — Windows blockers, exact status and guarded follow-up (2026-11-03)

Scope: Product Owner Windows screenshots (source 48 kHz / 16-bit stereo, 3:03), Project MASTER 48 vs 88.2 kHz, Return to Source, Preview Anyway, GR HISTORY, Rate & Time and ETA. This note does **not** constitute an FQ, Gate G closure, physical DEVICE qualification, or permission to merge main.

## Proven baseline
- PR #61 (Draft, base main) exact base SHA `a589719060d6ef92dec97d16ecb04d78ed75f22e`: Focused #41717904470 Windows + Linux SUCCESS. Baseline ETA uses actual SRC/M17 progress and *provisional time-based*, indeterminate M16 forecast; do not claim an M16 processed-frame percentage.
- FQ96 code macro-lot originated on no-CI staging `staging/m17-fq96-blockers-no-ci-20261103` and was fast-forwarded to PR #61 SHA `250f41b1d7e64393dd3d310238a645a984230a68`. Associated Focused run #41721017928 must be terminal and dual-OS PASS before the next promotion.
- The later Return diagnostic commit stays ONLY on no-CI staging until that run is terminal; PR #61 and main are not implicitly updated by a staging commit.
- Do not merge PR #61 into main without explicit PO GO, authentic FQ and Windows physical gate. M18–M21 remain blocked behind predecessor qualification and post-M17 CI reset.

## Findings and implemented behavior

### GR HISTORY visibility
Before FQ96 the single lower bay incorrectly displayed PREPARATION for idle `NEEDS PREPARATION`, `BYPASS` and `ERROR` statuses. It now displays PREPARATION only for an actual offline M17 Prepare or active MASTER->DEVICE conversion; otherwise it shows GR HISTORY chrome. The graph is intentionally empty unless `TerminalPrivateGrViewModel` can independently authenticate both the applied gain sidecar and exact sink-consumed OUTPUT_RATE chronology. A Return-to-Source cannot continue accumulating M17 consumption. No fake zero trace, inferred monitor SRC witness, or reuse of stale realization IDs. Qt layout smoke exercises NEEDS, Preparing, READY, MONITOR, ERROR, BYPASS.

### Nonconforming cross-rate Preview Anyway
A measured NON-CONFORMING M17 output has *no* authorization to borrow the independent verified-MASTER monitoring SRC route. Existing direct terminal install requires exact master/device frequency, but Windows errors occurred after stopping/retiring the current source sink, cascading to ERROR/BYPASS. FQ96 introduces:
- `current_measured_matches_device_for_preview()`: actual fresh endpoint preferred exact-rate probe, current measured candidate, exact source owner, double candidate identity;
- advisory `nonconformingDirectPreviewAvailable` to disable the impossible button in the UI;
- click-time independent guard before any source STOP/DEVICE installation. A rejected request retains the nonconforming candidate and MANUAL audio, reports a diagnostic and does not change bypass.
A verified MASTER at another rate uses Listen to Master and independently preflighted DEVICE PCM; Preview Anyway is never made into a verified audition by this fix.

### Rate & Time cannot silently invalidate Prepare
The popup `onOpened` only calls the read-only advisory refresh; the only explicit rate mutation is six-rate `choose_master_rate`. The native Qt rate popup smoke now checks that open/close neither increments `rate_revision` nor emits `MasterOutputRateState::changed`. A genuine changed MASTER rate must still revoke a previous render epoch (even A->B->A), never silently reuse stale PCM. If the physical refusal returns on opening alone, capture sequence, current/selected MASTER rate, and rate revision around that operation before changing these gates.

### Intermittent MONITOR_RETURN_MANUAL_SINK_NOT_EXACT
A Windows Return-to-Source image reports a failed exact MANUAL reinsertion proof. The strict check is necessary; do not loosen state/phase/ID/duration/position/loop equality or retry against an alien sink. A later staging-only diagnostic categorizes which exact predicate refused: snapshot, stopped state, handoff phase, realization, duration, cue position, or loop. Keep the top-level error token for tracking. A new physical repro is required for a functional fix. Do not claim this symptom is already repaired.

### ETA M16 protected numerical boundary
Final SRC and M17 expose real post-block fractions. M16 primary and secondary call the frozen `measure_true_peak_full_resource` atomically: its internal multi-chunk implementation is SHA/blob-locked by M16 B2i Gate G, particularly `analysis/src/true_peak_full_resource.cpp` and its internal header. The existing FQ95 ETA for M16 is an *explicit provisional wall-time forecast* with honest indeterminate progress; this is not true numerical progress. Instrumenting the frozen source directly fails Gate G and is NOT authorized merely for UI. Before showing real M16 fraction, obtain a separately reviewed/approved observer seam with bit-identical six-rate proof, correct versioning and lock reconciliation. No fabricated percentage.

## Required next Windows acceptance cases (same 48 kHz physical sink)
1. 48->48 Ceiling known-conforming: Prepare succeeds; direct Preview Prepared, L/R authentic GR, Return to Source, empty chrome (no stale gain curve); no new full Prepare.
2. 48->88.2 Ceiling known-conforming: MASTER remains 88.2, Listen to Master converts DEVICE to physically observed 48 without lying about M16 LIVE/GR; Return to Source preserves verified PCM and valid master epoch.
3. 48->88.2 Ceiling non-conforming: measured evidence remains separate; Preview Anyway button unavailable, no source-sink retirement, no ERROR/BYPASS; Rate & Time opens/closes without changing source/master revision; explicit change of selected rate invalidates only when intended.
4. 48->48 Ceiling non-conforming with direct DEVICE support: explicit Preview Anyway only, no forged verified status, no M16 LIVE certificate.
5. Return-to-Source forced mismatch: exact refusal and bounded categorical diagnostic, never silently promote wrong owner; test each category with native fake sink if reproducer becomes available.
6. All six SOURCE/MASTER rates, 16 FIRs in one Windows artifact, physical 48 kHz / 16-bit sink with 44.1/48/88.2/96/176.4/192 target; error state recovery, 4K+FHD scaling, unrounded Ceiling gate, no merge until PO.

This is an operational triage document, not a substitute for terminal Focused + FQ logs, source-sha attestation, Windows physical audio evidence or a Product Owner release decision.
