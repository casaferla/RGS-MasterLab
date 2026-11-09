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

## Results — CI #28 and next discriminating experiment (9 November 2026)

CI #28 https://github.com/casaferla/RGS-MasterLab/actions/runs/42674647315
SHA `321161cd32eca9868a69a20a5ba4ea3b3267cebf`:
Linux portable source/CTest job PASS. Windows original FULL M23 CTest gate still FAIL
only `rgsml_source_metadata_panel_smoke`; 10 of 11 CTests pass, same EQ
992x612 inside host 732x475. Artifact Windows `13701880038`:
`m23-failure-bounded.txt` and `m23-controlled-matrix.txt`.

**Controlled matrix observed (all FULL 16-case native Qt smoke):**
- B1 candidate #27, screenshot explicitly OFF: PASS, 16/16, 22.4 seconds.
- B2 SAME candidate binary, screenshot ON: PASS, 16/16, 67.2 seconds.
- A1 FQ99, screenshot explicitly OFF: PASS, 16/16, 19 seconds.
- A2 SAME FQ99 binary, screenshot ON: PASS, 16/16, 66.9 seconds.
- Repeated B1 candidate screenshot OFF: PASS, 16/16, 18.9 seconds.
Both immutable SHA builds succeeded; Qt 6.8.3 MSVC and native backend,
same working directory and full test invocation were used.

**Updated inference:** screenshot capture is NOT required for candidate #27 to
pass. A clean candidate #27 independently built without any analysis QML
ablation passes the complete Qt smoke. The original workflow's failure is
therefore conditional on a DIFFERENCE NOT CONTROLLED in the previous matrix:
(1) binary/build graph (original M23 multi-target build versus clean focused
smoke build), (2) working directory (CTest's test directory versus matrix's
repository root), (3) CMake wrapper/CTest execution/environment. The results
DO NOT establish which of these factors is causal. No GUI patch authorized.

**Pre-registered follow-up, no production edits:** phase-2 invocation-context
probe `tools/qualification/m23/probe_native_gui_invocation_context.ps1`
runs the EXACT SAME compiled executable from root and from CTest's test cwd,
then via the unmodified CMake wrapper in that test cwd, screenshot OFF, full
suite, bounded failures/totals. Repeat for (i) original M23 gate binary and
(ii) clean candidate #27 matrix binary. Record executable SHA256, Qt root and
workdir. Six arms C0–C5; original strict M23 CTest still executes and may fail,
which must never be concealed by diagnostic PASS. No app, QML, C++ test, DSP,
FIR or old qualified GUI is modified. Read bounded artifacts only.

### Interpretation
- Original binary FAILS in all contexts, clean candidate binary PASSES:
  investigate build graph / QML generated resources / cache identity.
- Same binary passes from root, FAILS from test directory: investigate cwd
  and relative resource/bootstrap paths; test fidelity versus product behavior.
- Same binary passes direct but FAILS through wrapper: investigate wrapper
  Qt QML environment / timing / test process semantics.
- Both variants pass in controlled context and CTest remains RED:
  inspect CTest environment modification and prior-test/order interactions,
  maintaining the original strict geometry assertion.
- If results fluctuate across repeated equivalent runs, characterize
  scheduling or nondeterminism rather than changing editor geometry.

**NO release or merge, PR #99 remains Draft; goal is causal explanation
before any changes to qualified GUI or speculative permanence policy.**

## Results — CI #29, identical executables fail only via CMake wrapper

CI #29: https://github.com/casaferla/RGS-MasterLab/actions/runs/42680094592
SHA `4209fc6d2b69182272a1319890c4ff29d00347a0`.
Linux portable CTest PASS; Windows strict original smoke FAIL (10/11 CTests PASS,
EQ 992x612 at (12,12) in 732x475 host). Bounded Windows artifact
`13702468852` includes `m23-controlled-matrix.txt`,
`m23-gui-context.txt` and `m23-failure-bounded.txt`.

The repeated clean immutable FQ99/candidate control matrix remains 5/5 GREEN,
each arm 16/16 QTest, screenshot ON and OFF. The six **same binary** launch-context
arms gave the essential distinction:

