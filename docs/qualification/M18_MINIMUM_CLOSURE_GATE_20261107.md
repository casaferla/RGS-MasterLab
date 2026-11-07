# M18 — Minimum closure gate (PO schedule protection)
2026-11-07 · RGS MasterLab · L1-M18 Dither / final WAV delivery
Status: staged planning guard, NOT evidence of release. #1088 GREEN both Windows/Linux at 798bd8865408d669a08ef74273f816378019f9a2. PR #84 stays Draft; main unchanged.

## Scope freeze: finish the one real User Export journey
Use the existing M17 verified MASTER candidate, the already written deterministic PCM24 TPDF / undithered IEEE-F32 transactional exporter, and the existing Product Single-Writer epoch protection. Do NOT rebuild dither, DSP, M16, M17 or add more independent defensive layers unless a failing end-to-end test demonstrates a concrete gap.

### Acceptance A — real positive Product export (blocks M18)
A native Windows Qt integration fixture must use the ACTUAL product controller group (ProjectSession, selected SOURCE, supported MASTER rate, MANUAL chain, M17 limiter/candidate, complete Product binding), prepare/accept an M16-checked M17 full-resource MASTER, acquire a fresh explicit Product-issued consent ticket, then export BOTH PCM24 seeded TPDF and IEEE-F32 no-dither by the single private Product Export action.
Check original SOURCE and project remain unchanged; two create-new WAV files are readback-verified (format, frames, Fs/channels and encoded SHA), result phases reach VERIFIED_DELIVERED, ACK retires consent, and another export requires a new user confirmation. The existing M17 candidate→worker positive tests can be REUSED; do not invent synthetic Product success.

### Acceptance B — lifecycle negative and rollback (blocks M18)
In the same Product fixture: invalid destination, SOURCE/path alias, unsupported format/seed, stale SOURCE/project/rate/chain/limiter, collision, cancellation during nonblocking export, any worker-held commit, and Return-to-Source/shutdown must not corrupt or overwrite a file, lose a success result, retain a re-usable ticket or block Qt. Assert Product once-per-mutation epoch logic; reuse existing focused tests where sufficient instead of writing duplicate tests.

### Acceptance C — minimal user-visible GUI (blocks M18)
Only after A and B pass, register the ONE Product-owned Export action and its status in app/main. UI needs a simple explicit output choice (PCM24 or float32), destination selection, Export / Cancel / Acknowledge, verified-delivery state, and error. Use the accepted M17 Ceiling read-only (do not invent a second threshold or sonic edit). Do not block the UI during encoding or final verification. Keep 100% encoded distinct from successful delivery. Any extra GUI polish/performance improvements belong to subsequent UX work, not M18 close.

### Acceptance D — final Focused and PO Windows smoke (blocks PR #84 merge)
Run serial Windows+Linux Focused CI only on coherent 15–25-commit macro-lots (or smaller justified fixes); no concurrent CI, no full logs, only bounded artifacts. Confirm the actual release-candidate SHA green and original M16/M17 tests unchanged. Obtain PO Windows physical smoke: real 44.1/48/192 kHz SOURCE as available, PCM24 & float32 export, Cancel/ACK, collision. PR #84 remains Draft and never merges to main without PO authorization.
Per PO instruction: ONE combined Full Qualification only AFTER integrating M18–M22; do not run individual M18 FQ.

## Why so many prior cycles / controlling the remaining work
Most previous rounds guarded separated components against background export races. That substrate is already present. The remaining work is the orchestration and one positive real Product test, not another DSP implementation. Stop counting commits as delivered features; treat completion of A, B, C and D as milestones. Target planning: ~2–4 coherent integration macro-lots plus focused repairs and PO smoke; contingency remains unknown until A is green. Do not claim a fixed calendar date or a guaranteed number of CI runs.
