# RGS MasterLab — OPERATING CONTRACT & HANDOFF BOOTSTRAP
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