| Arm | Executable | Same Qt full suite, screenshot OFF | Result |
| --- | --- | --- | --- |
| C0 | original M23 multi-target build | PowerShell direct, repo root | PASS 16/16 |
| C1 | original M23 multi-target build | PowerShell direct, CTest tests cwd | PASS 16/16 |
| C2 | original M23 multi-target build | CMake wrapper, same tests cwd | FAIL 15/16, EQ overflow |
| C3 | clean immutable #27 focused build | PowerShell direct, repo root | PASS 16/16 |
| C4 | clean immutable #27 focused build | PowerShell direct, tests cwd | PASS 16/16 |
| C5 | clean immutable #27 focused build | CMake wrapper, same tests cwd | FAIL 15/16, EQ overflow |

Binary SHA256 for gate and clean builds are different, but **each exact binary**
reproduces the PASS direct vs FAIL CMake distinction. The test directory itself
does not account for this divergence. This strongly localizes the difference to
the CMake parent process / `execute_process` invocation and/or stdout/stderr
capture. The precise mechanism is **NOT YET proven**. It does not justify
blaming M19-M21 or changing the frozen Parametric EQ.

The existing diagnostic CMake wrapper
`tests/cmake/m16_c2_smoke_ctest_diagnostic.cmake` invokes the unchanged Qt
executable via `execute_process(COMMAND ... RESULT_VARIABLE ... OUTPUT_VARIABLE ...
ERROR_VARIABLE ...)`. The wrapper emits a bounded Qt failure neighborhood
only after the executable exits. Its behavior as process launcher or output
capture mechanism is now the subject of phase 3.

**Pre-registered next diagnostic**, limited to the same built original gate
executable, same tests CWD, full 16-case Qt smoke, screenshots explicitly OFF,
same Windows Qt/native backend, repeated cases:
P0 PowerShell direct; P1 CMake execute_process with result only/no capture;
P2 CMake execute_process capture OUTPUT_VARIABLE+ERROR_VARIABLE (as in original);
P3 CMake execute_process OUTPUT_FILE+ERROR_FILE; P4 stdout-variable only;
P5 no-capture repeat; P6 both variables repeat; P7 direct repeat.
CMake driver and PowerShell probe are diagnostic-only; the original wrapper,
CTest gate, application QML and all C++ test assertions remain UNCHANGED.

Interpretation must account for nondeterminism: if all CMake launch modes fail
but direct passes, investigate CMake as parent process and native window exposure;
if only variable capture fails, investigate stdout/stderr pipe configuration
and affected GUI scheduling. If results fluctuate, characterize run-order and
timing instead of modifying geometry. **Never turn an observational diagnostic
PASS into a release/FQ pass**.

No changes to `main` or qualified modules, no automatic merge, PR #99 Draft.

## Results — CI #30: CMake-parent behavior, stdout capture excluded

CI #30 https://github.com/casaferla/RGS-MasterLab/actions/runs/42685836317,
SHA `44b9f69eeb2bd2328c1062a85dca174b933a5b32`. Linux portable PASS;
Windows original CTest 10/11 PASS, Qt smoke FAIL EQ 992x612 in host 732x475.
Bounded artifact ID `13703864744`, files `m23-cmake-capture-modes.txt` and
`m23-failure-bounded.txt`.

**Same exact original gate EXE, SHA256
`DFA57F49A346592BCED0F7E3F0292A86C3A1AFAB6FD93818010429D6E999DB6F`,
same CTest tests cwd, screenshots OFF, same 16-case Qt suite:**
- P0 PowerShell direct: PASS 16/16, 19.7s.
- P1 CMake execute_process without OUTPUT/ERROR capture: FAIL 15/16, 12.6s.
- P2 CMake OUTPUT_VARIABLE + ERROR_VARIABLE: FAIL 15/16, 12.5s.
- P3 CMake OUTPUT_FILE + ERROR_FILE: FAIL 15/16, 12.5s.
- P4 CMake stdout variable only: FAIL 15/16, 12.5s.
- P5 repeat CMake no capture: FAIL 15/16, 12.5s.
- P6 repeat CMake capture variables: FAIL 15/16, 12.5s.
- P7 repeat PowerShell direct: PASS 16/16, 16.4s.
All failures were the identical original EQ containment assertion.

**Causal inference:** output capture mode alone is NOT the necessary trigger;
the entire CMake child-process launch path is implicated. The original Qt
test is otherwise unchanged and can pass through direct PowerShell invocation.
This still does NOT establish that the Qt engine or CMake is defective:
process parentage/console/window activation/exposure/GUI scheduler can be
different and have not been isolated. Reintroducing/fixing M19–M21 visual
children is NOT justified by these data.

