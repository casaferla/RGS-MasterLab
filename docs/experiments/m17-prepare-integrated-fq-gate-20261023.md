# RGS MasterLab — M17 First Physical Prepare Test Gate · 23 Oct 2026

STATUS: EXPERIMENTAL / NOT QUALIFIED / NO MAIN MERGE.

Mandate: PO requests first real Windows build and FQ as soon as technically safe. Stop speculative micro-optimization. Windows is primary.

Provenance: parent product PR54 SHA a92084aabf85131f205e8708b819299b15d03616. Isolated research PR55 SHA 8dd24c807a16698c3c1e8714990b1b4a3080f83e, CI #704 PASS both. This branch is independently isolated; PR54 and main stay unchanged.

Implementation: import detector feed, rolling lookahead selector and exact cache from research prototype PR55 as opt-in DSP offline candidate; preserve frozen IModule, genuine registry/factory, typed ceiling, D=H+8 witness and source ownership. Select acceleration **only** when the explicit full-resource asynchronous M17 Prepare worker invokes the existing output-rate candidate. Preserve original renderer for all other callers and regression oracle. All PCM remains private until authoritative full-resource M16 measure, optional global trim and re-measure on verified same OUTPUT_RATE. No speculative prebuffer/Play prefix release. Cancel and progress checked at bounded batch boundaries.

Required gates before PO-operated physical Windows FQ:
- PR Focused Windows and Linux green on integration SHA.
- Same-source six-Fs mono/stereo PCM and applied gain witness binary64 identity, -1 and -12 dBTP.
- M16 final check status/peak/trim identical; negative cancellation/stale ownership and bypass regressions.
- Compiler strict FP and no 64-frame realtime claim.
- On physical Windows, measure 20s WAV at 44.1/48/192 kHz, and whole song at 192k; compare original-vs-accelerated Prepare times with phase telemetry, verify M16 TP, GR history, UI Minor Fix and source/loop semantics. Record specific 44.1k -12dBTP prior failure as OPEN until physically retested.
- Product acceptance, physical Gate M/L and FQ triggered by PO. No merge or public 'QUALIFIED' status automatically.

This is not yet physical evidence. Tests must fail closed if accelerated output differs from frozen reference.
