# M18 — Product Mutation Coverage & Macro-Lot Admission Contract
Date: 2026-11-06
Status: PREPRODUCTION AUDIT, not merged, not FQ, not product activation.
Baseline: Focused #1052 Windows/Linux SUCCESS at 7b80452ec414cd3d5d781cd61d9a434420efa8aa.

## Purpose
The independent M18EpochCommitBarrier is useful but alone UNSAFE as authorization to publish a file from a background worker. It must be connected to all authoritative changes that could make an M17 Master, selected Output-Rate, source identity or consent stale. This checklist is an admission gate for the NEXT coherent macrolot; it is not evidence those changes have been implemented.

## Existing paths requiring explicit review / coverage
1. app/src/terminal_private_product_session.cpp::request_private_impl — starts a fresh Pending and calls renderer_->request_private(rate), superseding previous M17 assumptions; enforce epoch transition before any observable pending/renderer publication.
2. app/src/terminal_private_product_session.cpp::cancel — withdraws pending/master and calls publisher_->revoke_private and renderer_->invalidate. This can invalidate the owner while an M18 worker is encoding. A commit-held lease MUST deny/defer the invalidation before the first destructive operation. The UI must display BUSY without blocking the event loop.
3. app/src/terminal_private_product_session.cpp::cancel_pending_on_region_change — SOURCE-region signals call cancel synchronously; the lease must make that path fail/queue instead of silently revoking an active export.
4. app/src/terminal_private_product_session.cpp::failed and stale — both may clear Pending; their renderer-driven/error recovery branches must not retire an actively committed owner before readback.
5. app/src/terminal_private_product_session.cpp::on_renderer_changed — ERROR/STALE/MEASURED/READY transitions can invoke failed/stale/renderer invalidate/installs. Audit signal reentrancy, and ensure no callback bypasses the busy commit guard.
6. app/src/terminal_private_product_session.cpp::return_private_monitor_to_manual and retry_source_return_after_monitor_retirement — SOURCE/manual owner may be reinstalled, renderer invalidated and Pending cleared during monitor recovery. All transitions must respect held export owner identity without breaking existing M17 physical return guarantees.
7. app/src/terminal_private_product_session.cpp::install_measured_private / install_accepted_candidate, suspend_for_comparison / resume_comparison — potential stage and audible identity changes. Distinguish nonconforming measured PCM (never an authorized M18 source) from checked full M17 Master.
8. app/src/terminal_private_product_session.cpp::adopt_pending_source_loop_without_rerender — SOURCE loop can change without rerendering full PCM; determine if this invalidates Product export consent / source epoch, and enforce a single policy.
9. app/src/terminal_candidate_gate.*::invalidate/accept/staged — master candidate ownership can change externally, not only via the Product session. Audit every callsite and ensure an invalidation that affects export identity participates in the same generation.
10. Project-wide Source/Project selection, output Master rate, Gold/Prepared/manual owner, chain revision and file close/delete — inspect upstream App/Project controllers and the authoritative project/session state, not just this TerminalPrivateProductSession class. Without their cooperation, barrier is NOT a real Single-Writer Epoch lock.
11. app/src/m18_private_export_authority.cpp currently constructs a synchronous Qt-owning-thread QPointer+consent callback. DO NOT transfer this callback or access to QObject to worker threads. Introduce an immutable export ticket and independently revocable, thread-safe Product-issued authority; recheck at admission and before file commit. Consent revocation must affect unpublished candidates.
12. platform/windows/src/transactional_audio_exporter.cpp::export_impl — new M18 request must keep its existing identity/no-overwrite/rollback/postcommit readback contract. The worker-held lease must span the entire commit-and-readback region. Neither M16 TP mathematics, source SHA or M18 PCM codec may be silently bypassed.

