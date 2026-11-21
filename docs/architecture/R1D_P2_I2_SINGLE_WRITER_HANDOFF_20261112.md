# RGS MasterLab — R1d-P2 / I2 single-writer handoff to Product preview
Date: 2026-11-12. Owner: R1d-P2 integration chat, by explicit PO permission.
Status: **exclusive P2 I2 staging writer by governance, not a GitHub-enforced lock**.
No authority for main merge, FQ candidate rewrite, Product default activation, or release.

## Verified ancestry / qualification separation
- Parent branch/PR: [M23 Draft #99](https://github.com/casaferla/RGS-MasterLab/pull/99), source `staging/m23-failure-matrix-focused-ci-20261108`.
- Frozen exact FQ #101 candidate SHA: `616c3b7a9952220dfedb488bf39aa068df6d2e97`.
- [Windows Full Qualification #101](https://github.com/casaferla/RGS-MasterLab/actions/runs/42898634288) completed SUCCESS on 2026-11-10 at the exact SHA above; the PO's physical/manual Windows test is still pending. An automated FQ success is not the same as physical approval.
- **Never write/force-push** the FQ #101 head, PR #99, its old writer branch `staging/m23-failure-matrix-writer-no-ci-20261108`, `main`, or M17 frozen code.
- This new `staging/r1d-p2-i2-preview-writer-no-ci-20261112` is a *successor* branching directly from FQ #101 SHA, not a modification of the qualified object. The old M23 writer and candidate currently diverge; do not attempt implicit fast-forward of the old writer.
- P2 research source authority: `19990d13b53d22f34ae25d6fbef3234823d3b1d3`, folder `research/r1d-p2-live/`; blobs are re-used byte-for-byte in this staging branch with no algorithm changes.

## What single-writer means
The PO explicitly released the operational chat's writer turn while it waits for physical FQ #101, allowing this chat to develop P2 I2. **Only this chat writes this P2 staging branch until handoff**. The operational chat must not concurrently edit the same branch/overlapping GUI or audio files. This does not stop the PO from testing the already frozen FQ #101 in parallel.

## Scoped implementation goal: audible manual preview, *not* an icon-only row
1. Start from committed exact research P2 pinned sources, genuine `IModule` and true `ProcessingChain` tests; reconcile with FQ #101 main ancestry.
2. Add separate `Streaming True Peak Limiter` manual DSP to Product `ModuleRegistry` and `MasteringChainState`, preserving historical project restores and existing M17 `rgsml.dsp.true-peak-limiter` terminal registration.
3. Bind parameter authority and transport/latency into the **real** manual audio preview, with explicit state epochs and careful bypass, process, EOS/finalize. UI must only say ACTIVE if the played PCM truly passed through P2.
4. Add Qt/QML chain row/editor and the PO-approved waveform-under-ceiling copper icon `#C4774A`, with full UI text **Streaming True Peak Limiter**; preserve minimum viewport 1184×688, tooltips, dim/glow and other module identities.
5. Perform focused compile and tests Windows+Linux, runtime playback and UI screenshot then PO's physical listening. Do not claim full AutoMatch/FQ/production qualification from this research step.

## Explicitly deferred, without blocking first audible preview
- Full canonical AutoMatch L3 27-feature tuning/solver throughput/Q0 + M13 Fresh Verify.
- Complex runtime checkpoint: research I1.8 proved bit-deterministic fresh-epoch full rerenders on 4 synthetic cases; full-track performance/real solver remain open.
- Official FQ after this new Product delta, physical PC test of new preview, and release/merge approval.

## Stop conditions
Do not allow M17 change, production CI surprises, corrupt project persistence, missing latency compensation, click/dropout on transitions, or false ACTIVE UI. Before any FQ or merge, request separate PO instruction. For CI, use a distinct candidate branch from this writer with exact-SHA non-force update, and preserve this writer branch's history. No CI or Product tests are asserted by the source copy alone.

This commit is a **source migration / handoff**, not functioning Product wiring, not a Qt build or physical test.
