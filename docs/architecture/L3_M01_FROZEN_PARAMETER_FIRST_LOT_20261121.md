# L3-M01 — bounded implementation lot 1, frozen parameter authority

2026-11-21. Baseline `main` 478eb806 (L1 + minimal L2 + L3 research evaluation). Prior L3 Gain work on d3db0057 stays isolated, not an AutoMatch input-Gain authority.

Read authority: L3-M01 Product Owner closure ACCEPTED/FROZEN v1.0, 2026-10-06
https://docs.google.com/document/d/1FcGbdfwQ6MzZWB483MPiVHGlM3y53x2T5Ac-5lnMAQw/edit

This lot implements exactly the first necessary production DSP layer: P01/P02 typed 1–6 ordered stable UUID bands, filter discrimination BELL/LOW_SHELF/HIGH_SHELF (no unsupported NOTCH/HP/LP), EqRouting reuse, default one enabled BELL 1000 Hz/Q0.707, numerical controls and structural P04 endpoint law, sample-rate 0.45 Fs and mono route admission. All invalid values reject, no clamping. DSP module type/schema identity remains frozen `rgsml.dsp.dynamic-eq` / `rgsml.dsp.dynamic-eq.parameters/1.0.0` (reserved for registry implementation).

**Explicitly not implemented yet:** C01 per-frame time-varying binary64 TDF-II audio execution, detector BPF/LP/HP, per-lane RMS/linking, effective-memory certificate, state transactional rollback/checkpoint, module factory, ModuleParameterPayload/codec, dynamic EQ editor, L3-M02 qualification. No claim of completed M01 or full AutoMatch.

Next development work is the *actual* accepted C01 audio module on this branch, then focused native conformance and listening, no parallel alternate implementation. Inherited L1 audio paths must not be changed during this parameter-only lot.

L2 → L3 reconciliation: current M18 project Save As only uses `ProjectRepository::save_create_new`, not L2-M02 Save/Update transaction. L1 preview contains P2-only 256 MiB prefix LRU, not full solver L2-M04 cache. M19–M21 descriptive comparisons have 9 scalar M21 metrics, not 27 solver-authoritative L3-M05/M06 features; this gap is deferred to the M05/M06 implementation, not filled with fabricated weights. Preview/full parity, loop/audition and Product epoch guards exist in L1 and are reused. Do not declare L2 Artist Alpha acceptance or claim full M02/M03/M04/M06/M07/M10 status.
