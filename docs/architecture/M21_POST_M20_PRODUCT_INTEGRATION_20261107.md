# L1-M21 — real M19/M20 metrics port, PC-free staging
Date: 2026-11-07

## Predecessor boundaries
M18 PR #84: focused #1094 dual-OS PASS, Windows physical PO
acceptance DEFERRED, no main merge.
M19 new Draft PR #86: public whole-scope five-metric M16 bridge,
independent Windows/Linux Product CI PASS.
M20 new Draft PR #87: bounded FFT/PSD/stereo analyzer, independent
Windows/Linux Product focused CI #2 PASS at `fb97f884`.
This branch is isolated on the M20 candidate and remains **NO MERGE**.
M21 private PR #70 and prior M18–M20 heads remain unchanged.

## Implementation
- Ported exactly 18 original M21-owned blobs from private PR #70
  (`0a659900`) without importing historical parent ancestry.
- Added native C++ typed adapter
  `app/src/internal/m21_real_product_metrics_port.hpp` consuming
  **REAL** public `BasicLevelAnalysisResult` and
  `TonalStereoAnalysisResult` rather than fabricated string values.
- Exact scope begin/count, frame domain, sample rate and layout must
  agree; absent owner/generation evidence is a hard refusal.
- Exactly PREPARED/GOLD/PROCESSED; RAW is never substituted. Both sets
  of scalar deltas reuse the existing M21 unrounded **Gold - Current**
  comparator. GOLD has independent provenance and generation.
- Unqualified numeric M16 TP remains explicitly missing (never
  silently elevated to PRESENT or displayed as 0.0).
- Mono stereo metrics remain NOT_APPLICABLE. Silent/insufficient/
  undefined statuses propagate without artificial values.
- Actual M20 PSD bins retain canonical M20 rate/FFT geometry.
  Linear-power normalized fractions are **not** shape dB; this port
  does not invent tonal shape values, common comparison bandwidth or
  Gold synchronization.
- QtTest checks real mono source, Gold +6 dB level difference,
  absent TP, stale generation, absent independent owner and mismatch.
- Independent Windows/Linux focused M21 workflow builds public
  M19/M20 analyzers and M21 real typed port, plus M21 private guards.
  This is NOT M21 Gate S/M, FQ, Qt UI visual acceptance or main merge.

## Still to close
Actual Product PREPARED/GOLD/PROCESSED immutable PCM owners,
authoritative Phase-6 generation tokens and async invalidation,
canonical FeatureDefinitionRegistry/status and audited per-band shape
comparison; Qt/QML binding and PO visual gate. GUI approximation is
permitted while no Windows workstation is available. Listening/FQ
will be consolidated later, with explicit PO merge approval.

**STATUS: backend integration staging; no canonical release claim.**
