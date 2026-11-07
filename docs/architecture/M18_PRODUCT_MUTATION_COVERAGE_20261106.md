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

## Chat #71 macrolot B+C+D — isolated candidate before Focused CI
Following the earlier Qt-free ticket component A, the isolated staging adds
`app/src/terminal_private_product_session.hpp/.cpp` with an OPTIONAL
`set_private_m18_mutation_admission()` callback and a fail-closed Qt-owning-
thread check. Intercepts 12 local transition paths:
request_private_impl, cancel, failed, stale, renderer status callback,
return_monitor / retry_return, suspend/resume comparison, adopt_source_loop,
and accepted/measured installs. In the absence of this callback, all M17
behavior is unchanged, and no QML route installs it. The callback is
purposely NOT claimed to protect external Source/Project/UI changes.

Tests added in already-registered Focused binaries:
* The real M17 Product activation native test attempts cancel and source-
  loop adoption while the synthetic admission denies all mutations: both
  must be refused and exact checked M17 stage preserved; then gate disabled
  for remaining original M17 tests.
* The existing M18 Windows candidate-delivery test executes actual PCM24
  44.1k mono odd-frame `TransactionalAudioExporter::export_new_file` on
  a real std::thread with an immutable request + Qt-free Product epoch
  ticket. It requires destination/readback SHA metadata and zero tmp.
  It then changes simulated Project generation, revokes consent and
  attempts an identical new worker publication with the OLD ticket:
  commit must fail and candidate temp must roll back. Original source
  bytes unchanged; already-finalized prior output remains intact.
* The ticket contract's earlier consent, Source/rate change, concurrency,
  stale and Product destruction tests remain enabled.

**IMPORTANT COVERAGE LIMIT:** This new callback is LOCAL and INERT.
Actual upstream Source/Project/master-rate/chain mutators, candidate-gate
invalidation, and any other bypass are NOT yet bound to its epoch.
No GUI export, production async worker, user file output schema or FQ
is authorized. Even after a green Focused candidate this restriction
remains. Before real integration, gate ALL upstream mutations before
publishing any observable change; maintain the exact M17 stage and
physical source identity, and define deterministic cancellation/retire.
No changes to M16 B2i protected blobs or legacy F64 codec.

The new sublots are a coherent **preproduction candidate** for one
Windows+Linux Focused native CI; they are not independently qualified
until that run returns terminal. Never promote during an existing CI,
never merge to main or start M18 FQ without Product Owner authorization.

## Follow-up staging after Focused #1055 Windows compile red
- Focused #1055 PR SHA `ea0f5bbc9a894ae6a1bbf19e4a3e9c76c69e51ff` Windows compile failure: `tests/unit/app/test_terminal_product_activation.cpp` referenced undeclared `vm` at C2065; not a discovered DSP/codec failure. Staging fix `8db2b240` constructs a real `TerminalLimiterViewModel` bound to existing test limiter state, then gates/rechecks that object. Linux may still be running; do NOT mutate PR or start CI while any job remains active.
- In same no-CI staging, `app/src/internal/m18_checked_stage_worker_bridge.hpp` captures an exact shared checked M17 stage and a typed Qt-free `M18ProductEpochTicketAuthority::Ticket`. Checks actual manual chain revision and MASTER-rate binding, original manual realized ID, Prepared/Manual source ownership, active post-M16 final_check, and retains the exact AudioBuffer through aliasing shared_ptr without audio copying or QObject access. Uses worker ticket's revocable commit lease factory, never the former synchronous QPointer callback.
- Real M17 product activation test verifies zero-copy PCM pointer equality for the new bridge, rejects a worker ticket whose output rate matches DAC but not selected MASTER, and still tests four concurrent owner-side mutation denials with the same held lease.
- IMPORTANT: Typed stage bridge remains a PREPRODUCTION function, not a Product-issued live user export action. It cannot independently prove a current real filesystem SOURCE, Project epoch, or all upstream mutators. The matching Product current owner check and single-writer mutation wiring are prerequisites for any UI/worker enablement.

