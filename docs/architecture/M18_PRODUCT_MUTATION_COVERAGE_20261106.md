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
