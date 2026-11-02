# RGS MasterLab — OPERATING CONTRACT & HANDOFF BOOTSTRAP

## 2026-11-02 — DIRECT 14-BINARY SOURCE FILES NOW SUPERSEDE THE PENDING SOURCE ZIP IMPORT
All fourteen authenticated original Q0 FIRs are normal Git-tracked `resources/fir/RGSMSR1_*.bin` dependencies on no-CI staging, with exact frozen SHA-256 checks; CMake and the exporter stage them alongside the two already frozen Playback kernels under ONE installed `resources/fir` root. This replaces the earlier proposal to put the historical 11.45-MB candidate ZIP in the public repository. The historical archive is preserved in RGS ContentBox as immutable provenance, not part of the release installation. PO permission to distribute all 16 still does NOT authorize Q1–Q6 DEVICE qualification, physical acceptance, cold Windows FQ launch, main merge or M18. This staging change awaits new Focused dual-OS CI before PO may dispatch FQ.


## 2026-11-02 — LATEST PO DECISION: ALL 16 FIR COEFFICIENT FILES ARE DISTRIBUTABLE PUBLIC DEPENDENCIES
The Product Owner explicitly APPROVED publishing/distributing the 14 original mathematically frozen RGSMSR1 FIR coefficient binaries alongside the existing 2 frozen Playback FIR files. They are ordinary software dependencies. The prior privacy/nonrelease distribution prohibition in this repository is SUPERSEDED **solely for public source bytes and packaging**; Q0 immutable SHA/geometry authentication, SRC functional DEVICE-route qualification, Q1–Q6 status, M17 physical acceptance, FQ user authority and CI integrity gates remain binding. Do NOT infer Q1–Q6 product admission from file distribution authorization. The desired artifact is ONE RGS MasterLab Windows ZIP with 16 binary files on the SAME LEVEL under `resources/fir` with unified manifest/loader/installer integrity controls, no separate ZIP/verifier/environment/path registration or disclosure of historical source development phases to the normal end user. The two Playback kernels and 14 Final SRC kernels retain their respective independent DSP semantics and are not mathematical substitutes. Keep bit-identical approved files and full SHA verification; no regeneration.
**Actual packaging is NOT YET COMPLETE**: the existing GitHub code/CI distributes 2 Playback binaries but lists 14 pinned original files only in the manifest, and original 14 were manually provisioned on PO PC. Retrieved original archive in RGS ContentBox: `RGS_Phase11_14_Q0_candidate_manifest_and_assets_NONRELEASE_2026-10-30.zip`, Google Drive file ID `1pbmZqDNefH-F_RiNZEdP9z_BC3seUGsh`, exact ZIP SHA-256 `94d3340667ab5c86b2db5f2d0e191f58b5d05863413312fb3545faed51798ce4`; inventory 14 `RGSMSR1_L*_M*.bin`, 11,440,952 FIR binary bytes, plus source candidate manifest whose release terminology is historical. SHA pins live in `phase11_frozen_rgsmsr1_pins.hpp`. The authenticated Drive archive is NOT publicly accessible anonymously by default, and one attempt to transfer its 11-MB bytes via GitHub connector blob write failed at HTTP/2 body size; do not claim that repo/CI possesses it. Need ONE verified source artifact injected into the public build infrastructure, then CMake/CI normal deterministic 16-file deploy, fail-closed missing assets and a real FQ ZIP proof. Never claim 16-FIR bundle shipped until verified from an actual artifact. PO will not repeatedly install original FIRs on every FQ.
**Also PO previous UI decision:** REMOVE separate `Master Loop` popup and button; reuse always-present SOURCE Region Start/End, Loop Region and Clear Region controls to edit independent MASTER listening coordinates during exclusive Master DEVICE audition. Do not modify the SOURCE region state in that mode, do not cross-apply any loop to a different immutable source/M17 owner. Qt native tests and real Windows FQ remain required. Freeze the previously approved DSP icon glow.


