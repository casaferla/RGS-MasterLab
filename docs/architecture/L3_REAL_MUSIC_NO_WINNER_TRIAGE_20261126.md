# L3 Product real-track failure triage — 2026-11-26

Source: Windows physical AutoMatch search on a real 44.1 kHz stereo source, 3:19.184 in the application. Screenshot shows Total 80%, Tonal 90%, Dynamics 85%, Stereo 95%, Loudness 90%, independent AutoMatch TP safety ceiling -1 dBTP. The screenshot is evidence of **one real Product search failure**, not proof that the underlying source/reference pair is unmatchable.

Exact UI error: `L3_PRODUCT_SEARCH_NO_REVIEWABLE_WINNER: worker=4 search=3 fixed=2`.

Native enum mapping from source:
- `NativeAutoMatchWorkerPhase::search_completed = 4` — asynchronous worker completed, not cancelled.
- `NativeProductSearchStatus::no_measured_candidate = 3` — no measured candidate winner.
- `NativeFixedSearchStatus::correction_required = 2` — Fixed measurement requested correction; not a no-correction path.

No M12 adoption was authorized; MANUAL and prior REF_LINKED remain untouched. Do not label this a GUI hang, timeout or an M13 verification failure.

## Mandatory next diagnostic

Product UI must show the retained worker's `RefinementBatchReason`, actual attempted count, `NativeSearchPerformance` completed/failed/cache-hit/preflight/budget counts and native original/fixed/seed/search timings on failure. In particular distinguish:
- `no_complete_seed_or_stale_source` with all preflight rejections;
- real render failures versus candidate safety rejection;
- `seed_attempt_budget_exceeded` / `budget_exhausted`;
- `invalid_inventory` / `invalid_evaluation` / `invalid_seed_trial`;
- a genuinely completed candidate not reconstructed due to identity checks.

Never weaken M11/M16 hard safety or broaden solver budgets without a reproducible failure class and CI evidence. Do not reinterpret the -1 dBTP limit as the terminal M17 Export TP Limiter setting. The original real WAVs and independent reference are required to reproduce this exact numerical failure; the screenshot alone does not contain the audio or native trial ledger.

## Qualification gate

1. Windows Product cold smoke and exact build SHA; no QML errors.
2. Negative test with a valid Fixed correction request but no measured winner: status is SEARCH FAILED, not CANDIDATE READY; Apply disabled, no M12 mutation, diagnostics visible.
3. A positive six-rate synthetic fixture must still yield the exact native M12/M13 evidence. This is necessary but not sufficient for real music.
4. Run a full-length independent source/reference music benchmark, capture actual trial ledger and first-admissible latency, and review whether no-winner is a deterministic solver issue, a legitimate hard safety outcome or a candidate topology failure.
5. Physical Windows test on the actual reported source/reference pair before declaring the user-observed issue fixed.