**Pre-registered next diagnostic, no production changes:** run the identical
16-case smoke with screenshots OFF on Windows via (1) direct PowerShell,
(2) CMake directly spawning Qt, (3) CMake spawning an intermediate PowerShell
process which launches Qt with the exact same arguments. Repeat CMake direct
and PowerShell direct. Perform this on both the unchanged *original candidate
gate executable* and a clean-build *FQ99 baseline executable*, so we also
determine whether the qualified baseline fails under the same CMake-parent
conditions. Qt 6.8.3 MSVC/native backend; independent clean baseline build,
same source SHA pins, bounded first-failure report. Two new nonrelease
launcher scripts and one parent-chain test driver implement this matrix.
The authoritative original M23 CTest gate remains strict and must not
be declared green because of any diagnostic PASS.

Potential discriminators:
- FQ99 CMake direct FAIL and candidate CMake direct FAIL:
  this is also a qualified-baseline issue in the current CMake invocation,
  not a demonstrated M19–M21 regression.
- FQ99 CMake direct PASS, candidate CMake direct FAIL:
  still revision-dependent; record the exact intermediate transition.
- CMake -> PowerShell -> Qt PASS where CMake -> Qt FAIL:
  launch context matters; investigate console/parent and UI exposure.
- Both CMake variants FAIL while standalone PowerShell PASS:
  distinguish CMake ancestor/stdio handles from Qt exposure before choosing
  an infrastructure fix.
No workaround may weaken the containment assertion, and no GUI change is
authorized without evidence. PR #99 Draft, main frozen.

## Results — CI #31: FQ99 and candidate BOTH fail as direct CMake child

CI #31: https://github.com/casaferla/RGS-MasterLab/actions/runs/42688132408,
SHA `b75c407c748ff884c4ca6aa2cabf81ebfce71649`.
Linux portable tests passed. Windows original strict M23 CTest remained RED with
the same Parametric EQ geometry 992x612 inside 732x475. Bounded Windows artifact
`13706930468`, file `m23-parent-chain.txt`.

**Same runner, Qt 6.8.3 MSVC native QPA, full 16-case smoke, screenshots OFF,
same cwd within each triple, unmodified executable / source:**

| Qt launch path | FQ99 immutable baseline | Candidate #27 |
| --- | --- | --- |
| PowerShell -> Qt executable | PASS 16/16 | PASS 16/16 |
| CMake -> Qt executable | FAIL 15/16 | FAIL 15/16 |
| CMake -> pwsh -> Qt executable | PASS 16/16 | PASS 16/16 |

Candidate CMake-direct FAIL repeats, and direct PowerShell PASS repeats. All
failure cases hit the SAME original FQ99 EQ containment invariant. This
demonstrates that the test-launch path alone is sufficient to reproduce this
specific failure even in the qualified baseline. There is NO basis from these
observations to attribute the symptom to M19–M21 scenegraph integration.
The low-level Windows/Qt scheduling or exposure difference under the
CMake child remains unproven; do not mislabel an OS/Qt bug as proven.

### Evidence-backed minimum test-infrastructure correction

A narrow Windows-only change was prepared on the isolated writer:
- `tests/cmake/qt_smoke_native_windows_launcher.ps1` creates a PowerShell
  intermediate process with exactly the same `-v2 -o "log,txt"` Qt test
  invocation, and returns `$LASTEXITCODE`.
- `tests/cmake/m16_c2_smoke_ctest_diagnostic.cmake` uses CMake -> pwsh ->
  unchanged QtTest on Windows, with the same `RESULT_VARIABLE`,
  `OUTPUT_VARIABLE`, `ERROR_VARIABLE`, bounded failure reporting and
  strict nonzero exit assertion. Non-Windows retains historical execution.
  PowerShell 7 (`pwsh`) is an explicit Windows test-runner dependency; if
  absent the wrapper fails closed, rather than silently marking green.
- The focused M23 workflow removes the now-completed expensive experimental
  matrix; the original **all 11 real Windows CTests remain untouched**,
  including source-metadata Qt smoke and its original EQ containment
  assertions. The Linux portable suite is unchanged.

This is a correction to the TEST LAUNCH CONTEXT, not to a qualified GUI
or to the audio application. It is not a full product/FQ acceptance; the
focused CI will still fail if Qt reports a genuine failing assertion,
missing log, or nonzero exit. If this gate goes green, the next separate
architectural decision remains how to replace the experimental M19–M21
create/destroy integration with a persistent M16-style presentation once
the real visual panels and icons have been reviewed and physically tested.
No automatic merge or FQ claim. PR #99 stays Draft.