## 2026-11-01 — PO NEXT WINDOWS FQ: REUSE PREVIOUSLY VERIFIED LOCAL ORIGINAL-Q0 FIR BANK WITHOUT REINSTALLING

For the PO's NEXT cold Windows M17 FQ, do **not** ask the PO to re-extract/reinstall the 14 original RGSMSR1 Q0 binaries with `install_private_q0_originals.py` if their existing local authenticated 14-file flat directory remains intact. **Existing current executable behavior already supports reuse**, not repeated installation: `app/src/main.cpp` chooses the explicit `RGSML_FINAL_SRC_Q0_PRIVATE_DIR` root when configured; otherwise it uses the current executable-relative `resources/fir`. `TerminalPrivateRenderController::set_original_q0_fir_root` verifies all 14 immutable SHA/geometry/symmetry pins on each app start and re-authenticates the actual requested original FIR at each cross-rate render; this re-verification is inexpensive compared with MPFR certification and MUST NOT be skipped. The two public immutable Playback FIR binaries continue to come from the new executable's own `resources/fir` regardless of the private Q0 override.

**One-time persistent reuse for new FQ extraction folders:** if the already-verified private 14-file directory is NOT inside the new executable's `resources/fir`, point the user-level Windows environment variable `RGSML_FINAL_SRC_Q0_PRIVATE_DIR` once at the EXISTING folder containing the 14 `RGSMSR1_L*_M*.bin` files directly (not their parent). Subsequently launch FQ builds from fresh Windows processes that inherited the updated environment; users do not rerun Python/verifier or copy FIRs into each public artifact. If neither the new executable's own resources nor an explicit existing private root contain the genuine full set, show missing/unavailable cross-rate status and ask only to identify the saved original local directory: never download silently, scan random folders, generate approximations, bypass pins, or publish the private coefficients. Avoid setting the override to obsolete partial Playback-only directories; do not use or modify System32. Keep an exact SHA FQ/artifact and real physical audio qualification distinct from the original FIR identity validation. This is an operational reuse method with existing code, not an authority to redistribute original Q0 binaries or claim that an unrelated past FQ certifies the new executable.

## 2026-11-01 — NEW PO MILESTONE: POST-M17 CI AUDIT / RESET BEFORE M18 (MANDATORY)

Product Owner requires an explicit **CI architecture review/reset immediately AFTER M17's functional and physical acceptance, BEFORE starting M18 Dither**. Current Windows+Linux Focused runs are too broad/slow: collect measured per-job and per-step wall times, cache hit rates and real failure evidence from recent #957/#958 and accepted follow-on; identify duplicate test execution and repeated environment/toolchain installation; rationalize gates without deleting essential checks. Preserve mandatory per-PR fast focused Windows+Linux verification and all correctness/provenance gates for DSP, M16 True Peak, M17, exact Final SRC Q0 pins, 16 external FIR/PRIVATE exclusion, tests against unqualified paths, and installer/Windows artifact readiness. Partition appropriately among: (a) fast mandatory PR gate; (b) heavier scheduled/targeted/full qualification; (c) PO-only Windows cold FQ and physical listening. Cache Ninja + sccache, dependencies and generated immutable qualification evidence where safe; establish baseline/target run times and compare green coverage BEFORE disabling or moving any gate. "Reset" is a designed cleanup of workflow architecture and duplicated work, NOT a wipe of CI history, bypass of a red, false-green or relaxed qualification. Make one coherent CI-reset macro-lot on no-CI staging, promote with exact-SHA CAS, run Windows+Linux gate. This latest milestone ordering supersedes earlier "M17 immediately to M18" wording in timing only. M17 still must pass authentic FQ before CI reset; no premature M18, main merge or original private FIR publication.

## 2026-10-31 — FINAL PO PRIORITY SUPERSESSION: FIR macro-lot NOW before new FQ/M18

