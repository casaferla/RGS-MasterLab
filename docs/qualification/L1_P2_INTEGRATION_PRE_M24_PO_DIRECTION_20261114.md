# RGS MasterLab — L1 P2 integration before M24: Product Owner direction

Date: 2026-11-14. Status: authorized **L1 integration development**, NOT release, FQ, Gate G/M, main merge or Product-default activation.

## Decision and priority

PO explicitly corrected the operational #83 handoff: R1d-P2 Streaming True Peak Limiter must be integrated into the **same L1 development corridor before L1-M24**, while completing automated/physical qualification of M18-M23. Do not defer integrating P2 until after L1. The independent research source may remain as historical provenance; only the **L1 Single-Writer Epoch** may now modify the integrated Product candidate.

After L1-M24 and required qualification, do only the indispensable L2 prerequisite bridge for L3 AutoMatch, then prioritize L3. The accepted 2026-11-13 L1→L2 bridge→L3 PO priority direction remains controlling, without falsely marking L2 done.

## Verified source ancestry and merge

- main remains at `304f593c89e7079aefa31727055b0880243245e4`; no main merge.
- L1 M17/M18 writer parent: `90937819084d77e8c1a9697682abffe7db76bed4`, preserving M17 -2.70 dBTP numeric roundoff correction and M18 exact PCM24/F32 WAV acceptance.
- P2 research Product I2 parent: `bd148a88a882dba10ff09f0190d57faa61a7e980`; predecessor P2 registry and real Qt GUI focused CI were successful (run IDs 43516371590, 43516371794) on Windows/Linux as applicable.
- Both parents share merge base `616c3b7a9952220dfedb488bf39aa068df6d2e97`. Comparison identified **11 L1-only** changed paths and **80 P2-only** changed paths, **no overlapping path**. Therefore merge commit `46b33f9c8cf081d5c24586f1119546a2cf4c4713` has two real parents and retains both sets. Git compare confirms L1→merge added only 80 P2 paths and P2→merge added only 11 L1 paths.
- Sole active integrated writer: `staging/l1-m24-p2-integration-writer-no-ci-20261114`. Pre-integration writers remain immutable for provenance, not competing writers.
- One focused integrated candidate branch: `staging/l1-m24-p2-integration-focused-ci-20261114`. Exact initial test SHA `8057d622713a362ed4d42aab45f6e80bd6e8bb48`, Actions run https://github.com/casaferla/RGS-MasterLab/actions/runs/43603174846.
- The integrated workflow calls *existing* P2 native DSP, P2 Qt Product GUI, M17/M18 exact export and M19-M23 evidence workflows. No fake success is inferred from historical greens. No automatic FQ.

## Integration limits — things still to deliver

1. **P2 development GUI and audio:** P2 is a fifth true MANUAL DSP and an actual RenderPreview processor under the I2 research opt-in (research ZIP marker or explicit `--rgsml-p2-i2-preview`). The existing Product build remains default four manual DSPs. Neither the shell UI nor the module row alone grants active-audio provenance.
2. **P2 Project persistence:** old Project restore only permits the historical 2/3/4 node mastering chain and the current P2 opt-in correctly refuses SaveAs to avoid silent corruption. This is a real outstanding L1 integration blocker before claiming ordinary user-ready P2, and requires a backward-compatible, versioned scoped Project extension and tests. Do not silently drop P2 or change frozen legacy contracts.
3. **P2 playback and meter:** physical user feedback (2026-11-12) reported M16 LIVE TP coverage stopping after handoff, brief PREPARED/PROCESSED discontinuity, and 4–5 s whole-chain re-render after parameter change. The I2 development work includes research-only SRC handoff recovery and switch tests, but device-level verification is still required. Do not bypass M16 proof or claim gapless playback.
4. **P2 telemetry/options:** Ceiling/Drive are genuine typed DSP values. 2%/5% is an unqualified research choice; shipping UX should remove the experimental selector only after choosing/qualifying an actual fixed profile. The P2 chain may remain bypassable (unlike the observer modules). M17 stays separate, after Final SRC, final certified limiter.
5. **M17/M18:** preserve PO 2026-11-14 division: M17 full Master True Peak verification; M18 faithful deterministic WAV codec/readback and delivery, **no postquant second TP veto and no M18 micro trim**. The field-tested -2.70 dBTP result remains narrow physical evidence, not a cumulative FQ.
6. **P2/cache:** research bounded immutable-prefix cache and verification oracles do not automatically qualify Product cache, its sidecars or memory/epoch contract. Use controlled performance proof before AutoMatch; no unsound shortcuts.
7. **M24:** official 17-step real SOURCE+REFERENCE, Project reopen, preview/full, M18 deliver, genuine analyzer GUI and actual PO audio judgement remain mandatory. PO physical testing is deferred until the PC is available; do NOT mark PASS.

## First combined focused run finding and fail-closed reconciliation

The new integrated candidate initially exposed a historical **M23 evidence manifest** stale test-slot reference `exactPcm24ReadbackAndCeilingEvidence`, removed when M18 was simplified per PO. Both M23 native jobs stopped in source-inventory selftest before C++ tests. This is not a failed P2 PCM result.

On the *writer only*, align `postdecode_true_peak_pcm24` with the genuine successor `exactPcm24ReadbackAndCodecEvidence` but classify the original category `partial`, not `existing`; the historical independent postdecode TP claim is superseded and must never silently re-enter M18. Corresponding M23 test/receipt policy expects 19 authentic existing + 1 explicit partial while all qualified flags remain false. A follow-on exact-SHA CI is required; do not promote until all initial focused jobs complete.

## Exit rules

No background/unattended promotion, no force push, no unapproved FQ/main merge. Finish coherent technical corrections in the no-CI writer, then CAS-promote once after the current candidate is terminal. Disclose exact commit, CI link, Windows/Linux jobs and real physical blockers.

**Next Product work after combined focused gate:** reconcile genuine P2 persistence/Project owner and actual normal-build opt-in, typed parameter revisions and M18 epoch invalidation; fix M16 provenance/switch only with real proof, and package one PO physical Windows build before final M24 acceptance. Reuse existing P2/render/registry/QML/Qt tests, not new algorithms.
