# M18 final integration exit gate — RGS MasterLab
Date: 2026-11-07
Status: preproduction; NOT a release, NOT Product export activation.
Last verified Focused baseline: #1082, Windows/Linux SUCCESS, SHA 1496ca31147da8743bd67769aca43c308df010f4.
Current immutable candidate: #1083, SHA e40fc6651e78fb214c946596f47963405321d4eb.
Scope: M18 final PCM24 TPDF/IEEE-F32 WAV export and one owner-bound Product epoch.

## Implemented and actually exercised
- Canonical Windows transactional WAV output, PCM24 deterministic seeded TPDF, IEEE-F32 no dither; same M16 postquant verification/readback, SHA-256 and no-overwrite.
- Detached Qt-free worker, cooperative cancellation, exactly one export in the Product-owned async session, terminal result requiring acknowledgement; encoding progress is distinct from verified file delivery.
- Immutable M17 terminal candidate retention, actual SOURCE physical-file provenance, MASTER output rate & rate revision, Product ticket capture and worker commit lease.
- Real Project, Source, GOLD, Audition, Region, limiter and Master rate controllers have opt-in owner-thread mutation guards, some using whole-operation physical writer batches.
- Private UI progress ViewModel exists but is *not instantiated or registered by main.cpp*.
- The #1083 candidate newly checks output format before starting work and presents verified SHA/byte evidence only after readback.

## Critical blockers — MUST close before activating Product export
1. **One authoritative binding.** Provide a single Product-owned installation and verified rollback for ALL gates on the same UI owner thread: SOURCE selection/checked committed callback; Project Open/SaveAs; MASTER rate/source revisions; GOLD/Region; audition/processed selection; manual chain; terminal limiter & candidate; M17 Product session/render; file provenance, and explicit consent. Independent per-controller tests do NOT prove system-wide composition.
2. **One transaction, not nested epoch burns.** A Product Prepare, source commit or M17 accept/cancel pathway must acquire a correctly scoped epoch/physical batch before any observable write and must not reenter separately with a second independently consumable Product generation. Double-admission and status-ignoring legacy `void` callbacks remain a block until their actual call graph is reconciled.
3. **Only one verified-current export owner.** Ticket must bind one exact accepted M17 MASTER and current SOURCE/Project/chain/rate/limiter/consent, and must be withdrawn when any relevant upstream mutation is actually accepted. A failed preflight or no-op may not needlessly revoke unrelated valid consent. No synthetic or self-issued tickets in the shipping path.
4. **Real Product UI.** Explicit format choice, consent, output destination and progress/cancel/acknowledge must only call the above authorized path. No unsafe QML direct worker entry and no GUI thread wait for DSP/IO.
5. **System-level qualification.** One native integrated scenario on Windows covering authorized PCM24/F32 full WAV, user cancel midstream, stale SOURCE/Project/rate/chain/candidate, attempted mutation during commit, readback failure, collision and shutdown/return-to-source. Keep protected M16 and M17 behavior unchanged. Focused CI per 15–25-commit macro-lot; combined full qualification after integrated M18–M22, per PO instruction.

## Operating policy
- No main merge without the Product Owner's explicit authorization.
- A green Focused run proves only that exact SHA's registered tests; it is not evidence the above Product wiring is finished.
- Do not grow repetitive defensive test variations without removing an exit blocker. Avoid claiming M18 closed solely from CI count.
- While CI #1083 runs, keep all next changes on `staging/m18-post-m17-reconciliation-no-ci-20261105`. Do not move PR #84 or create another CI until both jobs are terminal and PO reports verdict.