The Product Owner independently cancels the previously dispatched M17 FQ; do not falsely claim it already stopped without a live terminal status. To avoid qualifying two separate binaries, implement **all-16-FIR external migration on no-CI staging immediately**, bundling source-bit-preserving two-legacy-kernel export, version-pinned 14-private-Q0 root discovery, loader, shared installed resource manifest, Windows deploy, native negative/hash/bitwise tests and provenance docs into one macro-lot. Do not start a CI until the candidate is complete. Then exact-SHA CAS fast-forward the complete candidate ONCE into Draft PR #61, run Windows/Linux Focused, fix genuine red as one coherent follow-up. The PO then manually launches one replacement cold Windows FQ for the new executable and relevant private local source tests/physical listening. After acceptance proceed to M18 Dither. Earlier prerequisite old-FQ-first and immediate M17→M18 lines below are historical superseded ordering, not permission to waive qualification or publish private FIR. Public GitHub/release artifacts must not contain original 14-Q0 binary assets without separate PO authorization. GitHub/ContentBox snapshot with true SHA/CI state; no FQ/main merge without PO.

## 2026-10-31 — LATEST PO SUPERSESSION: FQ M17 → all-16 external FIR macro-lot → M18

This latest milestone ordering replaces every historical directive that M18 immediately follows acceptance of M17, but does **not** relax M17 functional/physical FQ, strict SRC/Q0 provenance, Single-Writer/CAS, or authorized FQ/main release gates. Product Owner requires: (1) M17 cold Windows FQ + actual private original 14-FIR-based physical acceptance; (2) **if successful, implement and qualify the full 16-FIR external resource/installer migration as a single cohesive macro-lot before M18**, including bit-exact migration of both frozen 44.1↔48 embedded playback FIRs, the 14 unchanged private Final SRC FIRs, authenticated resource root, versioned manifest, native regression tests, portable/installer deploy and targeted Windows artifact/audio validation; (3) then start M18 Dither. If first FQ is red/incomplete, fix M17 first. One complete staging lot → one Windows+Linux Focused CI, recovery CI only for real failures; separate PO-gated FQ for changed deployed artifact. Never publish private Q0 assets in public GitHub/CI without release authorization. Refer to [FIR_ASSET_UNIFICATION_ALL_EXTERNAL_20261031.md](FIR_ASSET_UNIFICATION_ALL_EXTERNAL_20261031.md). No implementation/FQ/merge authority implied by this note. The #942 two-OS green SHA remains immutable, staging docs-only updates are unqualified.

## 2026-10-31 — GitHub ↔ RGS ContentBox continuity without long chat handoffs

Product Owner confirms two archives remain current in complementary roles: GitHub the LIVE canonical source of code/CI/branch and short checkpoint, RGS ContentBox the frozen/private authoritative source and milestone snapshot. [Full synchronized workflow](PROJECT_ARCHIVE_SYNC_CONTRACT.md). At coherent macro-lot decision or terminal CI, update both with exact SHA/time/status and links; never mirror every commit, republish private FIR or redo a long historical recovery audit between chat windows. In every successor chat minimally read mandatory operational files, reconcile PR/staging SHAs and live CI, then continue. There is **no automatic background sync**; acknowledge any deferred/failed write. Owner's fast macro-lot Single Writer CAS and no FQ/main without GO are unchanged.

## 2026-10-31 — Product Owner high-throughput macro-lot process (MANDATORY)

