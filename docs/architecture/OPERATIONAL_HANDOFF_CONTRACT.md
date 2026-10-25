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
