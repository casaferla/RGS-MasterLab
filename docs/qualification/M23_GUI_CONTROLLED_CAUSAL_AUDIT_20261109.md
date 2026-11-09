# M23 Qt GUI — Controlled causal audit, macro-lotto 1 (nonrelease)

Date: 2026-11-09. Product Owner authorized causal diagnosis only.
Repository: casaferla/RGS-MasterLab.
Qualified immutable baseline: FQ #99 `5e4871d5849e4121b0612c2142d1e051d6c9a3d2`.
Investigated candidate: focused CI #27 `ba8bf9ea41b4d4e950bb780f035a368f1f1a10c9`.

## Why

Two independent read-only Work audits agree the host obtains 732x475 logical units at
the minimum 1184x688 window but Parametric EQ stays at implicit 992x612 at (12,12).
The inner host-to-ColumnLayout-to-EQ geometry assignment is unconfirmed; neither
report establishes a permanent-panel limitation, a Qt bug, or a need to create/destroy
analyses at each navigation. M16 True Peak Meter, M17 and Master Audition are
qualified precedents for persistent, declarative pages.

**CRITICAL confound** in old A/B: candidate real CTest ran the WHOLE Qt smoke
without explicitly enabling screenshot capture; the modified temporary worktree
ran ONLY `emptyReadyErrorAndWindowLifecycle` WITH
`RGSML_GUI01_EVIDENCE_DIR` enabled. The capture helper resizes, calls
`show()`, waits and grabs an image. FQ99 original qualification also enabled
screenshots. Therefore the apparent earlier 'A/B green after removing new item'
does NOT prove the item is the cause. Treat the old one-case A/B result as
observational/noncausal. The existing Linux M23 job does NOT run this GUI smoke.

## Pre-registered one-run experiment

Use a single GitHub Windows 2022 runner, Qt 6.8.3 MSVC x64, unchanged native
Qt QPA/backend configuration, `Release` Ninja Multi-Config builds and the
same working directory and CLI `-v2 -o "<arm-specific-output>,txt"` for the
FULL `rgsml_source_metadata_panel_smoke` executable in each arm.
Each arm is a new process. The script *only* explicitly sets/unsets screenshot
directory, and uses the exact immutable baseline/candidate source SHA.

| Arm | Source | Full smoke | Screenshots |
| --- | --- | --- | --- |
| B1 | Candidate #27 | YES | Explicitly OFF |
| B2 | Same built candidate exe | YES | Explicitly ON |
| A1 | FQ99 | YES | Explicitly OFF |
| A2 | Same built baseline exe | YES | Explicitly ON |
| B1 repeat | Same candidate exe | YES | Explicitly OFF |

The A/B script uses a detached read-only baseline worktree and an independent,
clean baseline build. It never removes or modifies M19-M21 production code.
It produces one bounded `m23-controlled-matrix.txt` artifact with per-arm
exit status, first failures, summary and elapsed time; full logs and PNGs remain
ephemeral on runner and are **not uploaded**. If compilation fails, mark relevant
arms `INCONCLUSIVE` and do not infer pass.

The previous non-equivalent source-ablation probe is NOT invoked in the CI.
The original candidate M23 CTest gate remains unchanged and authoritative;
the matrix is always labeled DIAGNOSTIC_ONLY and cannot make a red test green.

## Interpretation rules (before seeing results)

- B1 FAIL, B2 PASS: screenshot/exposure path affects candidate; diagnose
  genuine missing UI readiness versus flawed precondition of the test. NEVER
  enable screenshots simply to hide a product defect.
- A1 FAIL, A2 PASS: qualified baseline ALSO needs screenshots in this setup;
  the prior source-regression attribution needs revision.
- A1 PASS, B1 FAIL: revision-dependent regression remains; still need matched
  ablation (same full suite, screenshot mode and build hygiene) to attribute.
- B1/B2 FAIL and A1/A2 PASS: strong revision-dependent difference, next trace
  ColumnLayout/host parent, window, child geometry and docking transitions.
- Nonreproducible B1 repeat: prioritize test-order, Qt initialization and
  environmental determinism; do not ship a speculative layout patch.

## Gate after the matrix

Do not alter, clamp, clip, rescale or reflow FQ99-qualified controls, including
Parametric EQ/M16/M17/Master Audition. No test is disabled and no assertion
tolerance changes. No DSP, FIR, ProcessingChain or project schema changes.
Do not merge to main. PR #99 stays Draft. Diagnosis alone is not M23/FQ
closure. Only after interpreting the matched matrix may the next isolated
diagnostic identify the first layout divergence, with object identity,
`parentItem` versus QObject parent, controlling proxy, visibility, size hints,
`ColumnLayout` geometry and bounded polish trace. A future permanent
M16-style panel integration is the preferred architectural objective,
subject to actual evidence and PO approval.