**2026-10-31 PO OVERRIDE — PERMANENT LARGE COHERENT MACRO-LOTS:** Prefer large functional work packages containing all related code, QML, native regressions and documents on the no-CI staging branch. Intermediate checkpoint commits are allowed. PROMOTE ONCE by expected-SHA fast-forward to Draft PR, run ONE Windows+Linux Focused CI per substantial completed macro-lot (not per fix, file or tiny feature). While that immutable PR CI runs, continue authorized next-lot work on staging and poll job/step states within the active chat; do not demand repeated routine "Vai". On red, use bounded diagnostics, fix all related failures on staging and promote a single subsequent candidate after the prior run is terminal. On green, continue the next authorized macro-lot. Preserve single-writer epoch, exact SHA CAS, genuine SRC maths/provenance, SOURCE/MASTER/DEVICE guards, one Prepare, no private FIR assets on public GitHub, and full Windows/Linux CI. Never claim a staging SHA is covered by prior green CI. PO alone authorizes physical FQ and main merge. The current four workstreams are (1) MAIN Ascolta Master transport, (2) six-rate Prepare time/RAM, (3) canonical Phase3 Final SRC Q0 + applicable genuine 36-pair integration, (4) M17 Windows physical FQ readiness. #939 Windows+Linux PASS qualifies the first two workstreams for Focused CI, NOT physical FQ. Defer unimplemented Android/Reference Match gates; after PO-approved M17 proceed immediately to M18 Dither.

Inherited exact overlap instruction (copy verbatim to all later handoffs):

> RGS_OVERLAPPED_STAGING_PR_CI_ACTIVE_TURN_V1_20261025: nella sessione attiva, dopo Recovery Audit e una sola promozione fast-forward con SHA atteso, la CI sul branch PR Draft controlla uno SHA immutabile mentre la Chat Operativa CONTINUA a sviluppare sullo staging senza PR e senza CI. Durante la CI verifica periodicamente gli stati live di run/job/step, lavorando sui lotti successivi già autorizzati, e non richiede nuovi “Vai” ordinari. Solo quando la CI precedente è conclusa e il nuovo lotto coerente è pronto, verifica gli HEAD e promuove una sola volta il candidato successivo. Nessun full log GitHub, polling programmato/background, force push, merge main o FQ senza GO. Ogni successiva chat deve trasmettere letteralmente questa clausola a tutte le proprie successive chat, insieme a SOP RGS ContentBox, branch/HEAD/PR/CI verificati e differenza fra candidato verde e staging non qualificato.

> **2026-10-27 PO reactivated time calibrator BEFORE Prepare:** Read `docs/architecture/M17_MASTER_AUDITION_SINGLE_PREPARE_GATE.md` section "RECOVERED PO PERFORMANCE-TIME ADVISOR". This requirement was agreed 23 Oct in RGS ContentBox Master Output Rate §11, temporarily de-prioritized later 23 Oct on PO request, and **re-prioritized 27 Oct** for 96-vs-192 slow-PC decisions. Do not omit the historical supersession. Internal deterministic on-host tests + M17/M16 + true SRC, six-range pre-Prepare ETA when evidence exists, real Prepare observations feed aggregate per-host profile, 192 offline independent of DAC, single prepared MASTER consumed by Master Audition. Distinguish implemented prototype from missing persistence, six-rate UI and final SRC; no speculative ETA. This is one L1-M17/Phase11 gate before M18, not new unowned module. MUST propagate verbatim scope/status in next handoff.


> **2026-10-27 mandatory Product Owner supersession — P0 M17/Master Audition BEFORE M18:** Read `docs/architecture/M17_MASTER_AUDITION_SINGLE_PREPARE_GATE.md`. The approved 23/26 October design is ONE shared immutable MASTER PCM prepared by M17 at PROJECT TARGET_MASTER_OUTPUT_RATE, Master Audition reuses it, DEVICE_RATE SRC only on listening branch. The separate `Prepare MASTER` in FQ #83 and M17's device-probed rate remain NOT ACCEPTED. FQ #83 automatic green does not close physical audio Gate M/L. Every outgoing handoff must explicitly propagate this open P0, its original [Drive authority](https://docs.google.com/document/d/13fIk06CgiPLWcaW3KAZPCUoNFN5ffh6NHlVXK1sfA_8/edit), actual failed user scenario, code seams, and precedence before M18. PO only launches Full Qualification; do NOT start M18 or merge main.

**Authority:** Product Owner · **Version:** 2026-10-25 · **Status:** ACCEPTED / FROZEN for chat-operated development.
**Applies to:** all operational ChatGPT sessions, coding agents, GitHub edits, PR/CI handoffs and replacement chats.
**This is NOT an optional summary.** Product architecture decisions are governed by their own frozen documents.