## Chat #71 — staging after #1056 red / #1057 launched, NEXT macrolot unqualified
The Product Owner is the sole observer/reporter of CI results from now on.
The #1056 Windows failure is a STRICT MSVC warning-as-error
C4834/C2220 from two unchecked `SourceSelectionViewModel::adopt_probed_source`
returns in the original source metadata smoke; staged
`2df7fc082b91a28e9a125e787a7d57dc980346d7` fixes BOTH via
`QVERIFY(Status)`. The PR #84 head was serially advanced to this exact
SHA and triggered [Focused #1057](https://github.com/casaferla/RGS-MasterLab/actions/runs/42246730038).
NO polling of #1057 or PR mutation until the Product Owner reports
both terminal platform results.

Following **staging only**, one cohesive additional mutation-coverage lot:
1. `MasteringChainState` now owns an optional, runtime-only M18 gate
   that admits/rejects all FIVE canonical typed changes (Gain/EQ/Compressor/
   Stereo-M/S parameters and generic bypass) BEFORE changing DSP state.
   No callback exists by default in any Product instance; original M11-15
   processing and tests remain unchanged. Guard exceptions deny closed.
   The runtime guard is excluded from semantic `operator==` so it never
   becomes project/audio identity. The existing native mastering-chain
   test suite adds explicit five-way BUSY denial, unchanged old parameters,
   same chain revision, and explicit opt-in removal to restore normal edits.
2. `TerminalCandidateGate::try_invalidate_for_m18` is a NEW optional
   status-returning guarded path. A refused worker-held epoch must leave
   the original checked M17 stage and ticket untouched; the existing
   M17 `void invalidate() noexcept` is deliberately unchanged.
   The real M17 Product test now simulates one held M18 lease denying
   FIVE distinct mutations (Terminal cancel, Selector target, MASTER
   rate, M17 limiter BYPASS, and the new candidate invalidation).
   This does NOT mean the Product uses the guarded candidate path yet.

**HARD BLOCKER, NOT PRODUCTION READY:** legacy `TerminalCandidateGate::invalidate`,
several VIEWMODEL draft/undo mutations, and cross-controller Source/Project
rebinding STILL bypass the new runtime epoch. The actual Product has not
installed the gate, no M18 worker GUI/export job is activated, no storage
schema is approved and no FQ/merge is authorized. Do not claim protection
until every mutator is migrated and a cold Windows FQ plus PO physical
acceptance are completed. These next-lot changes have NOT been compiled
or run in #1057 and require their own later exact-SHA Focused CI.

### Subsequent staging sublot — preserve Product gate on Project restore
The original Project-open DSP replacement assigned a freshly deserialized
`MasteringChainState` with the default NULL runtime gate, thereby *silently
erasing* any opt-in Product M18 gate installed on the previous state. The
`ProjectSessionViewModel` now calls
`MasteringChainState::adopt_chain_after_authorized_project_open`,
which preserves the pre-existing Product runtime guard when replacing the
chain AFTER the previously required Project-open preflight admission.
The native mastering-chain test verifies the gate still denies a subsequent
Gain edit after a successful whole-chain replacement. This only fixes
lifetime/policy retention; it does **not** automatically protect a direct
MasteringChainState assignment by unrelated callers, nor the nested
Source/Gold/MASTER mutators during Project-open. No GUI activation and
no new CI for this individual staging sublot.

## Chat #71 — #1057 Windows red, target-specific fix on staging (PO checks CI)
Product Owner reported Windows RED on [Focused #1057](https://github.com/casaferla/RGS-MasterLab/actions/runs/42246730038), immutable PR #84 SHA `2df7fc082b91a28e9a125e787a7d57dc980346d7`.
We read ONLY the reported Windows job diagnostics; no recurring CI polling.

**Blocking Focused CTest:** `rgsml_terminal_product_activation_tests` failed exactly at
`privateMaster192kOn48kDeviceRemainsUninstalled`, line 3158,
`sharedEpoch.try_transition(777U,778U,verifiedId,true)`.
The new M18 `M18ProductExportIdentity::valid` incorrectly required
`chain_revision != 0`, while the real freshly constructed M17/manual
`ProcessingChain::revision()` can be zero. Similarly the new typed worker
bridge incorrectly rejected `stage->rendered.manual_chain_revision == 0`.
SOURCE/PROJECT/MASTER generations and the ticket epoch are deliberately
nonzero and STILL checked. An exact chain-revision equality, including
legitimate zero, remains mandatory. This is a preproduction M18 policy
bug, NOT evidence of an M17 limiter or DSP-math regression.

**STAGING FIX (no CI yet):**
- `f6b550a`: Product ticket allows a zero starting DSP chain revision.
- `b720d13`: M17 checked-stage to M18 worker request still pins EXACT
  revision but no longer marks zero as unowned.
- `de955b9`: real M17 checked-MASTER test confirms valid exact identity and
  rejects a different chain revision even at the correct MASTER sample rate.
- `2a2854a`: Qt-free worker ticket negative/positive test uses revision
  zero to prevent regression.
No original M17/M16 DSP, protected B2i blobs, FIR assets, codec or M18
PCM format are altered.

An additional standalone direct playback-SRC diagnostic in the Windows
job reported `PLAYBACK_FIR_RESOURCE_ROOT_NOT_CONFIGURED` from
`test_playback_support`; the CTest run itself passed other test targets.
The direct diagnostic lacks the CTest-injected
`RGSML_PLAYBACK_FIR_ROOT` environment; this is recorded separately,
not silently reclassified as a DSP failure nor used to weaken any
qualification gate. Do NOT promote or start another CI until Product
Owner communicates both platforms' #1057 terminal results.

The staging also contains the uncompiled central mastering-chain
mutation gate and guarded candidate invalidation covered in prior audit
entries. They will travel together in the *next* exact-SHA macrolot
following completion of #1057. PR #84 and `main` unchanged.

## Follow-up #71: renderer invalidation isolation after Focused #1058 candidate
- #1057 Windows red: fail-only artifact `rgsml-terminal-activation-bounded.txt`
  (913-byte ZIP, NOT complete logs) identifies test
  `TerminalActivationIntegrationTest::privateMaster192kOn48kDeviceRemainsUninstalled`,
  failing `sharedEpoch.try_transition(777U,778U,verifiedId,true)`.
  The original policy rejected genuine initial zero MANUAL chain revision.
  Fix already on staging: Product ticket permits exact zero revision but
  still matches complete Source/Project/Master generations and actual rate;
  checked-stage bridge compares EXACT chain revision, including zero.
  Separate stale/wrong-chain tests added. Promoted to PR #84 at
  `3908b6c0c389bdf861dcda3035d7284ddef84dfe`, triggering Focused
  [#1058](https://github.com/casaferla/RGS-MasterLab/actions/runs/42253593966).
  Product Owner exclusively checks Windows/Linux CI results.
- **Following #1058 staging-only / not in PR:** renderer's original
  `void invalidate()` still calls `TerminalCandidateGate::invalidate()`
  and changes serial, progress, pending job, output state. Introduced
  `TerminalPrivateRenderController::try_invalidate_for_m18()`, which
  checks Qt owner thread, consults candidate's status-returning
  `try_invalidate_for_m18()` FIRST and leaves all renderer observables
  intact on BUSY. A single internal finalizer shares original M17
  invalidation code; the legacy M17 ordering remains unchanged.
  Existing native Product activation test asserts a held worker epoch
  rejects both candidate-gate and renderer invalidation, retains exact
  verified stage, renderer status and render invocation count.
  No production caller uses the new renderer admission yet.
- **Remaining hard blocker:** renderer `request_private()`,
  error/incomplete paths, Product cancellation and private source/
  project switch still invoke the legacy void `invalidate()`. They
  can bypass the opt-in status-returning path. A single Product-owned
  state controller and all-callsite migration is REQUIRED before
  turning on asynchronous M18 export, plus M18 FQ and PO acceptance.
  Do not claim M18 merged/qualified from this staging work.

## Chat #71 — next staging while Focused #1058 is held on exact SHA
PO-provided terminal report for [Focused #1057](https://github.com/casaferla/RGS-MasterLab/actions/runs/42246730038): **Windows RED, Linux GREEN**. Windows fail-only M17 terminal artifact (not a broad DSP failure) shows `privateMaster192kOn48kDeviceRemainsUninstalled()` failure at ticket `try_transition`, because the initial M17 ProcessingChain revision can legitimately equal zero. The isolated stage now permits chain revision zero but still pins equality and REQUIRES nonzero Source/Project/MASTER generation plus monotonic nonzero Product epoch. That and the other queued mutation gates were promoted serially to PR #84 SHA `3908b6c0c389bdf861dcda3035d7284ddef84dfe`, triggering [Focused #1058](https://github.com/casaferla/RGS-MasterLab/actions/runs/42253593966). **Only PO checks #1058 result.**

After that PR promotion, no-CI staging added an isolated nonblocking **exclusive Product mutation batch**:
- `M18EpochCommitBarrier::try_begin_mutation(old,new)` atomically advances epoch and holds an RAII mutation lease across a multi-controller Project open; while it is alive a worker `try_acquire`, competing mutation, epoch advance and retire all refuse immediately. Releasing it admits only the *new* epoch. The previous worker commit lease has mutually exclusive priority. No Qt callback or I/O occurs while the short barrier lock is held.
- `M18ProductEpochTicketAuthority::try_begin_product_mutation` under its own consistent mutex updates the Product Source/Project/MASTER/chain identity and consent **with** the batch lease atomically. Even a ticket for the new identity cannot be captured until the batch is released, and an old worker ticket cannot publish. Existing single-mutation `try_transition` still applies to simple non-batch paths.
- Existing `rgsml_m18_windows_candidate_delivery_tests` now includes two extra deterministic regressions: cross-thread worker commit rejected while Project batch is held, and exact Product identity/consent+no-partial-capture behavior through PROJECT replacement and batch release. No new executable/workflow/CI triggered by staging alone.
- **NOT YET PRODUCT WIRED**: `ProjectSessionViewModel` and nested Source/Gold/MASTER/chain controllers do NOT yet hold this batch lease through real Qt project-open; the export worker is still disabled; unrestricted legacy `TerminalCandidateGate::invalidate()` remains a Product blocker. Do not represent this staging primitive as closing M18 or authorizing user-facing WAV export. Keep main unchanged; no FQ/merge without PO.

## 06 Nov 2026 — #1058 dual compile red and bounded diagnostic fix
The Product Owner reported both Windows and Linux RED for Focused #1058
([run 42253593966](https://github.com/casaferla/RGS-MasterLab/actions/runs/42253593966)).
**Do not download complete GitHub job logs.** Inspected only the tiny
`RGS-Focused-Errors-Windows-1058` and `RGS-Focused-Errors-Linux-1058`
artifacts. Both identify the **same** compile defect in
`tests/unit/app/test_mastering_chain_state.cpp:198`: the new M18 test
used `*state.value()==stateBeforeGuard`, invoking a deleted defaulted
`MasteringChainState::operator==` because `dsp::ProcessingChain` has
no equality operator (MSVC C2280/C2088, GCC deleted operator).
This is a TEST compile bug, not a demonstrated failure of the M18 audio
or epoch transaction logic.

Staging commit `fa68ae5ee2c580698c2baffc09e0f19b1b76a48f` removes
that invalid aggregate equality and replaces it with comparisons of
chain ID, module count, revision, all module instance IDs and the
existing Gain/EQ/Compressor/Stereo-M/S parameter snapshots. It does
**not** change DSP algorithms, product behavior, or `main`.
The original defaulted aggregate equality was already unavailable;
the M18 runtime guard does not grant a new equality operation.

The remaining staging changes since #1058 include renderer guarded
invalidation and isolated Product mutation lease/barrier tests. They
are **NOT YET COMPILED/QUALIFIED** and must enter only one coherent
PR #84 SHA for the next serial Windows/Linux Focused CI; the Product
Owner alone will report its results. PR Draft, no merge/FQ/GUI export.

### Follow-up: scoped ProjectSession batch hook and real Project-open regression
- `ProjectSessionViewModel::set_private_m18_project_open_batch_begin`
  is a separate opt-in callback that returns an opaque RAII **Product-owned**
  batch lease after all Project preflight succeeds and **before**
  `stop_and_clear`, `adopt_probed_source`, MASTER-rate/GOLD/chain update.
  The token lives until every Project-open return path completes.
- If batch mode is installed, an independent per-operation
  `M18ProjectMutationAdmission` on the same ProjectSession is forbidden:
  conflicting admission modes fail closed. Wrong QObject thread,
  throwing/null/failed batch also fail before transport/source changes.
  Production registers NEITHER optional mode; M17 remains unchanged.
- Existing `rgsml_project_session_tests` has a new real `.rgsml`
  Project-open regression: batch active during the nested SOURCE adoption,
  released upon return, and a refused batch prevents any SOURCE
  adoption. No new build target or CI workflow.
- **Important scope**: This API provides lexical batch lifetime, NOT
  automatically nested authorization for independent SOURCE/GOLD/MasterRate
  guards. Their Product callbacks must explicitly recognize the SAME
  transaction. Further error-path rollback and full authoritative Product
  wiring are mandatory BEFORE any M18 background export activation.
  No main merge, no FQ, and these tests have not yet run in Focused #1058.

## Chat #71 — corrected single-writer provenance and guarded second M17 Prepare
The Product Owner clarified that the apparent adjacent staging commits
were authored by THIS SAME operational chat before interrupted responses.
They are NOT evidence of a separate concurrent writer. Resume the
existing single staging writer, sequential CAS file updates and serial
Focused CI promotions. Do not stop work or claim an external conflict.
CI results are checked and reported only by the Product Owner.

Following isolated preproduction sublot, NOT included in PR #84 / Focused
#1059: `TerminalCandidateGate::m18_mutation_admission_installed()` is a
read-only indicator of the optional Product epoch gate. In
`TerminalPrivateRenderController::request_private`, when that gate is
installed, first call the existing status-returning
`try_invalidate_for_m18()` before serial/progress/job/M17 candidate
withdrawal. A worker-held M18 commit will immediately fail BUSY; a
second M17 Prepare must not silently cancel the already verified MASTER.
When no gate is installed, the exact legacy `invalidate()` path remains
unchanged. The existing Product integration test adds denied-second-
Prepare, verifies the error and stable renderer state plus retained stage,
and updates the five/six-path denial counter to seven.
This is not yet compiled or independently qualified; still no
production GUI/worker export nor `main` merge.

## #71 post-#1059 green — fail-closed consent on aborted Product batch
Product Owner reported Focused #1059 GREEN. Serial PR #84 fast-forward
to `446ae57a4567f0957adc4da8fccf4f562d0d8767` triggered
[Focused #1060](https://github.com/casaferla/RGS-MasterLab/actions/runs/42258433430).
The PO, not the agent, verifies Windows/Linux conclusions.

Following isolated staging (NOT INCLUDED in #1060), an additional
security review found that `M18ProductEpochTicketAuthority::
try_begin_product_mutation(expected,next,identity,consent=true)` previously
made the new Product export-consented as soon as its RAII batch was
destructed, even if downstream `.rgsml`/SOURCE/GOLD/MASTER restoration
had failed partway through. The worker could then capture a seemingly
current, but **incompletely verified**, new Product ticket.

**Fixed:** Product batch admission now refuses `consent=true`
unconditionally; the batch always advances the epoch with consent=false.
An aborted batch's RAII release CANNOT reinstate worker authorization.
Only a later, separately verified M17 final Master and explicit user
opt-in may call `try_transition(next,next+1,identity,true)` after the
batch has ended and before any ticket can be captured. A held old worker
commit still rejects a new Project batch before identity mutation.

The existing Windows M18 native integration target extends its
Product batch regression: deny forbidden consentful batch, reject old
ticket and partial new Product capture, deny transition while the batch
is active, deny capture even AFTER release, require a second explicit
consent transition, then verify later revocation and worker rejection.
No Product QML hook, export enablement, legacy M17 changes, project-file
schema change, or direct source writer permission are made. This sublot
is **uncompiled until a later exact-SHA Focused CI**.

### Additional #71 no-CI sublot — strict M17 candidate pointer for M18 worker
Previously the typed M17→M18 bridge could accept any retained `StagedCandidate` pointer provided by its caller, as long as output rate and manual chain revision matched the Product ticket. This left a same-rate/same-revision detached previous-Project buffer plausible before the real Product was wired.
The signature now also requires `const TerminalCandidateGate& currentProductGate` and rejects unless `currentProductGate.staged() == stage` on the one Product owner thread. This check precedes zero-copy alias construction or worker lease factory publication. The native M17 Product test passes its real current candidate gate for all valid, wrong-Fs and wrong-chain cases and adds a regression with a distinct empty gate + the exact same valid ticket and old stage: the request MUST fail `InvalidState`.
Together with the monotonic epoch and Product mutation contract, a new request must originate from the actual current M17 candidate; a retained old buffer cannot be reused after candidate-gate replacement. This does NOT yet prove the physical file Source identity; authoritative SourceSelectionViewModel reference, Project generation and fully-wired mutations remain mandatory at real GUI activation.
These test and bridge changes are STAGING ONLY, not part of Focused #1060 and not yet compiled on Windows or Linux. Do not promote while #1060 is awaiting Product Owner verdict.

### Chat #71 subsequent staging macrolot — SOURCE provider/locator + destination contract
After the last #1060 PR candidate (SHA `446ae57a`), the Qt-free
`make_m18_checked_stage_worker_request` preproduction bridge now receives
the *authoritative current SOURCE reference* from the Product owner
alongside the requested export SOURCE reference and destination. Before
creating ANY worker request or aliasing the retained M17 PCM it rejects:
- unreadable authoritative current SOURCE or unreadable requested SOURCE;
- provider ID or opaque locator mismatch, including a same-locator but
  different-provider source;
- destination without write permission or with exactly the same
  provider/locator identity as SOURCE;
- the preexisting stale/detached M17 candidate gate, wrong Master Fs,
  chain revision mismatch, or invalid M17 terminal verification.

The native M17 Product integration test is updated at all four bridge
call sites and adds negative tests for wrong locator, wrong provider,
unreadable current source, unreadable requested source, same-resource
write destination, and non-writable destination. It retains the
zero-copy PCM pointer-equality assertion and previous stale-gate,
rate and chain checks. All new ResourceReference fixtures have at least
one valid permission, as required by `ResourceReference::create`.

**Security scope:** Provider+locator equality is a strict logical
identity check, not proof of the same physical file under Windows
symlink/hardlink/rename semantics. The Windows transactional exporter
remains responsible for physical alias handling and final source/dest
readback. At any production UI integration, the Product owner must
read the *actual* SourceSelectionViewModel current reference without
reentrant event-loop dispatch, bind generations to the exact verified
M17 stage, and enforce one Product-wide mutation epoch until commit.
These functions remain uncalled by the GUI; no Project schema keys or
DSP algorithms change.

**Qualification:** Entire sublot is staging-only, not in Focused #1060,
and has no new CI yet. Next PR promotion requires the Product Owner's
#1060 verdict plus an exact-SHA ancestry/diff check. No merge/FQ.

### Post-#1061 staging — Qt owner SOURCE snapshot and source-locator handoff
The Product Owner reported Focused #1060 GREEN; an exact serial
fast-forward moved Draft PR #84 to SHA
`9b0bc8c9ba71249788302d3b8897d05b6d1c4fd3` and launched
[Focused #1061](https://github.com/casaferla/RGS-MasterLab/actions/runs/42262727474).
Only the PO checks run results, so this staging lot has NO knowledge of
the #1061 conclusion.

A subsequent isolated larger macro-lot tightens actual owner provenance:
- `app/src/internal/m18_owner_source_snapshot.hpp` provides an
  opt-in **Qt owner thread** capture of the actual selected
  `SourceSelectionViewModel::source_resource()->reference()`. Fails
  closed before reading QObject state from a worker thread, when
  SOURCE is missing, or when the source lacks READ permission. Returns
  a VALUE copy, not a QObject/pointer sent to worker.
- `app/src/internal/m18_checked_stage_worker_bridge.hpp` additionally
  exposes the Product-facing
  `make_m18_checked_stage_worker_request_from_product_source`
  overload, which obtains the reference from the real QObject before
  passing to the already-typed Source/MASTER/epoch identity validator.
  This overload is inert until a real Product callsite has all upstream
  mutation gates installed.
- Existing `tests/ui_smoke/test_source_metadata_panel.cpp` adds a
  focused Source snapshot regression: no SOURCE must refuse,
  actual in-memory probed SOURCE must capture its exact identity,
  wrong-thread worker must reject without dereferencing QObject,
  capture remains current afterward, and two physically distinct
  valid WAV paths with identical content must produce DIFFERENT
  source identity snapshots after selection.
- Existing Windows real-file delivery test now checks aborted-Project
  rollback for **both PCM24 and Float32** rather than PCM24 alone.
- Existing Windows epoch regression covers Product destruction while
  an exclusive Project mutation token is alive (no stale worker
  authority thereafter), plus all six rate identities and zero/
  unsupported generation/rate rejection.

This is NOT active Product export, no GUI control, no new Project
schema, no new CI target, no DSP/Final SRC change. Source file physical
identity, symlink/hardlink revalidation and the product-wide mutation
epoch remain mandatory before enabling a user export.

### Post-#1061 staging final macro-batch — Product minting thread and EOF epoch safety
Completing one 15+ commit staging-only lot before serial promotion:
1. `M18ProductEpochTicketAuthority` records the immutable C++ creator
   thread ID, distinct from the worker lease factory. Its three
   Product-owner operations `try_transition`,
   `try_begin_product_mutation`, and `capture` now **reject calls from
   any foreign thread** with InvalidState BEFORE locking or minting
   credentials. A worker cannot grant its own export consent, change the
   Project identity, or create a new ticket. Normal worker
   `Ticket::commit_lease_factory` remains Qt-free and cross-thread safe.
2. The existing native M18 Windows delivery integration test checks
   wrong-thread attempts for all three owner operations, then confirms
   the owner-thread ticket remains current. Further strict identity
   tests reject wrong Source/Project/MASTER generations (including
   invalid generation zero), enumerate all six supported M17 Final SRC
   rates and unsupported rates, and prove the monotonic epoch cannot
   wrap or create a stale factory at UINT64_MAX.
3. The actual Qt SourceSelection snapshot test now also loads two REAL
   distinct WAV paths with byte-identical audio; each selection must
   issue a different provider+locator identity. Owner-thread capture
   fails before dereferencing QObject if invoked by the worker.
4. If copying the live SOURCE reference throws std::bad_alloc,
   `capture_m18_current_source_on_product_owner` returns IoFailure
   rather than creating a partial/forged export request.
5. The preproduction Product-source worker bridge remains **opt-in,
   unused by the actual GUI**, with zero-copy M17 stage and exact
   Source/master/epoch read validation before publication.
6. The Windows real transaction rollback test checks both PCM24 and
   Float32 after a failed Project-open mutation batch; another test
   proves owner destruction while a batch token remains alive does
   not revive stale worker permissions.

**Qualification boundary:** No changes above are in the already-running
Focused #1061; do not claim PASS before that new candidate completes
its own Windows+Linux Focused gate. No extra PR promotion while #1061
is pending, no Full Qualification or main merge. Before production
activation, all authoritative Source/Project/MASTER/M17 candidate
mutators must share one Product epoch and the worker must receive the
typed verified stage, immutable SOURCE, and fresh explicit consent.

### #71 after Focused #1061 GREEN — Product session to worker seam macrolot
Product Owner reported #1061 Windows+Linux GREEN; the Draft PR #84
advanced by a single exact-SHA fast-forward from 9b0bc8c9 to
`af0528b4e73430f0d90d8b415c8b9f0bf3bc0bf5` and triggered
[Focused #1062](https://github.com/casaferla/RGS-MasterLab/actions/runs/42265949888).
The PO alone checks the #1062 outcomes. No CI polling or extra PR
promotion while #1062 is pending.

Next staged-only coherent macrolot brings the **real private Product
session** one step closer to the existing Qt-free Windows worker:
- `TerminalPrivateProductSession::capture_m18_verified_stage_for_worker`
  returns the EXACT retained M16-checked M17 `StagedCandidate` only on
  its Qt thread, only for the selected final MASTER rate, and only
  while it is the same Product candidate-gate stage. It does not
  issue any export approval, modify playback, or copy PCM.
- `make_m18_checked_stage_worker_request_from_live_product` takes
  the actual `TerminalPrivateProductSession`,
  `SourceSelectionViewModel`, `MasterOutputRateState`,
  `TerminalCandidateGate`, the explicit immutable Product ticket,
  source/destination resource references, encoding, and dither seed.
  Every QObject must have the SAME current owning Qt thread. It
  derives the stage and current SOURCE/rate on that thread, then
  delegates to the existing checked zero-copy exporter request factory.
  Worker invocation is forbidden.
- The Product ticket now carries a separate optional
  `master_rate_revision` scalar. The LIVE Product builder *requires*
  nonzero exact equality to `MasterOutputRateState::rate_revision()`,
  which increments on every accepted SOURCE commit EVEN if sample rate
  and locator appear unchanged, and on selected MASTER rate edits.
  Standalone synthetic ticket tests can still omit this optional
  field, but such tickets cannot be accepted by the real Product
  worker builder. A stale pre-SOURCE-selection ticket thus cannot
  borrow a subsequently selected stage just because its Fs matches.
- Existing Windows native M17 Product activation test closure now
  links the actual SourceSelectionViewModel. A genuine local IEEE-F64
  WAV generated from the real verified M17 master is loaded through
  that component in a QTemporaryDir, the actual Project MASTER rate
  is adopted, and a new revision-bound Product ticket is minted
  solely for the test. Assert exact zero-copy M17->M18 PCM pointer,
  no second render, and correct PCM24 and Float32/no-dither requests.
  Deny stale requested SOURCE, missing SOURCE, wrong/absent Project
  MASTER rate, unbound identity revision, same-rate SOURCE reselection,
  illegal PCM24 missing seed, illegal Float32 seeded request, and
  invocation from a foreign worker thread. Existing M17 Product
  cancellation, audition, M16 and FIR tests remain in the same native
  executable.

**Important unresolved integration boundary:** This full Product
builder is private and UNUSED by `app/src/main.cpp`. No user-facing
Export is enabled, no async job is launched, no new saved Project
schema is approved. Numeric Product source/project/master generations
still require one authoritative coordinator that gates ALL mutations,
including legacy void candidate invalidations and Qt reentrancy, plus
FQ cold Windows and Product Owner physical acceptance before any
shipping export. These new tests/source are not compiled or qualified
in #1062. Do not merge PR #84 yet.

### #71 post-#1063 staged macro-lot — asynchronous, cancelable M18 WAV delivery
Product Owner reported Focused #1062 Windows+Linux GREEN; serial PR #84
fast-forward to `a00236137e2bca2eab45cd9fcb170d025ed714a1`
triggered [Focused #1063](https://github.com/casaferla/RGS-MasterLab/actions/runs/42268433766).
The PO alone checks #1063 results. All work below is on the single
NO-CI staging branch, **not** on PR #84, and is **not compiled/qualified**.

NEXT coherent functional step toward a real (but still disabled) M18 export:
- The canonical Windows `TransactionalAudioExporter` now accepts an
  OPTIONAL worker-only cancellation predicate and bounded
  encoded-frame callback for the M18 request overload ONLY. The existing
  F64 synchronous export path passes null cancellation and progress
  pointers and preserves its original encoding/commit behavior.
- M18 cancellation is checked before candidate creation, once per
  4096-frame PCM write, before expensive candidate validation,
  immediately after validation, while the commit lease is held before
  publication, and after destination SHA/readback. Rejection cleans the
  temporary candidate; if requested after the newly published WAV
  appears but before the final checkpoint, cleanup removes that newly
  published destination. A throwing cancellation predicate is treated
  as CANCEL, while a throwing progress observer aborts and cleans up.
  No success/verified delivery is emitted until file SHA, delivered
  audio verification, final Product authority and cancellation checks.
- `platform/windows/src/internal/m18_async_export_worker.hpp` contains
  the first Qt-free detached M18 export worker. The only things crossing
  threads are the immutable, lifetime-retained MASTER request with
  source/destination reference values, exact lease factory, shared
  cancellation flag and encoded-frame progress atomics. It owns NO
  QObject, QPointer, UI renderer, or mutable SOURCE state.
  Dropping the UI-side handle requests cancellation without waiting
  for encoding to finish. The background thread owns its own lifetime
  until it records a success or terminal failure.
- Progress is deliberately `encoded_frames()/total_frames()`, NOT
  a claim of file delivery. `finished()` and
  `result_if_finished()` are separate. 100% frames can be encoded
  while SHA/true-peak/epoch/rename/readback remain outstanding.
  Cancellation requested after verified completion does not remove
  an already successful file; pre-publication cancellation is
  cooperative, with bounded check points.
- The existing native Windows M18 delivery integration test extends
  both PCM24+seed and Float32+no-dither with: cancel before creation,
  midway through PCM streaming, after publish/readback, callback
  exception, progress-observer exception, successful detached worker
  delivery and SHA verification, atomic encoded frame evidence,
  duplicate-output FAIL_IF_EXISTS without changing delivered bytes,
  deterministic cancellation while an export waits at commit lease,
  deterministic revocation of the Product epoch before the worker
  reaches commit, 100%-encoded-but-not-verified assertions, source
  bytes intact and no orphan candidate files. No QML or actual app
  export action is activated.

Still **BLOCKED for real Product activation**: one authoritative
Product epoch wired BEFORE every Source/Project/MASTER/DSP/M17
mutation, explicit user consent and final .rgsml encoding+seed key
mapping, Qt-side export lifetime/polling/error presentation, full
Windows/Linux Focused gate for this new batch, separate cold FQ,
physical PCM24/Float32 acceptance, and PO merge authorization.
The detached worker is strictly an internal preproduction component
and must NOT be invoked by QML before all gates are wired.

### Chat #71 post-#1063 GREEN: actual Product async handoff + dormant Qt job model
PO reported Focused #1063 Windows and Linux GREEN. The sole Draft PR
#84 was fast-forwarded, with exact expected old SHA, to
`c49c2d5ef921b00d2ff5c174e981c5e3eccb98dd`, triggering
[Focused #1064](https://github.com/casaferla/RGS-MasterLab/actions/runs/42271049335).
The Product Owner is the EXCLUSIVE observer/reporter of test outcomes.
NO polling of CI results and NO log-complete GitHub downloads.

The next cohesive staging-only 16+ commit macro-batch moves M18
significantly closer to Product/GUI integration but is deliberately
not activated or qualified:
1. `app/src/internal/m18_private_async_export_handoff.hpp` is an
   explicit typed Product-owner → worker handoff. It captures the actual
   current M17 verified stage, selected SOURCE, Project MASTER Fs
   and exact revision, M18 epoch ticket, encoding, seed and destination
   on the owning Qt thread; only the lifetime-retained immutable
   `M18TransactionalAudioExportRequest` crosses to
   `M18AsyncExportWorker`. Early preflight failure launches NOTHING.
2. `app/src/internal/m18_private_async_export_session.hpp` owns at
   most ONE active/pending worker. A result must be explicitly ACKed
   before a second export starts; GUI teardown calls nonblocking
   `cancel()`. Snapshot provides encoded frames, total, precise
   status and ErrorCode, distinguishing `FAILED` (I/O, SHA, collision,
   stale epoch) from actual `CANCELLED` only when exporter evidence
   starts with the exact cancellation reason. 100%-encoded is NOT
   `VERIFIED_DELIVERED` until the worker finishes SHA+readback.
3. `app/src/m18_private_export_progress_view_model.hpp/.cpp`
   declares a real Qt `Q_OBJECT`, testable Q_PROPERTY statusText,
   encodedProgress, exportInProgress, canCancel, canAcknowledge,
   verifiedDelivered and errorText. An 80-ms owner-thread timer
   observes only worker atomic data; it stops on terminal states.
   Starting is a typed PRIVATE C++ method, intentionally NOT
   `Q_INVOKABLE`, while Cancel/Acknowledge are genuinely functional.
   No instance is created or registered in `app/src/main.cpp`
   or QML, so there is NO new menu privilege or partially working
   production Export.
4. The existing Windows M17 terminal Product integration CMake
   closure compiles the new Qt ViewModel with AUTOMOC, so early
   compile/link errors are caught on the next focused candidate.
   An end-to-end native Product test now starts a real M18
   asynchronous worker from genuine SourceSelectionViewModel and
   already checked M17 PCM, writes real PCM24 seeded and Float32
   undithered WAVs, checks SHA/readback, frame counts, final file
   and no overwrite on duplicate export. It also tests a single-job
   lifecycle with explicit acknowledgment, and the real QObject
   ViewModel: IDLE, encoding/verification completion, VERIFIED,
   100%-encoded progress, available Cancel/Acknowledge controls,
   duplicate-output error preserved as FAILED even after late Cancel,
   preflight denied if SOURCE absent, wrong-thread start/ACK denial,
   signals and return to IDLE after acknowledgment.
   **No full-resource re-render or PCM copy is permitted.**

Staging-only: no CI for this new batch, no main merge, no M18 FQ,
and no physical Product GUI Export enabled. Hard blocker remains
wiring ONE Product epoch to every SOURCE, Project, chain/DSP/MASTER,
candidate-gate and M17 renderer mutation before any production worker
publication. Project output schema / seed semantics need final PO
approval, together with physical Windows tests and cold FQ.
Do not move PR #84 before the PO explicitly reports #1064 outcome.

### Chat #72 — staging-only GOLD and Project MASTER fail-closed coverage (after #1064 candidate)
The Product Owner's explicit Windows/Linux #1064 verdict has not yet been
provided in this chat, so **PR #84 stays pinned to `c49c2d5e`**.
No CI/FQ/main promotion is authorized by this sublot.

- `GoldSelectionViewModel` now has a separate optional,
  runtime-only `set_private_m18_gold_mutation_admission`. It checks
  the owning Qt thread and Product admission BEFORE a direct GOLD
  replacement or clear touches `AuditionSourceSelector`; guard
  exceptions fail closed. Native Project-session integration exercises
  an existing GOLD owner, denied replacement, denied clear, and
  restoration of the unchanged legacy behavior after removing the gate.
  Foreign-thread clear returns without mutating even Qt error state.
  This GOLD callback is **NOT registered in the application**.
- SOURCE-driven GOLD removal in `GoldSelectionViewModel::sourceChanged`
  is intentionally distinct: the SOURCE/Project epoch must have been
  authorized BEFORE its selector change. A GOLD-level callback cannot
  safely retroactively reverse an already published SOURCE transition.
- `ProjectSessionViewModel::openProject` no longer `Q_ASSERT`s and
  continues on a rejected nested MASTER rate choice; instead it reports
  `PROJECT_MASTER_RATE_MUTATION_REFUSED` and exits without applying
  subsequent GOLD/region/DSP snapshot changes. The corresponding
  native Project-session test injects a denied M18 rate mutation,
  confirms the old default MASTER rate is preserved and the Project
  open is visibly refused, then retries successfully after removing
  the admission.
- **Critical remaining limitation:** the SOURCE may already have been
  adopted before the nested MASTER choice is rejected. Fail-closed
  consent revocation and exclusive Project-wide epoch batching are
  mandatory; neither this defensive fix nor a collection of local
  callbacks makes Project-open fully atomic. Candidate-gate legacy
  void invalidation, all nested owner transitions, error reentrancy,
  and actual Product registration remain outstanding.

These are independent staging commits only. They are **uncompiled and
unqualified** until a future coherent exact-SHA Focused CI, and do
not enable M18 Export, add GUI controls, or alter frozen M16 DSP.

### Chat #72 — post-#1064 admission batch and CI #1065 (PREPRODUCTION)
PO confirmed #1064 Windows+Linux SUCCESS. Draft PR #84 was promoted ONCE
by expected-SHA fast-forward from `c49c2d5ef921b00d2ff5c174e981c5e3eccb98dd`
to `8f19dcc0b8262fd2014eccba9d87eec4122429da`, strictly 27 ahead,
0 behind, with no protected M16 B2i source changes. This triggered exactly
one Focused [#1065](https://github.com/casaferla/RGS-MasterLab/actions/runs/42278593611)
Windows+Linux on SHA `8f19dcc0`. **Result belongs to the PO; no CI log
polling or premature PR movement.** Main has not changed.

Following the promotion, the separate NO-CI staging adds a new cohesive
admission/negative-test sublot:

- New M18 opt-in `MasterOutputRateState::try_source_committed_for_m18`
  and `try_source_cleared_for_m18` reject worker-held BUSY, foreign-thread
  admission and throwing gate BEFORE SOURCE-rate/revision Qt notification.
  Exact same-48k SOURCE reselection must advance revision if authorized,
  not bypass the gate as a no-op. Uninstalled legacy void APIs unchanged;
  **all real Product callsites still need migration**.
- `choose_master_rate` now reuses the same fail-closed owner-thread
  guarded mutation check rather than directly invoking a callback which
  could accidentally be called from a foreign worker.
- `SourceSelectionViewModel::selectSource` checks owner thread BEFORE
  Qt path/error processing when the optional Product gate is installed.
  Existing native SOURCE tests prove busy rejection, same SOURCE retention,
  no wrong-thread error/signals, and successful legacy restoration when
  the opt-in guard is removed.
- Existing native MASTER-rate tests prove denied same-rate SOURCE rebind,
  distinct-rate change, clear, throwing callback and subsequent accepted
  mutation/revision notifications.
- Native real Project-open batch test now serializes and asserts a nested
  SOURCE selection, an accepted 192k Project MASTER restoration and a GOLD
  adoption all under the SAME RAII Project mutation batch. Their callbacks
  reject changes outside the batch; the Product batch must be released
  at function exit. New test remains uncompiled until a later CI.

**Still not Product wired:** none of these new opt-in guards is installed in
`app/src/main.cpp`; unguarded legacy void paths can still bypass M18;
Project-open can partially adopt SOURCE before a rejected nested MASTER
restore, even though export consent must remain revoked. No GUI Export,
Product schema/seed approval, physical test, FQ or main merge is authorized.

### Chat #72 follow-up — Focused CI coverage correction (staging only)
Review of the existing `.github/workflows/pr-focused.yml` found that
`rgsml_source_selection_tests` was a genuine CMake QtTest target but was
not compiled or run in the reduced Focused CI build list on either OS.
The new opt-in SOURCE thread/BUSY regression would therefore have been
invisible to Focused. The workflow now adds this one existing target
to the explicit Linux and Windows build lists AND both CTest filters.
No broad log/download, new workflow, DSP/FIR changes, or FQ dispatch.
The workflow change will accompany the next complete staging macro-lot
AFTER the PO reports #1066's terminal result; it is not in the pinned
PR #84 candidate under review. Linux/Windows remain platform-parallel.

### Chat #72 subsequent staging — Project SaveAs independent admission while Open has a batch
**Do NOT mistake an Open-only Product batch for a SaveAs permission.**
Previously `ProjectSessionViewModel::set_private_m18_project_open_batch_begin`
could protect Project-open but could not coexist with the generic
`set_private_m18_project_mutation_admission` (Open explicitly refuses
ambiguous modes). Its SaveAs path therefore had no independently
available M18 permission and could publish an unguarded `.rgsml` file.

The isolated, opt-in fix provides
`set_private_m18_project_save_admission` separately from the Open
RAII-batch path. SaveAs refuses BEFORE snapshot/file allocation when:
- an Open batch exists without a separately installed Save admission;
- the generic gate and the dedicated Save gate conflict;
- a Save gate explicitly refuses, throws, or is called from a foreign
  Qt thread.

Without any M18 gate the legacy M17 SaveAs remains unchanged; the
original generic M18-only save policy still works when no batch is set.
Project open/save check foreign-thread calls before Qt error signals or
filesystem access whenever M18 policy is installed.
The existing Windows-native Project session tests add denial, exception,
ambiguous-guard, foreign-thread, rollback/no-output, and authorized SaveAs
cases. **Staging only, uncompiled, no Product registration, no PR CI move.**

This fixes one specific bypass but does NOT by itself close a Product-wide
single-writer epoch: physical SOURCE changes, DSP, terminal limiter,
candidate-gate legacy invalidations, nested project-error callbacks, and
fresh verified M17 consent must all participate before GUI export activation.

### Chat #72 — consolidation into 15–25 commit macrolots: SaveAs owner lease + reentrancy
PO requested bigger coherent CI batches, rather than dispatching one Focused
run after only 8–10 commits. This staging continues after immutable PR #84
`9cc9e471` / Focused #1066, without touching the pinned candidate while
the PO checks Windows/Linux. No CI or FQ dispatched for these changes.

The Project Open-batch vs SaveAs boundary now has an explicit optional
`M18ProjectSaveBatchBegin` returning a Product-held opaque RAII lease.
When the Project has an Open-batch policy, a one-shot SaveAs status grant
is insufficient and must fail closed; a real SaveAs batch must own the epoch
from BEFORE snapshot capture until AFTER canonical .rgsml create-new,
readback/verification, and final Project metadata/Qt signal publication.
Missing, null, throwing or competing grants are rejected without file
creation. An unchanged legacy M17 session installs no M18 guard; previous
behavior stays unchanged. A separate Save-only gate cannot authorize
unguarded Open Project.

A shared owner-thread `M18ScopedProjectOperation` sentinel now prevents
nested/reentrant Project Open/Save calls, including synchronous Qt signals,
while a Product-gated operation is executing. Its RAII lifetime covers all
returns, including errors and exceptions handled by admission. It does not
block the Qt event loop. Without M18 policy, legacy M17 behavior is intact.
The owner-only guard remains dormant in the application.

Windows-native Project QtTests added: null/throwing/ambiguous SaveAs leases,
collision rollback with original bytes preserved, actual status-signal
publication while the lease is still held, refusal of Save-only Open,
nested Save during Save, nested Open during Save, and nested Save/Open during
Open; independent second commands after scope exit must succeed normally.
The real M18 Product epoch coordinator, renderer legacy invalidation,
SOURCE/Gold/project mutations and explicit export consent are still **NOT**
fully installed: therefore NO M18 GUI export, Product Merge, FQ or release.
All new sublots are **uncompiled** until a later Windows/Linux CI gate.

The Focused workflow staging also adds the existing SOURCE-selection native
test target to BOTH build/test matrices, avoiding blind spots in the earlier
reduced Focused CTest allowlist. No M16 B2i frozen source, M17 DSP, FIR assets,
audio codecs or already-green candidate paths were edited in this batch.

### Chat #72 after #1066 green — M18 direct terminal Single-Writer closure (staging only)
PO confirmed #1066 Windows+Linux GREEN. PR #84 was fast-forwarded from
`9cc9e471` to `86177384eef2cdb930b78678c93159c1fc4e3722`,
promoting a 22-commit macrolot with zero divergence. The next single
Focused [#1067](https://github.com/casaferla/RGS-MasterLab/actions/runs/42288760084)
runs Windows+Linux on exact SHA `86177384`. No CI result is to be
inferred from elapsed time; the Product Owner reports it. Main unchanged.

The next cohesive 16+ commit staging-only batch addresses a bypass in
the *terminal DSP state itself*, not the GUI editor alone:

- `TerminalLimiterState` owns an optional Qt-free M18 Product mutation
  admission. The real thread owner is captured at construction. Any
  DIRECT canonical Ceiling or BYPASS mutation must pass before
  changing parameter / chain revisions, including attempts bypassing
  `TerminalLimiterViewModel`. Invalid Ceiling and true no-op Ceiling
  changes never burn an epoch. A no-op BYPASS under M18 guard also
  leaves authority unchanged. No callback installed = old M17 behavior.
- A foreign-thread direct mutation is rejected before calling the
  Product epoch callback. Throwing callback or BUSY refuses without
  changing data or revisions. A scoped reentrancy sentinel prevents
  a callback from recursively committing another terminal mutation
  before its own admission has finished. No GUI thread wait/spin.
- `TerminalLimiterViewModel` now explicitly rejects *ambiguous dual*
  M18 guards (VM + State): one user action cannot advance two distinct
  epochs. When only the direct state guard is installed, all accepted
  edits still flow through it exactly once; successful no-op edits
  invoke no mutation callback. When any M18 guard is installed, a
  foreign thread cannot even modify Qt-owned draft fields or fire
  editor notifications. Ceiling, reset, and bypass statuses distinguish
  active-export refusal from invalid parameter values.
- Existing native Windows M17 Product QtTest adds direct State attacks,
  illegal foreign thread, actual `M18ProductEpochTicketAuthority`
  commit lease (BUSY then accepted/revoked), reentrancy, dual-guard
  ambiguity, one legitimate VM mutation per epoch, no-op behavior,
  error/status and unchanged M17 fallback. All new tests remain
  **UNCOMPILED until the next approved Focused candidate**.

Hard blockers unchanged: state/VM admission is not registered in
`app/src/main.cpp`, all candidate-gate legacy void invalidations
and Project/SOURCE/Gold dispatches must share one Product epoch,
fresh verified M17 Master and final output preference/PCM24 seed rules
require explicit PO consent, and M18 must complete Windows cold FQ and
physical PCM24/F32 acceptance. NO GUI Export enabled, NO main merge,
NO qualification claim for this staging batch.

### Chat #72 — CI #1067 Linux build failure diagnosis and portability correction
[#1067](https://github.com/casaferla/RGS-MasterLab/actions/runs/42288760084)
on `86177384` finished Windows SUCCESS and Linux FAILURE at
`Build Focused DSP Targets`. The **bounded** 545-byte artifact
`RGS-Focused-Errors-Linux-1067-28b199152d7c93982cb68a218973d1f537103a9c`
was inspected without downloading any GitHub job logs. Its exact compiler
diagnostic is from `platform/windows/src/transactional_audio_exporter.cpp:22`:
`fatal error: Windows.h: No such file or directory`.

Root cause: the preceding staging change placed native
`rgsml_source_selection_tests` in BOTH Windows and Linux Focused
build/CTest lists. That Qt integration target links the Windows
`rgsml_platform` native implementation and cannot be built by the
portable Linux Focused guard. It was **not** a failing SOURCE test and
not an audio/limiter algorithm regression.

Fix: remove only the SOURCE target from the Linux focused build
list and Linux CTest regex. Keep the SOURCE target in BOTH Windows
Focused build and Windows CTest regex, so its genuine native M18
BUSY/owner-thread cases remain covered. Existing Linux portable
Master-rate and independent M18/dither/DSP tests remain covered.
Do not add fake Windows headers, condition out functional Windows
code, alter frozen M16 B2i, or misrepresent Windows-native source tests
as Linux-portable tests. One new exact-SHA Focused Windows/Linux
candidate is required before qualification; no old #1067 result can
qualify the updated candidate.

### Chat #72 — next NO-CI staging after #1068 dispatch: SOURCE Region/Loop authority
While Focused [#1068](https://github.com/casaferla/RGS-MasterLab/actions/runs/42294595972)
checks exact PR candidate `1a0e0628`, a separate unqualified batch
guards `AuditionRegionViewModel` with an optional Product-shared epoch
callback. This is not enabled in `main.cpp`, and no GUI Export is
visible. It covers direct `set_region`, `clear_region`,
`set_start`/`set_end_exclusive` via `set_region`, and
`set_loop_enabled` BEFORE playback loop mutation. Busy/throwing,
reentrant, foreign-thread calls fail closed. True no-op loop toggles
do not advance the Product epoch. Foreign worker QML time edits/nudges
cannot publish Qt signals or change Region metadata.

Normal Qt-owner `seek` remains allowed while worker consent is live:
navigation has no authority to mutate exported Master PCM, SOURCE or
selected MASTER rate. SOURCE-driven `source_committed` is a derived
part of the OUTER, already-admitted SOURCE/Project epoch and does not
advance another independent Region epoch. The actual Product must
install the common gate and deny SOURCE outside the exclusive batch.
New Windows native Region QtTest proves busy, foreign-thread,
reentrancy, no-op, live seek and restoration after guard uninstall.

The Focused workflow staging adds `rgsml_audition_region_tests` to
the **Windows** native build/CTest list. It deliberately does NOT add
Windows-dependent SOURCE tests to Linux (the exact #1067 failure).
Linux portable DSP tests remain unchanged. These changes are
staging-only and **UNCOMPILED**, with no claim of qualification or
full Product Single-Writer completeness.

### Chat #72 — #1068 GREEN, full Region/Loop publication-scope macrolot
The Product Owner reports Focused #1068 green; the GitHub run was verified
SUCCESS on exact PR #84 SHA `1a0e0628f07f2cc5ec15a7575a9f05613aa217b9`.
The next isolated staging group extends the optional M18 Region gate:

- An owner-thread RAII `M18RegionPublicationScope` now spans validation,
  Product epoch admission, playback loop change, exact Region publication
  and synchronous `changed()` callbacks. It rejects nested Region
  setters and loop toggles during an active publication *without* Qt
  error-signal recursion. The separate existing admission-time sentinel
  still protects the Product callback itself.
- Invalid Region bounds and invalid Loop-enable requests are checked
  BEFORE the mutation admission, and identical Region/Loop requests are
  true no-ops when M18 mode is installed. Neither errors nor no-ops may
  burn an export epoch. Legacy M17 semantics remain unchanged when the
  opt-in gate is absent.
- New Windows-native QtTests cover an unchanged Region while a worker
  is BUSY, invalid bounds, invalid enable without a region, unguarded
  owner-thread seek, and reentrant `changed()` listeners attempting
  `clear_region` during Region/Loop publication. A later independent
  owner mutation succeeds after the RAII scope is released.
- The exact `rgsml_audition_region_tests` target is included only in
  Windows Focused compile and CTest, never accidentally in the Linux
  portable target list (cf. #1067 `Windows.h` build failure).

The scope is opt-in, not Product registered. A genuine single writer
still requires shared SOURCE, Project, GOLD, MASTER rate, terminal and
candidate-state admission with explicit owner-consent retirement. No
Release, Export GUI, FQ or merge is authorized by these isolated changes.
The next Focused run must compile/test the *exact promoted SHA* on both
OS before the PO may accept this lot.

### Chat #72 — Focused #1069 Windows red: bounded Qt smoke diagnosis and isolated staging fix
PO reported Windows FAILED on [#1069](https://github.com/casaferla/RGS-MasterLab/actions/runs/42297300251),
PR SHA `2d3c87431739eed9e0c1ef0d5b5ec241cb0ed8ec`.
Only tiny failure artifact ZIPs were fetched, never complete GitHub job
logs. The direct M18 QtTest summary states **20 passed/0 failed** and
terminal activation states **22 passed/0 failed**. Windows UI Smoke
`SourceMetadataPanelSmokeTest::emptyReadyErrorAndWindowLifecycle`
failed one assertion at line 5939: expected exactly `NOT AUDITIONED`,
observed the preceding `PAUSED` after a fixed 50-ms sleep. The
`CompressorViewModel` refreshes from a 33-ms Qt timer; overloaded
Windows CI can miss its first tick after a target handoff. The test
already used `QTRY_COMPARE_WITH_TIMEOUT(..., 1000)` later for the
exact BYPASS convergence. Fix the **smoke assertion only** to use the
same bounded Qt event-loop eventual check for the *unchanged expected*
`NOT AUDITIONED` state; neither DSP semantics nor assertions are
relaxed. This diagnostic indicates timing sensitivity, not proof of a
user-visible code regression.

The additional Windows report step failed because CTest was skipped
after the preceding UI smoke failure. The bounded M17 matrix artifact
reported `DIRECT_GRID_TEST=PASS` yet
`WINDOWS_PRODUCT_TEST=NOT_EXECUTED`; the full positive attestation
requires CTest and is correctly withheld. It is NOT an independent
failed 432-route audio algorithm test. No qualifying inference is
permitted until the next exact-SHA Focused Windows/Linux run.

The same isolated staging macrolot adds independent M18 selector
admission/publication reentrancy protection:
- GOLD clear fallback to PREPARED or PROCESSED uses one admission, never
  a second Product epoch;
- already-empty GOLD and unchanged audition target are identity no-ops;
- owner-thread scoped publication lease spans playback handoff and
  `changed()` callbacks; synchronous listeners cannot set GOLD or
  change target while the outer Product epoch is in flight;
- Product admission itself is not recursively callable and exceptions
  fail closed;
- native QtTests prove two fallbacks, denied nested admission,
  denied synchronous changed() reentry, throwing gate recovery, foreign
  Qt wrapper denial, and later independent authorized edits.

Current branch remains NO-CI staging and uncompiled. The #1069 Linux
outcome must be supplied by PO; no branch promotion/next CI before its
terminal result. PR #84 and main must not be merged. M18 Product-wide
single writer and physical qualification remain unresolved.

### Chat #72 — after #1069 Linux green, Focused #1070 and next isolated Product-epoch batch
The PO reports Focused #1069 **Linux green**, Windows red. GitHub
job data confirms Linux SUCCESS and Windows FAILURE on exact
`2d3c8743`. A fast-forward promoted the isolated 16-commit Windows
smoke timing correction and AuditionSourceSelector coverage to Draft
PR #84 SHA `1ff2915df45c774e619d1447927c5450a7dfee21`.
The one new Focused [#1070](https://github.com/casaferla/RGS-MasterLab/actions/runs/42299752247)
was queued on that exact SHA. PO controls Windows/Linux outcomes.
No Full Qualification or main merge.

A **separate, UNCOMPILED** 15-commit macrolot was prepared without
moving PR #84 or starting another CI. It closes two related
Single-Writer bypasses in the optional M18 mutation API:
1. `MasteringChainState`'s previously installed status callback had
   no thread affinity and allowed recursive edits or unregistering the
   callback during its own invocation. It now captures the Product
   installation thread, denies foreign mutation without calling the
   Product epoch callback, scopes a reentrancy bit through exceptions,
   and blocks unauthorized foreign/unwinding policy replacement. The
   ephemeral runtime guard survives authorized Project DSP-chain
   adoption, retaining the original owner. Native focused chain tests
   cover direct foreign edits, attempted foreign unregister, callback
   exceptions, reentrant edits/self-removal and Project replacement.
2. `GoldSelectionViewModel::adopt_probed_gold/clearGold` already called
   the inner `AuditionSourceSelector::set_gold/clear_gold`. With both
   optional Product gates installed, one user click would advance two
   independent epochs. A new read-only selector guard-presence witness
   allows GOLD ViewModel to fail closed if *both* are installed; when
   selector-only is installed the selector alone admits the real
   mutation; when GOLD-only is installed the ViewModel alone admits it.
   A scoped owner-thread GOLD publication bit covers nested selector
   calls, file metadata and synchronous Qt `changed()` signals,
   denying callback reentry and wrong-thread methods before any
   secondary Product mutation. Qt native regressions prove two-policy
   ambiguity, precisely one admission for selected GOLD/clear,
   reentrant Qt notifications, throwing and reentrant callback
   recovery, and unchanged legacy M17 with no gates installed.

The new code is only a guarded preproduction seam; no Product GUI M18
registration, new exporter activation, physical PCM24/Float32 gate, FQ,
main merge or proof that the entire single-writer orchestration is
ready. The current PR candidate and current CI remain unchanged.

### Product Owner clarification — ONE shared Full Qualification AFTER integration M18–M22 (2026-11-06)
The Product Owner reconfirmed the cross-milestone acceptance plan:
**DO NOT perform or require a separate Full Qualification for M18,
M19, M20, or M21 (or a separate one for M22 before the full set).**
Finish each milestone in strict dependency order, promote controlled
macrolots via focused Windows/Linux CI, and pass relevant targeted
module/integration regressions, but reserve the comprehensive Windows
Full Qualification, physical application tests and overall Product
acceptance for **AFTER M18, M19, M20, M21 and M22 have ALL been
integrated**. The prior scattered wording "M18 Full Qualification
before main merge" in historical documents is superseded by this
explicit PO decision. Focused green is only the intermediate technical
integration gate and must not be described as a completed Full
Qualification. Never merge to main without explicit PO authorization;
do not remove or waive relevant interim safety and cross-module CI tests.
This policy does not activate the M18 exporter before the shared
Single-Writer Product epoch has been wired and tested.

### Chat #72 — #1070 GREEN and focused #1071: first shared Product epoch fanout (PREPRODUCTION)
The Product Owner reported #1070 GREEN. Both Windows and Linux
`SUCCESS` were verified on PR #84 `1ff2915d`. The following
16-commit macrolot was promoted by exact-SHA fast-forward to Draft
PR #84 `4751b9d1420c9ed2a4b9e00346d4dd6b8247d01c`;
[Focused #1071](https://github.com/casaferla/RGS-MasterLab/actions/runs/42301660916)
started on that exact SHA. Main is unchanged. The PO reconfirmed a
**single final Full Qualification after M18–M22 are all integrated**:
intermediate macrolots require Focused Windows/Linux and module checks,
NOT independent milestone-wide FQ.

The NEW isolated staging introduces a real, typed, Qt-free common
`M18OwnerMutationFanout` in
`platform/windows/src/internal/m18_owner_mutation_fanout.hpp`.
Each copyable controller callback contains a weak reference to ONE
Product-thread state; a successful edit advances the same
`M18ProductEpochTicketAuthority`, increments a monotonic conservative
Master generation and REVOKES consent before observable mutation.
A held worker commit lease refuses edits from every controller; a
foreign owner, expired binder, invalid operation, stale or exhausted
epoch fail closed. This fanout **DOES NOT MINT any export ticket**:
the real Product must separately refresh actual SOURCE/Project/
MASTER/chain identity and obtain exact checked M17 owner and explicit
consent. This protects against falsely treating a provisional revision
as a newly verified export.

The private `M18ProductControllerBinding` provides type-safe enrollment
of actual SOURCE + MasteringChain + MasterOutputRate, real
AuditionSourceSelector + Region + GOLD (one selector gate, not two),
and MasteringChain + TerminalLimiterState. Tests now exercise
real GOLD selection, real Region writes, real M11–M15 chain and limiter
edits against the SAME actual worker commit lease. The binding is
**not called by app/src/main.cpp** or QML.

The new exclusive `project_save_batch()` wraps the already-existing
Product authority's real `try_begin_product_mutation` and returns the
opaque RAII lease required by `ProjectSessionViewModel`: one batch
spans canonical .rgsml SaveAs snapshot, create-new, readback and Qt
publication. Real ProjectSession integration test verifies worker-commit
BUSY -> no output, subsequent successful SaveAs -> file exists,
old ticket revoked, and Open Project denied in a SAVE-only configuration.
This is a deliberate intermediate policy: Open Project has nested
SOURCE/MASTER/chain/GOLD mutations which must share one separately
authorized Project-open batch, without individual epoch advances.

CRITICAL REMAINING PRODUCT P0:
1. Provide an atomic Project-open nested owner transaction (no
   unrelated Qt reentrancy) and the exact current source/project/M17
   generation capture/reconciliation after it completes.
2. Enroll full Product session cancellations, candidate-gate
   `invalidate()` legacy void call sites, M17 retry/return paths and
   Source/MASTER rate dispatch into that one barrier; never allow
   unsafe direct invalidation while a worker holds a commit lease.
3. Establish verified M17 stage + physical SOURCE identity + selected
   output schema/PCM24 seed + revocable explicit end-user consent
   before minting any new worker ticket or showing Export.
4. Cross-qualify actual Windows async Product GUI, cancellation,
   progress, fault/rollback, decoded post-quant M16 TP, physical audio
   and RF64 under the **common post-M22 Full Qualification**.

NO main merge, NO M18 GUI Export activation, NO FQ and no second
Focused CI on this staging. Its code/tests are not yet compiled on
Windows/Linux; await the PO's #1071 terminal result before promotion.

### Chat #72 — #1071 green; #1072 candidate; P0 legacy mutation bypass closures
PO reported [#1071](https://github.com/casaferla/RGS-MasterLab/actions/runs/42301660916)
green; GitHub verified BOTH Windows and Linux SUCCESS on exact SHA
`4751b9d1420c9ed2a4b9e00346d4dd6b8247d01c`.
Fifteen staging commits fast-forwarded Draft PR #84 to
`83e096101086593708dc44b01434b502a1ed8778`, and one new
Focused [#1072](https://github.com/casaferla/RGS-MasterLab/actions/runs/42306402500)
started on that exact SHA. No main merge, no Full Qualification.

The subsequent NO-CI staging repairs two genuine remaining Product
mutation bypasses. Both are opt-in so M17 remains unchanged until a
shared M18 epoch is installed:

1. `MasterOutputRateState::source_committed` and
   `source_cleared` are historical void API mutations. Previously,
   installing the M18 rate callback would NOT affect these paths:
   a SOURCE replacement could silently increment `rateRevision_`
   and alter the selected MASTER rate even while a worker held a
   commit lease. With a gate installed, BOTH historical void paths
   now fail closed without Qt signals/revision changes. The bool
   `try_source_committed_for_m18` and
   `try_source_cleared_for_m18` explicitly request one Product
   admission and then invoke private `*_unchecked` implementations.
   QtTest covers denied/throwing gates, same-rate SOURCE identity,
   old void bypass denial, true authorized SOURCE reset, QML NOTIFY
   counts, legacy behavior and source clear no-op.
   **Mandatory follow-up before activation:** migrate main/Project
   SOURCE dispatcher to the status API, so M17's old void caller
   never silently loses real SOURCE change notifications.

2. `TerminalCandidateGate::invalidate()` is also a legacy void
   mutator. It previously bypassed `try_invalidate_for_m18()`
   and discarded checked M17 PCM during a worker commit. When its
   M18 gate is installed, the void entry now fails closed, and the
   status path exclusively invokes the private `invalidate_unchecked`
   AFTER successful Product admission. Other stage mutations
   (`capture`, `accept`, `accept_measured`) now request the
   Product epoch BEFORE replacing an M17 candidate. Invalid or
   exhausted capture must authorize candidate retirement, or
   preserve a worker-owned checked stage; an already-admitted
   stale/nonconforming candidate is cleared exactly once without
   reentering another epoch. An existing true M17
   terminal-admission test now proves legacy invalidate, valid
   replacement and malformed replacement cannot remove the
   immutable checked stage while the worker holds the lease; an
   independently authorized retirement succeeds afterward.

   **Mandatory follow-up before activation:** M17 Product controller
   still calls `invalidate()` from legacy void cancellation and
   return/error paths. Every such call must be migrated to the
   status API and respect BUSY/defer, including the M17
   renderer's rejected completion and cancellation. Until then
   the default Product M18 export remains disabled.

None of these isolated changes has been compiled or CI qualified.
Do not promote before the PO's #1072 result; do not merge to main.
As repeatedly directed by PO, **one combined Full Qualification
AFTER integrated M18–M22**, not an M18-only FQ.

### Chat #72 — Focused #1072 double red, same missing Project include; portable binding correction
PO reported #1072 WINDOWS+LINUX FAILED. GitHub run
https://github.com/casaferla/RGS-MasterLab/actions/runs/42306402500
completed failure on exact PR SHA `83e096101086593708dc44b01434b502a1ed8778`.
Only bounded artifacts were inspected (Windows 595 bytes, Linux
573 bytes), **never any full GitHub logs**. Both platforms stopped
at `Build Focused DSP Targets`. Their diagnostics were:

- Windows: `app/src/project_session_view_model.hpp(3): fatal error
  C1083: Cannot open include file: 'rgsml/project/project_snapshot.hpp'`.
  Failed compiling `rgsml_audition_source_selector_tests`.
- Linux: identical missing `rgsml/project/project_snapshot.hpp`
  while compiling `rgsml_mastering_chain_state_tests`.

Root cause: the new inline
`app/src/internal/m18_product_controller_binding.hpp` imported
`project_session_view_model.hpp` even for independent DSP and Audition
tests that do not have the Project module public-header include
directory. This is a dependency-graph error in the new M18 binding,
**not** a DSP math/audio result and **not** an indication those tests
actually ran. The additional Windows M17 matrix report correctly
withheld a complete PASS because test executables were unavailable.

Corrective isolated staging:
1. Remove the ProjectSession include and SaveAs binding from the shared
   DSP/Audition/Source binder, keeping its typed common Product epoch
   fanout unchanged.
2. Create `app/src/internal/m18_product_project_save_binding.hpp`,
   which alone includes `project_session_view_model.hpp` and exposes
   `M18ProductProjectSaveBinding::bind_project_save_only`.
3. Update only `tests/integration/app/test_project_session.cpp`
   to use the Project-specific binding. Existing Project test target
   already has its own required Project module compile/link context.
4. Preserve Windows/Linux focused compile of actual DSP/Audition
   binder users, forcing a native include-boundary check on both OS.

The following isolated 17-commit SOURCE/master-rate and M17 candidate
gate epoch-bypass corrections remain in this NO-CI staging; they are
not claimed compiled. The #1072 SHA is red and cannot qualify this
newer candidate. Promoting this corrected macrolot requires a
distinct new Focused Windows/Linux run; still NO main merge or
Full Qualification until after integrated M18-M22 as PO directed.

### Chat #72 — after #1072 double-compile-red, #1073 dispatched; next SOURCE publication epoch staging
Focused #1072 compiled neither platform's new controller tests due to
the new shared binding's accidental `project_session_view_model.hpp`
dependency; the precise bounded diagnostics and split-Project fix are
recorded above. Following the 21-commit exact-SHA promotion, Focused
[#1073](https://github.com/casaferla/RGS-MasterLab/actions/runs/42309556526)
was started on Draft PR #84 `2b952c182eef7a40ad25b01eadffba7148d8594d`.
It must be checked by PO. No other CI triggered; no main merge.

The NEXT, fully isolated NO-CI macrolot addresses a separate direct
SOURCE Product ownership bypass:
- `SourceSelectionViewModel::adopt_probed_source` formerly admitted
  its Product epoch only at the first call, then changed SOURCE
  metadata, invoked a synchronous application-wide
  `sourceCommittedHandler_`, reconfigured transport and emitted
  `sourceChanged/sourceStateChanged/errorChanged`. A reentrant Qt
  callback could start another SOURCE/Project adoption and advance a
  second epoch before the first publication had completed.
- When the opt-in M18 gate is installed, a scoped RAII SOURCE
  publication flag now remains held through that full sequence.
  Direct nested `adopt_probed_source` fails with InvalidState;
  QML picker/Qt `selectSource` silently refuses nested publication
  and foreign-owner-thread calls before probing or emitting errors.
  The Product admission callback itself is non-reentrant with a
  separate throwing-safe guard, and cannot uninstall its current
  authority while executing.
- SOURCE transport and committed-handler setters are also guarded:
  no foreign-thread or in-flight Product write can replace the
  transport or destroy the currently invoked `std::function`.
  Ordinary M17 behavior without an installed M18 callback is
  unchanged.
- Windows native `rgsml_source_selection_tests` now includes
  callback self-uninstall attempts, recursive SOURCE picker calls,
  nested direct Project SOURCE adoption, synchronous SOURCE Qt
  changed listeners, foreign-thread direct probed-source adoption,
  and throwing callback recovery. No Windows-native SOURCE target
  was added to portable Linux (cf. #1067 failure).

IMPORTANT: This is NOT sufficient for the full M18 Product export
gate. `sourceCommittedHandler_` remains a legacy void cascade;
physical SOURCE metadata is written before that cascade finishes,
and downstream failures/exception may not be rollback-atomic.
To safely activate M18, implement an exclusive nested SOURCE/Project
batch that covers SOURCE, rate, M17 candidate, GOLD/Region and M11-15
without unhandled partial publication; bind/validate exact current
verified M17 Master and explicit user consent. This staging is
**UNCOMPILED**, does not enable Export or QML, and requires a later
exact-SHA Focused Windows/Linux gate before promotion. Still one
final Full Qualification after M18–M22, not per milestone.

### Chat #72 — Focused #1073 Windows red / Linux green: ProjectSession executable without CTest stdout
PO reported #1073 Windows FAILED. Verified on GitHub
https://github.com/casaferla/RGS-MasterLab/actions/runs/42309556526
at PR #84 SHA `2b952c182eef7a40ad25b01eadffba7148d8594d`:
Linux Portable DSP Guard SUCCESS; Windows focused **compilation SUCCESS**
but `Run Focused DSP Tests (Release)` FAILED. Downloaded ONLY bounded
diagnostic artifacts (NOT GitHub job logs). `rgsml_project_session_tests`
was the sole named failed test: CTest showed command start, **0.39 sec**
duration, `<end of output>`, `Test Failed`, without process exit code
or QtTest method. Thus no reliable assertion-level diagnosis exists:
could be Windows process launch/DLL status, early exception or abort.
Do NOT call it a confirmed product-code regression or hide the test.

Bounded positive evidence from the same run:
- Direct M18 Windows: 24 PASS, 0 FAIL, process exit 0.
- M17 terminal activation: 22 PASS, 0 FAIL, process exit 0.
- Device SRC: 47 PASS, 0 FAIL, 6 skipped, exit 0.
- M17 adversarial matrix: Windows Product PASS, direct grid PASS.
- Linux Focused: SUCCESS.

A temporary diagnostic-only Windows workflow step was added
immediately before normal Focused CTest: invoke the **specific**
ProjectSession test binary with `-v1 -o <runner temp>,txt`, record
the signed native exit code or process launch exception, and upload
only the bounded 16-line tail for raw QtTest, redirected stdout/stderr.
No full QtTest, CTest or GitHub logs are uploaded. The probe deliberately
does NOT hide a failing normal CTest and does not waive mandatory tests.
Its output can distinguish a 0xC0000135 runtime loader failure from
an actual named QtTest assertion or crash. Remove/reduce the temporary
probe once root cause is proven. The following 17 commits of standalone
M18 SOURCE full-publication Single-Writer guards remain in staging and
are UNCOMPILED. A next exact-SHA Focused Windows/Linux run, NOT a
Full Qualification, is required after promoting the corrected staging.
Still no merge before explicit Product Owner approval and one shared
FQ only after integrated M18–M22.

### Chat #72 — next isolated M18 Project Open owner batch after #1074 dispatch
Current focused gate is
[#1074](https://github.com/casaferla/RGS-MasterLab/actions/runs/42348334420)
on PR #84 SHA `f5ccbb6d59e12703698172af2485e49e46e7279e`.
The PO is checking its outcome. The newer NO-CI staging below
has NOT been compiled and must not be promoted before the gate verdict.

This isolated macrolot implements the first real *private* typed
`M18OwnerMutationFanout::project_open_batch()`:
- One Product-owner-thread `try_begin_product_mutation` revokes
  consent and obtains an exclusive RAII lease before any Project
  SOURCE/Master/Gold/chain replacement. The opaque token owns BOTH
  the underlying M18 commit-barrier lease and the nested-child phase;
  release is fail-closed and cannot revive an obsolete M17 ticket.
- The already-existing Product status callbacks now permit **only
  explicitly listed Project-open child operation tags** within that
  exact held batch (SOURCE adoption/selector, Gold, Region/Loop, MASTER
  rate and M17 candidate retirement), without advancing a second
  epoch. Unrelated writes, absent tags, nested Project Open/SaveAs,
  retired Product owner and foreign threads are rejected. This
  allowlist is necessary but NOT sufficient to prove no malicious Qt
  reentry: a future consolidated Product command router must also
  identify the actual nested caller and ensure its scope is correctly
  guarded.
- The Project-specific binder now offers
  `bind_project_open_and_save` for both Project callbacks, distinct
  from the portable DSP/Audition binder. This avoids the #1072
  missing-Project-header regression.
- `ProjectSessionViewModel` uses
  `MasterOutputRateState::try_source_committed_for_m18` when a
  Product M18 rate gate is present instead of the obsolete `void`
  `source_committed` path, avoiding a silent MASTER rate mismatch
  after a project SOURCE restore. When M18 gate is absent, legacy M17
  rate behavior is unchanged.
- The M17 terminal candidate receives a separate typed Product
  binder so that neither Project nor Audition portable tests import
  unrelated platform headers. Windows native tests check a genuine
  pending commit lease denies candidate invalidation; after release,
  the obsolete ticket remains revoked.
- Qt-free Windows M18 tests prove worker-busy denial, a single
  Open Project lease across allowed child callbacks, nested/unknown
  operation denial, worker-thread denial, retirement of controller
  callbacks while a token remains alive, and no restoration of
  export consent after an aborted operation.

IMPORTANT REMAINING P0:
* Main.cpp SOURCE handler still calls legacy void MASTER rate reset;
  it must move to a coordinated status-returning SOURCE/Open batch
  before enabling the Product-wide M18 gate. Open's actual callbacks
  and display publishers must be wired and tested with the real
  Source/Gold/Rate/Region/Chain data; SOURCE's historical committed
  handler is still void and can partially publish before an error.
* All M17 render candidate cancellation/return/completion paths must
  acknowledge BUSY and use `try_invalidate_for_m18` rather than
  bypassing/discarding the current stage.
* Actual verified-M17 identity, user consent, asynchronous exporter
  lifecycle/progress and export GUI remain OFF; no Product export
  ticket is minted by this new Project adapter.
* No Full Qualification until all M18–M22 are integrated, per PO.
  Never merge to main without PO authorization.

### Chat #72 — #1074 Windows project-test two-assertion failure and bounded probe correction
PO reported #1074 Windows FAILED. GitHub verified
[Focused #1074](https://github.com/casaferla/RGS-MasterLab/actions/runs/42348334420)
on exact SHA `f5ccbb6d59e12703698172af2485e49e46e7279e`:
Linux SUCCESS, Windows FAILURE in the *diagnostic ProjectSession probe*,
which erroneously caused subsequent mandatory Focused CTest to be
SKIPPED. Windows **compilation succeeded**, and bounded artifacts
reported direct M18 QtTests 24 PASS/0 FAIL and M17 terminal activation
22 PASS/0 FAIL.

The new ProjectSession bounded diagnostic resolves the prior mystery:
the executable DID launch successfully and reported
`PROCESS_EXIT_CODE=2`; 29 tests PASS and **2 tests FAIL** in 375 ms.
The first probe only preserved its last 16 QtTest lines (all PASS)
rather than FAIL assertions, so no exact failed-method attribution is
yet available. Do not falsely claim runtime loader/DLL failure or
declare either application behavior correct before the detailed check.

The staging workflow fixes the diagnostic-only probe:
- collect only the `FAIL!` / `QFATAL` line and at most five following
  assertion-context lines (maximum eight bounded matches), plus the
  existing last-16-line tail, no raw QtTest/CTest/job logs;
- force the probe step to complete with `exit 0` after recording the
  REAL native exit code, so the mandatory Windows Focused CTest
  executes and owns the actual qualification gate. A failing
  ProjectSession test will still make CTest red.

One definite semantic mismatch was identified independently:
`ProjectSessionViewModel::openProject` now correctly routes SOURCE rate
restoration through `try_source_committed_for_m18` inside the exclusive
Project-open batch, rather than silently ignoring the M18 gate using
legacy `void source_committed`. The existing regression
`m18ProjectOpenBatchKeepsWorkerLockAcrossNestedSourceCommit` expected
one rate callback, which counted only MASTER rate selection. It must
now expect **two separate child admissions under ONE physical Project
mutation lease** (SOURCE rate and selected MASTER rate). This
assertion was updated, maintaining the strict one-batch/one-lease and
readback checks. Since the previous probe did not disclose failures'
names, do not assume this accounts for both red assertions.

This new staging remains UNCOMPILED. Promote as one coherent macrolot
for exactly one subsequent Focused Windows+Linux CI, then inspect only
bounded failure artifacts if Windows remains red. Keep Draft PR #84
and main distinct; no merge or Full Qualification (PO policy: FQ once
after M18–M22 integrated).

### Chat #72 — #1075 Focused in-flight; next isolated actual SOURCE full-batch integration
PO reported #1074 Windows red; GitHub verified Linux SUCCESS, Windows
ProjectSession native process exit 2 with **29 PASS / 2 FAIL**. Earlier
probe printed only the last 16 passing QtTest lines, so exact failed
methods were UNKNOWN; no blind assertion weakening. The diagnostic
was upgraded to print ONLY FAIL/QFATAL lines with six bounded lines of
context, and changed to a non-blocking step so normal focused CTest
is always attempted. A separate Project Open regression was updated:
real source-rate restoration AND target-rate selection are two child
callback admissions under ONE held Project mutation epoch. The 20
commits were fast-forwarded to Draft PR #84
`88f931bf02cf919f42db45908b25e98c3dc585cf` and exactly one new
Focused [#1075](https://github.com/casaferla/RGS-MasterLab/actions/runs/42350124424)
started on that SHA. No main merge or separate FQ.

The NEXT, uncompiled NO-CI staging closes a Product-level SOURCE
transaction gap beyond the callback-only guard:

- New `M18OwnerMutationFanout::source_commit_batch()` reserves a
  **physical exclusive** `M18EpochCommitBarrier::MutationLease`
  before SOURCE adoption, revoking export consent and increasing the
  product generation exactly once. It holds the lease through
  `SourceSelectionViewModel`'s source-committed handler cascade,
  transport preparation and all synchronous Qt changed/error signals.
  Worker file publication refuses the SOURCE batch (BUSY); conversely
  the active batch prevents any old export ticket from committing.
- `sourceCommitActive` is scoped by an RAII token and whitelists only
  the expected nested SOURCE callbacks (selector prepared/processed,
  audio-source handoff, GOLD retirement, terminal candidate retirement
  and MASTER rate updates). Unrelated DSP/GOLD replacement is denied.
  Source batch may not overlap Project SaveAs or another Project Open.
- Project Open already owns a wider exclusive mutation token. When
  SourceSelectionVM is invoked *inside that operation*, its batch
  adapter returns a non-null shared lifetime witness of the same
  Product state (no independent Product generation), relying on the
  still-held Project lease to exclude workers throughout the child.
- A new opt-in `SourceSelectionViewModel::M18SourceBatchBegin` is
  mutually exclusive with the previous status-only callback. With
  it installed, SOURCE adoption fails closed if no lease is returned,
  a throwing callback fires, a worker invokes Qt, or a reentrant
  callback attempts to commit again. The exact lease stays alive
  until after Qt publication. Transport/callback setter and file picker
  guard both opt-in modes. **Legacy M17 with no gates is unchanged.**
- The typed actual SOURCE binder now uses the full physical batch
  instead of a one-shot status callback. Native Project integration
  tests drive real Source picker/selector and real Project
  Open+SaveAs through the same Product authority under a simulated
  worker commit. Real Project recovery verifies source+GOLD+MASTER
  192 kHz state, rate revision, and output file readback; M18 backend
  tests cover nested SOURCE under Project Open, physical worker BUSY,
  denial of unknown nested edits, and independent next-owner action.
- Separate SOURCE QtTests guard dual status+batch policy ambiguity,
  denied physical lease, subsequent success and exact error clearing.

BLOCKERS REMAIN: main.cpp still uses a void SOURCE committed callback
with unguarded downstream rate/telemetry/activation paths. It must be
converted to one status-bearing, fail-closed cross-controller SOURCE
transaction before any Product M18 Export UI/ticket can be enabled.
Similarly, M17 terminal cancellation/return routes must honor BUSY,
and complete verified-M17 actual source/project identity + explicit
user consent and physical delivery/rollback must be wired and tested.
No unqualified inference from this isolated preproduction branch.
Focused CIs per macrolot only, and one Full Qualification AFTER
integrated M18–M22 per the Product Owner.

### Chat #72 — #1075 Windows red: one obsolete master-rate denial assertion, corrected without weakening Product epoch
PO reported Windows RED on [Focused #1075](https://github.com/casaferla/RGS-MasterLab/actions/runs/42350124424)
SHA `88f931bf02cf919f42db45908b25e98c3dc585cf`.
GitHub verifies Linux SUCCESS, Windows compilation and all direct
M18/M17 tests SUCCESS, but mandatory Windows CTest red:
`rgsml_project_session_tests`. **Only bounded artifacts inspected.**
New fail-only ProjectSession probe identifies:
`ProjectSessionTest::master_rate_save_open_and_invalid_atomic`,
`tests/integration/app/test_project_session.cpp` (original #1075
line 917); actual `deniedMaster.source_rate_hz()==0`, test expected
44100. QtTest 30 PASS/1 FAIL, exit 1, 414ms. The source is correct:
when the rate Product gate is deliberately BUSY, the new
`try_source_committed_for_m18` refuses the initial SOURCE rate
adoption BEFORE the persisted MASTER-rate selection. Former 44.1k
fixture expectation depended on the legacy M17 void setter bypass.
Staging test is corrected to assert SOURCE rate 0, MASTER rate 0 and
`rate_revision()==0` after the denied operation, still allowing
ordinary subsequent retry after the external gate is uninstalled.
No production guard is relaxed; no test skipped or suppressed.

The next uncompiled M18 macrolot also includes physical SOURCE commit
batch, real Project Open/SaveAs same-epoch integration, actual Qt
regressions and export worker collision tests. A further nested
SOURCE batch-begin callback hole was closed: a scoped admission flag
now exists BEFORE an injected physical-batch callback returns its
lease. Recursive SOURCE selection, attempted callback self-removal,
and foreign-thread Qt mutation are denied from that point. Native
`rgsml_source_selection_tests` attempts a nested picker and callback
self-removal inside its batch-begin callback, then verifies exactly
one successful outer SOURCE publication.

The exact SHA is still staging-only and UNCOMPILED. PO has reported
Linux green for #1075. Promote a single 15–25-commit candidate for
Focused Windows+Linux qualification and wait for PO verdict; no merge
in main. ONE combined Full Qualification only after M18–M22.

### Chat #72 — #1075 single Windows failed assertion fixed, #1076 started, next checked Product SOURCE dispatcher staging
After PO's Windows-red report for Focused #1075, the bounded
ProjectSession diagnostic isolated ONE failing test out of 31:
`master_rate_save_open_and_invalid_atomic` incorrectly expected the
SOURCE rate to remain 44.1 kHz after an M18 rate BUSY denial.
The prior unsafe legacy void setter had set 44.1 kHz even when the
controller refused the selected MASTER rate. New explicit
`try_source_committed_for_m18` correctly leaves both SOURCE and MASTER
rate unset, revision 0; the test asserts this fail-closed invariant.
The PO's #1075 Linux gate was verified SUCCESS, Windows direct M18
26/26 PASS and M17 limiter 22/22 PASS. One coherent 24-commit
fast-forward promoted Draft PR #84 to
`e1be47cc22c060b117b1628dac30c2608713cc40`.
[Focused #1076](https://github.com/casaferla/RGS-MasterLab/actions/runs/42351825360)
was triggered on that SHA. The PO checks Windows/Linux; no next CI
may start before its verdict.

NEXT isolated NO-CI macrolot: **status-bearing real SOURCE Product
dispatcher**, with M17 behavior untouched until export activation.
- A physical M18 SOURCE batch now requires a separate
  `M18CheckedSourceCommittedHandler` before it can acquire/revoke an
  epoch; the old `void SourceCommittedHandler` can no longer claim
  that rate/selector/handoff mutations succeeded just because it
  returned without reporting a status. Status-only M18 admission and
  legacy unguarded M17 remain on their existing void callback.
- The checked callback runs WITH the physical lease still held. A
  returned failure or thrown exception prevents false SOURCE_READY
  publication and clears the provisional source/format metadata.
  Transport is asked to stop/clear, source Qt state is invalidated,
  and the Product authority's withdrawn consent is **never restored**
  by an early error or retry. The explicit user retry may only
  continue on a new lease.
- A typed setter refuses changes to the checked std::function while
  an M18 callback is executing or Qt publication is in progress.
  Qt native tests cover missing checked dispatcher, legacy M17
  inertness, physical lease BEGIN reentrancy, error/throw recovery
  and stale READY withdrawal. The real ProjectSession fixture
  installs a checked callback that actually returns results from
  MasterOutputRateState and AuditionSourceSelector under a single
  shared epoch, including real Project Open error + subsequent retry.
- `app/src/main.cpp` now PREPARES the real checked SOURCE callback
  using all the actual SOURCE waveform, MASTER rate, audition/GR,
  M17 device, GOLD and stereo controls, returning explicit failure
  on denied Master rate or prepared/audition handoff. This setter is
  **inert for M17**: main.cpp still does NOT install the shared M18
  physical batch, export ticket, async output control or QML
  Export button. Existing legacy void SOURCE callback remains
  unchanged and continues serving M17 until complete Product
  activation. The new checked dispatcher is NOT proof that every
  downstream UI side effect has atomic rollback; failed callbacks
  are fail-closed but may require a further clean SOURCE selection.

CRITICAL REMAINING BEFORE M18 ACTIVATION: full Product registration
of a single authority across main.cpp, M17 terminal candidate
invalidation/cancellation, session open/save operations and
export/consent identity; verified PCM output owner, PCM24 Float32
format/dither seed selection, worker lifecycle/progress and Qt UI.
Do not enable Product export until the end-to-end M17 identity and
Single-Writer integration are verified on Windows/Linux.
No main merge without explicit PO authorization. ONE combined FQ
after integrated M18–M22, not per individual milestone.

### Chat #72 — #1076 Windows compile red, Linux green; single MSVC test typo
PO reported #1076 red. GitHub verified
[Focused #1076](https://github.com/casaferla/RGS-MasterLab/actions/runs/42351825360)
on Draft PR #84 SHA `e1be47cc22c060b117b1628dac30c2608713cc40`:
Linux Portable DSP Guard SUCCESS; Windows Focused DSP FAILED at
**Build Focused DSP Targets (Release) — Ninja + sccache**. No focused
Windows test results are qualified by that run. Only tiny bounded
compile/test artifacts were inspected, **not full GitHub logs**.

Windows compiler summary (one error):
`tests/integration/platform/test_m18_candidate_delivery_validation.cpp(504):
error C2039: 'value' is not a member of
'std::function<core::Result<std::shared_ptr<const void>>(void)>'`.
The test `sourceCommitBatchReusesSingleEpochAndCannotRaceExportWorker`
assigned the Project-open callback *factory* via
`auto next=fanout.project_open_batch();`, then called
`next.value()` as if it were the returned Result. Corrected isolated
staging to `auto next=fanout.project_open_batch()();` so the factory
is invoked and the returned Result owns the RAII token before
`next.value()->reset()`. No production SOURCE, rendering, audio,
authority or FQ gating was weakened for this compiler fix.

The next staging macrolot also includes the **still uncompiled**
private real Product checked SOURCE dispatcher in main.cpp, replacing
void-only callbacks only when an M18 physical SOURCE batch is
installed; default M17 remains legacy and unchanged. The real
test target must compile this full candidate on Windows and Linux
before claiming success. No main merge; NO individual M18 FQ.
One combined Full Qualification after M18–M22 integration.