## Next true MACRO-LOT — stage several cohesive sublots, then ONE CI
A. Authoritative epoch+consent snapshot and invariant tests: capture M17's exact owner, source/project identity, rate, revisions, and explicit export intent ON the Product thread; no QObject access on a worker.
B. Mutation interlocks: route every dangerous Product and upstream project mutation through an opt-in nonblocking gate; reject/defer changes while a commit lease is active. Verify ERROR/CANCEL/REGION/RETURN/REPREPARE paths and avoid Qt event-loop waits.
C. End-to-end worker handoff with unchanged canonical Windows exporter: stale/revoked ticket rejection, collision/rollback, lease lifetime across post-commit verification, deterministic destruction/cancellation.
D. Adversarial tests: concurrent worker leases, revoke-before-commit, SOURCE switch, project switch, rate change, M17 stage replacement, owner lifetime, and repeated cancel/return signal reentrancy. Keep existing Windows and Linux M16/M17 gates green.

Promote to the Draft PR only AFTER A–D is coherent, source preflight passes, and all six M16 B2i protected Git blobs match the exact baseline. ONE exact-SHA CAS, ONE Windows+Linux Focused CI for the macro-lot. Any intermediate staging commits must be no-PR/no-CI, and do not publish an unguarded path. This does not authorize main merge, Full Qualification or user-facing GUI.

## Concurrency policy
The M18EpochCommitBarrier mutex only protects the SHORT gate transitions; never hold the mutex during DSP, I/O or a Qt callback. A held permit forces a Product mutator to respond immediately BUSY or explicitly schedule a deferred transition. If Product cannot guarantee this over ALL paths, do not activate the asynchronous exporter. A committed destination may become visible before subsequent readback fails; rollback is mandatory while the permit is still held.

## Decision log
Focused #1050 checked 48 Windows filesystem exports; #1051 checked 14 injected rollback cases; #1052 checked one nonblocking barrier primitive. Those were valid individual qualification checkpoints but small in delivery scope. Going forward batch multi-component functionality/negative tests into a single coherent preproduction macro-lot and avoid a new CI for each small test/documentation change.

## Handoff #71 — sublot A1/A2 staged, UNCOMPILED, 06 Nov 2026
Single no-PR/no-CI staging (after #1052 green): commit chain
`72c73a0` → `73607bd0` → `711d18b9` → `76a3e380`.
Adds Qt-free `platform/windows/src/internal/m18_product_epoch_ticket.hpp`
and extends the existing Windows M18 delivery-validation QtTest target.
**No new CI target or workflow, no main/PR #84 move, no GUI/export activation.**

- `M18ProductExportIdentity` is a nonzero Source, Project, immutable
  M17 Master generation, processing-chain revision and one of six legal
  output MASTER rates. A ticket captures one exact identity and epoch
  after explicit consent. A copied ticket alone never grants a commit.
- `try_transition(expected,next,newIdentity,consent)` is the sole proposed
  Product-owner mutation/revocation gate. It and worker admission
  serialize consent+identity checks with the preexisting nonblocking
  `M18EpochCommitBarrier`; a held worker commit prevents any transition.
  A stale ticket cannot renew or commit after a successful transition.
  No mutex is held during WAV I/O, DSP or Qt callbacks.
- Worker-safe lease wrapper rechecks authority lifetime and consent
  immediately before publish. Destroying the tentative Product authority
  invalidates both retained tickets and an already-acquired worker permit.
  Any failed recheck must preserve transactional cleanup/rollback.
- Test sublot exercises missing consent, no-op epoch, consent grant,
  competing worker permits, SOURCE change and consent revoke during a
  held commit, source/rate identity mismatch, stale ticket after revoke,
  fresh opt-in and retirement of the authorizing owner. Joins the worker
  before QtTest assertions. These new assertions are **not yet CI proven**.

**UNRESOLVED / gate BLOCKED:** This is deliberately a disconnected
preproduction primitive. Source/Project/MASTER mutators still bypass it,
the real Product does NOT mint these generations, the ticket is not
bound to an independently checked M17 output owner or filesystem source,
and the Windows exporter/GUI has NO Product worker caller. Do not use
this contract to justify asynchronous M18 publication. Next coherent
sublots must wire authoritative mutations (including cancellation and
error/stale/return reentrancy) and fully adversarial end-to-end worker
handoff, with no F64/M16/M17 regressions; only then one immutable
exact-SHA PR promotion and one Windows/Linux Focused CI. No merge/FQ
without Product Owner permission.