## 0. Mandatory new-chat recovery (BEFORE any write)
1. Read `AGENTS.md`, this file, and `docs/architecture/ACTIVE_OPERATOR_STATE.md` **from the intended active branch**. If a file cannot be read, DO NOT write.
2. Independently inspect LIVE GitHub: staging ref + PR ref + PR number/status; last exact commit, focused CI result and failed **step summaries only**; concurrent writers and pending FQ. Check the branch has not moved since reading it.
3. Compare the handoff narrative with GitHub; the live ref is authoritative for code provenance, frozen PO decisions for behavior. Record inconsistencies in the reply and stop unsafe writes.
4. Identify pending regressions, completed/untested corrections, unimplemented approved decisions, allow-list and next cohesive work package. DO NOT quietly drop an item from the outgoing handoff.
5. State the SHA and CI hyperlink, then resume the approved task. No user 'Vai' is required when an ongoing mandate has already been given.
6. After an error or CI red, inspect small bounded test-failure artifacts or failed-step names. **NEVER request full GitHub job logs**: they hang this chat.

## 1. Single writer + branch topology — DO NOT INFER
- The single-writing assistant owns exactly one **STAGING** branch during an epoch. Current authoritative names/SHAs are in `ACTIVE_OPERATOR_STATE.md` and must be verified live.
- Intermediate work ALWAYS commits to `staging/*-no-ci-*`, a branch without an open PR. **NEVER push individual intermediate fixes to the open PR branch**.
- GitHub's `pull_request` trigger runs Focused CI on each PR-branch update. A 5-file/10-file coherent fix must be committed and tested as ONE promoted candidate, not 5–10 PR pushes.
- Commit frequency on staging is flexible (small checkpoint commits allowed); **CI frequency is per meaningful complete candidate**, not per file. Bundle mutually dependent fixes, tests, docs and assertions before promotion.
- Before writing: GitHub recovery audit, expected SHA check, single-writer lease, task allow-list, baseline & independent state. Update through CAS/expected old blob SHA; inspect conflicts rather than overwriting others.
- Promotion is fast-forward of the existing PR branch to the staging commit, with expected PR SHA, **only when the candidate and its tests are ready to run**. A PR promotion triggers one Focused CI. If staging has diverged or no safe fast-forward: stop and reconcile. Never force push, merge or write directly to main without explicit PO Gate M/L.
- On red: triage failed-step summaries and small filtered artifacts; batch fixes on staging; re-promote a new candidate; repeat until green. On green: immediately prepare and publish the **next authorized cohesive task**, without waiting for a separate permission in an active session.
- Do not create scheduled hourly CI checks/automations (prior monitoring interfered). Real-time continuation here is limited to the active interactive session; never imply autonomous offline/background execution.
- Never initiate Full Qualification. Only Product Owner dispatches FQ, and only after Focused CI gates and agreement.

## 2. Required CI gates, reporting and evidence
- On each coding response show one clickable exact CI run URL; give branch + commit SHA + green/red/running, distinguish automated PASS from the PO's physical Windows FQ.
- Windows focused + Linux portable are independently relevant. No passing result from a parent SHA certifies later commits.
- FQ and UI screenshots must prove the actual Main-window geometry at 1184×688 and Windows 150% scaling; isolated QML fixtures are supportive, not a substitute for live Windows checks.
- Preserve true-peak, sample-rate and ownership provenance: never confuse Windows DEVICE PCM with immutable MASTER PCM; never certify M16 LIVE on an unverified sink; never silently retarget rates or disable fail-closed guards just to turn CI green.
- Failed CI is a task to act on, not an endpoint. Include what is fixed, what is unverified, and which work package is next.

