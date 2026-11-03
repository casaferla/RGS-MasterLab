# RGS MasterLab — GitHub / RGS ContentBox synchronization contract

**Product Owner decision — 2026-10-31.** This is a continuity and recordkeeping policy, not a CI, mathematical proof, release signature, or a replacement for product Full Qualification.

## Canonical responsibilities (NO indiscriminate mirroring)

**GitHub is the authoritative operational workspace**, because Git HEAD, Draft PR #61, code, pending staging changes, focused CI and live qualification state can be inspected together. Maintain `docs/architecture/ACTIVE_OPERATOR_STATE.md` as the short current-session checkpoint; `AGENTS.md`, `OPERATIONAL_HANDOFF_CONTRACT.md` and relevant M17/Phase11 gate documents govern the workflow and tests. The source SHA, PR SHA, completed CI conclusion, staging-only delta and pending gates must never be conflated.

**RGS ContentBox / Google Drive retains immutable or private authority**: frozen Product Owner contracts, mathematical proof archives (private FIR, Q0, Q1), review/signature evidence, handoff continuity and milestone decision snapshots. Do not publish private original FIR/taps, PCM, full transcripts or other protected assets to the public repository. Do not rewrite historical frozen Drive authorities merely because engineering state changes.

**Current Drive continuity mirror**, now linked from GitHub:
- [31 October M17 / Final SRC Q0 handoff](https://docs.google.com/document/d/1D3jbgKd4DMVxdFG8ZMYwsnPPNguzb13tFOPLqPhICkM/edit), in RGS ContentBox / Chat Handoff & Continuity Protocol / Current Handoffs.
- [Master continuity index](https://docs.google.com/document/d/1Snic1CzHKHhiKe3yP1kpd3XWBUK8fK5FZNIU891C8zA/edit).
- [Frozen M17/Final SRC rate contract](https://docs.google.com/document/d/13fIk06CgiPLWcaW3KAZPCUoNFN5ffh6NHlVXK1sfA_8/edit).
- [Mandatory staging/CI recursive SOP](https://docs.google.com/document/d/1l3zD_CumZWwBzaPt790U3OaChDa3hvqV8HJCkSpLRz8/edit).

## Triggered synchronization — no background automation

1. **Every meaningful decision and coherent macro-lot:** write GitHub technical state/code/tests/docs to **staging** in one Single-Writer Epoch, no standalone CI for docs, then update the linked Drive continuity handoff with the decision, relevant source links/SHAs, open gate status and difference between green PR and unqualified staging. Preserve chronological entries.
2. **After PR fast-forward and terminal CI:** amend the GitHub operational checkpoint and the Drive milestone snapshot with exact immutable tested SHA, Windows/Linux conclusion, failures or FQ blockers. A still-running CI must be called IN PROGRESS, not PASS. Update both after each significant validated milestone, not after every microscopic commit or each poll.
3. **On a new chat:** do a short **live GitHub recovery check** (AGENTS + OPERATIONAL_HANDOFF_CONTRACT + ACTIVE_OPERATOR_STATE + relevant M17/Phase11 gate; PR/staging SHA and latest CI) and consult Drive only for frozen or private source provenance or missing decisions. Reuse saved state; do **not** recreate long manual handoff packages or redo a full historical recovery audit between ordinary chat windows. The existing recursive overlap clause and safe CAS still govern each turn.
4. **Cross-reference without duplicating sensitive material:** GitHub checkpoint links to Drive handoff and private evidence references using IDs or cryptographic hashes; Drive handoff links to exact GitHub commit, run and review. Record **as-of timestamp / status**, and note any intentionally deferred synchronization.
5. **If either archive write fails:** explicitly mark the inconsistency on the side that was successfully updated, report it rather than claiming synchronized state, and reconcile in the next active work session before relying on that mirror. Neither ChatGPT nor a scheduled agent automatically syncs the stores in the background.

## Current state at policy adoption

- 31 Oct 2026, PR #61 Draft SHA `46541cde7f2ce6f94ce66f1ee7ff09a5ebaf2cae`, CI [#940](https://github.com/casaferla/RGS-MasterLab/actions/runs/41153372210) **in progress at last live check**, Windows and Linux focused jobs in progress; do not infer future outcome.
- Last Focused double green #939: `a11b6eaceb92d61cb4c9cbd0d43cd79980fedc82`. Staging immediately before adopting this contract: `474cf378180a798fd58e62ff10b41a4b3cd9ba70`, two document-only mathematical Q0 review commits beyond #940 PR candidate. This contract is another **unqualified staging-only documentation change**.
- M17 Final SRC canonical Q0 approval and 28 route admissions remain **OPEN**. Next milestone after PO-approved M17 is M18 Dither. Unimplemented Android/Reference Match global gates are deferred.

## One sentence continuity rule

**GitHub = current executable operational truth. RGS ContentBox = frozen/private authority + milestone snapshot. At every consequential macro-lot decision update both via SHA-linked concise records; no costly per-chat handoff ritual and no unsupported claim of automatic continuous sync.**