## 3. Cohesive batching before each promotion
1. Read PO corrections and pending defects; group related QML layout, tests and state handling. Avoid one PR push for each fix or each file.
2. Stage all related code + minimal regression tests + the required documents.
3. Re-read changed files, cross-check QML syntax, relevant tests, and the active SHA. Nothing in a prior handoff overrides a newer explicit PO decision.
4. Promote once, then run Focused CI; inspect errors; iterate on staging until green.
5. Maintain no-CI staging and accurate handoff state after EACH epoch.

## 4. Mandatory outgoing handoff
A new chat MUST receive:
- The one-line bootstrap command pointing to `AGENTS.md`, this contract, and `ACTIVE_OPERATOR_STATE.md`.
- Verified staging SHA, PR SHA, PR #, last CI exact URL/state, FQ SHA/status, branch direction/ancestor relation.
- Decisions **approved but not yet implemented** vs changes **implemented but not physically validated**.
- Open blocking bugs and no-regression invariants; next coherent work package.
- Tool-specific pitfalls, especially NO full GitHub logs and avoiding one-CI-per-file.
- Any requested file attachment. The chat must NOT rely solely on a transient sandbox file or memory summary.

## 5. Product and safety invariants (not superseded)
Windows 10/11 x64 primary; Android secondary; Apple excluded. C++/Qt/QML. Single DSP core/registry. RAW/PREPARED/PROCESSED/GOLD. SOURCE/GOLD immutable. Reference Match as controller, not secondary DSP engine. M17 and MONITOR audio ownership strictly guarded. M18 final output/dither must never change the monitoring SOURCE/Master rate by accident. Manual physical FQ is a separate PO gate.


## 6. OPERATIONAL_AUTONOMY_TOGGLES — PO approved 2026-10-25
Text toggles are reproducible instructions, **not** any native platform switch or continuous daemon. They apply to the active interactive turn only, preserving the Single-Writer Epoch, and may be reissued in a future chat:
```text
AUTONOMOUS_EXECUTION = ON
CI_MONITORING = ON
CI_MONITORING_METHOD = LIVE_POLLING_DURING_ACTIVE_TURN
POST_GREEN = CONTINUE_AUTHORIZED_WORK
POST_RED = DIAGNOSE_FIX_RETRY
RECOVERY_AUDIT = REQUIRED_BEFORE_WRITES
SINGLE_WRITER_EPOCH = STRICT
STAGING_INTERMEDIATE_WRITES = ON
STAGING_CI = OFF
PR_PROMOTION = ONE_FAST_FORWARD_PER_COMPLETE_BATCH
PR_PROMOTION_SHA_CHECK = REQUIRED
GITHUB_FULL_LOGS = FORBIDDEN
GITHUB_DIAGNOSTICS = JOBS_STEPS_SMALL_EXTRACTS
CI_LINK_IN_EVERY_OPERATIONAL_REPLY = ON
REAL_UI_SCREENSHOT = FIRST_ELIGIBLE_CI
MOCKUP_AS_TEST_EVIDENCE = FORBIDDEN
FULL_QUALIFICATION = PO_AUTHORIZATION_ONLY
MERGE_MAIN = PO_AUTHORIZATION_ONLY
```
Interpret `CI_MONITORING=OFF` as no repeated run polling in the current interactive turn (one final status query can still be used for safety). `AUTONOMOUS_EXECUTION=OFF` means stop after the current cohesive package and report, rather than starting another unrequested package. Neither permits bypassing frozen safety gates or queuing any automation; a ChatGPT run cannot observe GitHub once its active interactive execution has ended.


## 7. REQUIRED ACTIVE CI/STAGING OVERLAP — PO ACCEPTED 2026-10-25

**Permanent token:** `RGS_OVERLAPPED_STAGING_PR_CI_ACTIVE_TURN_V1_20261025`  
**RGS ContentBox / MASTER PLAN authoritative SOP addendum v1.4:** https://docs.google.com/document/d/1l3zD_CumZWwBzaPt790U3OaChDa3hvqV8HJCkSpLRz8/edit  
**Mandatory companion:** `RGS_MasterLab_Chat_Handoff_Template_v1.1` on RGS ContentBox; read its newest append-only block before authoring the next outgoing handoff.

This section supplements §§1, 3, 4 and 6, and makes their live-session concurrency semantics explicit. During ONE active ChatGPT operational turn:

1. **Stage development and PR CI are intentionally overlapped.** After the already authorized complete batch is fast-forward-promoted ONCE from PR-less, CI-silent staging to the open Draft PR using its exact verified expected_sha, GitHub Actions tests the immutable promoted SHA. **While #N is running, keep implementing / testing / documenting the next authorized batch #N+1 ONLY on the no-CI staging branch.** Do not pause productive work merely because the PR CI is in progress or ask for a routine "Vai".
2. Periodically inspect exact GitHub run, jobs and steps **during that active turn**. Work concretely on staging between polls. Avoid excessive polling, spam updates, full log downloads, background/scheduled task creation, or premature claims of PASS. Keep `TESTED_PR_SHA_N` distinct from `NEW_STAGING_SHA_N+1`. Never credit staging-only changes with the preceding CI result.
3. On CI red use failed-step names and bounded runner-filtered diagnostics only; correct on staging, then after the old run finishes and the coherent fix batch is complete, use a **new CAS-checked single FF promotion**. On green independently verify Windows/Linux and exact SHA, then continue already-authorized work and promote the next cohesive batch when ready. Never promote a new PR head while an earlier candidate CI is unfinished. Retain Single-Writer Epoch and verify current branch ancestry / competing writes before each promotion.
4. **Scope/limitations:** this capability depends on the chat turn actually remaining active; it does NOT mean that ChatGPT is a persistent background monitor, daemon or autonomous scheduled service. Historical 30–50 minute work sessions are observed behavior, not guaranteed runtime duration. No FQ, Gate G/L/M, freeze, or main merge without PO permission. No old instructions about hourly automations or reading entire Actions logs may override this.
5. **Handoff propagation is mandatory and recursive.** Each outgoing handoff must carry this token, the exact literal clause below, SOP link, both full branch names and independently verified LIVE HEAD SHAs, PR #/Draft state, last completed green SHA and exact CI URL + Windows/Linux outcome, running CI if any, staged but unqualified commits, writer epoch, next authorized lot, and PO-only gates. Instruct every successor to pass the identical requirement to its own successors. No handoff is complete without these fields.

**Textual operational toggles** (not platform-native switches): `AUTONOMOUS_EXECUTION=ON`, `CI_MONITORING=ON`, `STAGING_WORK_DURING_PR_CI=ON`, `CI_MONITORING_METHOD=LIVE_POLLING_DURING_ACTIVE_TURN`. PO can explicitly switch any of the first three OFF for an active turn; safety gates never change.

**Verbatim required handoff clause from RGS ContentBox SOP v1.4:**

«RGS_OVERLAPPED_STAGING_PR_CI_ACTIVE_TURN_V1_20261025: nella sessione attiva, dopo Recovery Audit e una sola promozione fast-forward con SHA atteso, la CI sul branch PR Draft controlla uno SHA immutabile mentre la Chat Operativa CONTINUA a sviluppare sullo staging senza PR e senza CI. Durante la CI verifica periodicamente gli stati live di run/job/step, lavorando sui lotti successivi già autorizzati, e non richiede nuovi “Vai” ordinari. Solo quando la CI precedente è conclusa e il nuovo lotto coerente è pronto, verifica gli HEAD e promuove una sola volta il candidato successivo. Nessun full log GitHub, polling programmato/background, force push, merge main o FQ senza GO. Ogni successiva chat deve trasmettere letteralmente questa clausola a tutte le proprie successive chat, insieme a SOP RGS ContentBox, branch/HEAD/PR/CI verificati e differenza fra candidato verde e staging non qualificato.»

The outgoing agent MUST reproduce that clause **verbatim**, not replace it with "as before" or a token alone. Previous independent recursive handoff clauses remain required.
