# CODEX TASK 009 / L1-M05 — Implementation report

## Outcome

`CODEX TASK 009 / L1-M05 — Waveform Zoom/Pan and Single Audition Region` is
complete on `task/L1-M05-waveform-navigation-audition-region` and ready for
owner review. `main` remains unchanged and Task 010 was neither prepared nor
started.

The delivery adds exact Source-frame waveform navigation, click-to-seek, one
half-open Audition Region, explicit loop orchestration, and the definitive
segmented Region time editor. It preserves the authoritative
`WindowsResourceReader -> WavReader -> canonical AudioBuffer -> WaveformSummary`
path, the Task 008 peak pyramid, the Task 007 public playback port, and
`WAV_ONLY` input scope.

The previously reported playback conflict
`BC12-L1M05-EXPLICIT-SEEK-LOOP-001` is resolved by the narrow private correction
authorized by `FD12-L1M05-EXPLICIT-SEEK-LOOP-001`.

## Git preflight and resume accounting

| Check | Observed | Result |
|---|---|---|
| current branch | `task/L1-M05-waveform-navigation-audition-region` | PASS |
| task `HEAD` / resume head | `17c36204846560d247602e786b1851f624c255fd` | PASS |
| `main` | `17c36204846560d247602e786b1851f624c255fd` | PASS |
| baseline subject | `CODEX TASK 008 / L1-M04` | PASS |
| baseline parent | `4469677b3f9b6511dfa84148fa63738c68d9b2e0` | PASS |
| baseline tree | `a97603f15f6072179599ffff5c1729ba83321323` | PASS |
| preserved Task 007 branch | `69c61a05602b18d68bccbec093b71131c4185ca7` | PASS |
| preserved Task 008 branch | `089a5a71cebee735142f89b9a3638496d48ceb4c` | PASS |
| historical untracked SHA-256 | `3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF` | PASS |

Authoritative packet evidence:

- consolidated Task 009 SHA-256:
  `E2D16163F77935E6D399D951E469F0E30D93DABDCAD9A1159BF4A8EEBEFE7F47`;
- forward delta SHA-256:
  `EE58847D030B71B52880349FA9E67E97F1E1B77C5C9B895C52E1559F38ECDC8F`.

The worktree classification remained explicit:

- `PREEXISTING_WORKTREE_CHANGES`: only
  `RGS_MasterLab_CODEX_TASK_001-2.md`, untracked, unstaged, and unchanged;
- `TASK_009_PRE_DELTA_CHANGES`: application/view-model, UI/QML, navigation,
  region, editor, tests, architecture, and delivery evidence listed below;
- `TASK_009_RATE_ADAPTATION_CHANGES`: none; Task 007 SRC, coefficients, assets,
  and `FrameMap` are unchanged;
- `TASK_009_EXPLICIT_SEEK_LOOP_FORWARD_CHANGES`: only
  `platform/windows/src/internal/playback_support.hpp`,
  `platform/windows/src/internal/playback_support.cpp`, and the existing owner
  test `tests/unit/platform/test_playback_support.cpp`.

No branch recreation, reset, restore, stash, rebase, or work loss occurred
during resume.

## Files and allow-list mapping

Modified:

- `app/CMakeLists.txt`
- `app/src/main.cpp`
- `app/src/playback_transport_view_model.hpp`
- `app/src/playback_transport_view_model.cpp`
- `ui/CMakeLists.txt`
- `ui/qml/Main.qml`
- `ui/src/waveform_item.hpp`
- `ui/src/waveform_item.cpp`
- `ui/src/waveform_presentation.hpp`
- `ui/src/waveform_presentation.cpp`
- `platform/windows/src/internal/playback_support.hpp`
- `platform/windows/src/internal/playback_support.cpp`
- `tests/CMakeLists.txt`
- `tests/ui_smoke/test_source_metadata_panel.cpp`
- `tests/unit/platform/test_playback_support.cpp`
- `tests/unit/ui/test_waveform_geometry.cpp`
- `docs/architecture/README.md`
- `docs/implementation/task-log.md`

Added:

- `app/src/audition_region_view_model.hpp`
- `app/src/audition_region_view_model.cpp`
- `ui/qml/SegmentedTimeEditor.qml`
- `ui/src/internal/waveform_viewport.hpp`
- `ui/src/internal/waveform_viewport.cpp`
- `tests/integration/app/test_audition_region_view_model.cpp`
- `tests/unit/ui/test_waveform_item_interaction.cpp`
- `tests/unit/ui/test_waveform_viewport.cpp`
- `docs/implementation/telemetry/L1-M05.json`
- `docs/implementation/reports/CODEX_TASK_009_L1-M05.md`

The packet placeholder `<accepted-transport-view-model-file>` maps exactly to
the existing `playback_transport_view_model.hpp/.cpp`. The new viewport helper
is private UI code. No new production target, decoder, audio cache, public
playback interface, codec, dependency, or error taxonomy was introduced.

`audio/`, `core/include`, public Windows platform headers, Task 007 historical
report/telemetry, Task 008 report/telemetry, RF-1, frozen phase material, and
`ToolchainManifest` sources are byte-for-byte unchanged from the baseline.

## Navigation, rendering, and boundedness

`WaveformViewport` stores one exact half-open Source-frame range. Checked
integer/rational operations map physical pixel boundaries to Source frames and
back without cumulative floating zoom state. Zoom preserves the pointer or
visible-playhead anchor subject to Source-edge clamping; keyboard zoom uses the
visible playhead, otherwise viewport center. Fit Source is exactly `[0,N)`;
Fit Region respects the deterministic minimum span.

Direct background drag pans from the gesture-start viewport snapshot, so move
event coalescing does not change the result. Keyboard pan provides coarse 10%
and fine one-physical-pixel-equivalent steps. Resize and DPR changes preserve
the exact frame viewport.

The renderer reuses the immutable Task 008 pyramid, selects only the visible
bucket window, and keeps the accepted maximum of 4,096 ranges per channel.
Navigation performs zero Source reads, decodes, summary rebuilds, or secondary
cache construction. The Task 008 shared physical-boundary seam invariant is
preserved.

QML owns layout and high-level bindings only. Exact frame/time, hit testing,
gesture state, viewport math, and playback commands remain in C++.

## Audition Region, seek, and loop semantics

`AuditionRegionViewModel` owns exactly one optional Source-frame
`[start,end)` region. Creation works in either Shift-drag direction; handles
edit only their endpoint; single-click on background or region fill issues one
exact Source-frame seek without autoplay or region mutation. Double-click is
deterministically separated from delayed single-click: background fits Source,
fill/handles fit Region, and neither generates a seek.

With Loop Region enabled, a successful new/edit transaction sets the exact
loop. If the authoritative playhead lies outside the new half-open range, the
application performs one explicit `seek(start)`; if it is already inside, it
does not reposition. Forward/reverse drag and event timing do not change the
result. Explicit click-to-seek remains exact before, inside, or after an armed
loop and never creates a second seek.

The authorized private Task 007 correction separates explicit seek position
from traversal-driven loop wrapping. A seek after the loop end stays at that
exact Source frame while the loop remains armed; wrapping becomes eligible only
after traversal re-enters the loop interval. Real boundary crossing still
wraps, EOF/stop/replay remains coherent, and exact plus playback-only
44.1<->48 kHz paths retain Source-frame identity.

Public `IAudioPlaybackService`, public Windows adapter headers, device
negotiation, PCM bridge, SRC kernels/coefficient assets, and unrelated Task 007
state behavior are unchanged.

## Definitive segmented Region time editor

Each endpoint uses eight keyboard-accessible fields in the required traversal:

```text
Start HH -> MM -> SS -> FRACTION -> End HH -> MM -> SS -> FRACTION
```

Separators are static. Segments accept only digits and permit partial edit
state. Hours are not clock-limited; minutes/seconds validate `00..59`; fraction
supports up to nine digits with `.5` semantics represented as `500000000`.
Composition, exact rational conversion, ties-to-even quantization, ordering,
Source-end bounds, and one-frame regions are authoritative in C++.

On semantic failure the editor shows accessible validation feedback, leaves the
committed region unchanged, and restores the last committed frame-quantized
value. It does not clamp or invent a nearby value. This behavior was manually
verified and explicitly approved by the Product Owner.

## Forward-delta and interaction test evidence

Deterministic coverage includes:

- viewport mapping, left/center/right anchors, repeated zoom without drift,
  Source-edge clamp, coarse/fine pan, fit, DPR, and minimum span;
- 4-physical-pixel click/drag threshold and coalesced-move independence;
- background/fill single click, handle editing, double-click cancellation,
  forward/reverse region creation, canonical `Ctrl++`, keyboard pan/fit/cancel,
  and wheel step bounding;
- all segmented editor formats, invalid values, exact quantization,
  ties-to-even, traversal, select-all, ordering, Source end, and one-frame
  range;
- stopped/paused/playing seek and region transactions with loop off/on,
  playhead before/inside/after, failure rollback, partial reposition outcome,
  Source replacement, picker cancel, and no spurious playback calls;
- explicit seek before/inside/after loop, real loop crossing, EOF/replay, exact
  and both authorized playback SRC directions.

The earlier findings for loop edit reposition, delayed double-click seek,
region-fill click interception, and explicit seek with armed loop were corrected
and retained under deterministic regression tests.

## Automated acceptance

Final accepted inventory is 41 tests: all 38 baseline tests plus three Task 009
executables. Complete Debug and Release CTest matrices passed 41/41 after the
forward correction and consolidated implementation.

After the final Product Owner clarification cleanup, only actually invalidated
surfaces were repeated:

| Verification | Debug | Release |
|---|---:|---:|
| full warnings-as-errors build | PASS | PASS |
| `rgsml_waveform_interaction_tests` | 1/1 PASS | 1/1 PASS |
| `rgsml_source_metadata_panel_smoke` | 1/1 PASS | 1/1 PASS |
| dependency-contract | 17/17 PASS | 17/17 PASS |

The dependency runs preceded the cleanup of two UI-only experimental handlers;
the cleanup changed no build graph, include, link, target, or dependency-rule
surface, so those results remain valid.

Additional acceptance:

- focused waveform-navigation, Audition Region, waveform, playback,
  playback-SRC, platform, Source-resource, and UI-smoke suites: PASS in Debug
  and Release;
- tests-off Release build: PASS with `RGSML_BUILD_TESTS=OFF`;
- final staged install: PASS;
- staged deploy smoke: PASS, exit code 0;
- required Qt GUI/Quick/QML/Multimedia DLLs and plugins: present;
- build/install `ToolchainManifest.json`: byte-identical, SHA-256
  `49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11`;
- final monitored normal close: exit code 0, stdout/stderr zero bytes;
- zero QML errors, binding loops, RGSML errors, or relevant new warnings;
- no dependency download, install, network access, machine configuration
  change, or antivirus bypass.

Qt deployment emitted only the previously accepted host notices for optional
DirectX shader-compiler discovery and an unset `VCINSTALLDIR`; all required
runtime artifacts were deployed and smoke passed. Avast/CyberCapture caused no
reported quarantine or detection.

## Manual Windows gate and Product Owner clarification

The Product Owner completed the functional gate on the actual staged Release
and accepted the real waveform, main zoom/pan paths, click-to-seek, single and
double click behavior, region creation/editing, explicit seek with armed loop,
loop wrap, region editing during playback, segmented time editor, Source
replacement, picker cancel, malformed-source failure/retry,
resize/minimize/restore, Source/playback invariants, and `WAV_ONLY` scope.

The local non-versioned RF64 recovery fixture committed successfully as
RF64/WAVE, 48 kHz, Float64, mono, one frame. The truthful message that the
current default output cannot reproduce that Source format exactly describes
device playback capability; it is not a Source load/recovery failure and does
not expand Task 009 playback scope.

Final classifications are recorded exactly:

- `Ctrl++` — `VERIFIED PASS`, canonical keyboard Zoom In;
- `Ctrl+-`, Ctrl+wheel, Home, and visible zoom controls — `VERIFIED PASS`;
- `Ctrl+= alias on non-US keyboard layouts` —
  `NOT REQUIRED / PO CLARIFIED`; it is not a residual bug;
- `Alt+F4 during active mouse-captured Audition Region gesture` —
  `PO_ACCEPTED_DEFERRED / NON-BLOCKING`; it is not represented as a test PASS.
  The observed extreme window does not crash or hang, remains responsive, and
  normal close after gesture completion succeeds. Future lifecycle/input
  hardening may revisit it outside Task 009.

The experimental non-US text alias and special active-grab `Alt+F4` handler
introduced during the final rework attempt were removed as unnecessary or
ineffective. The accepted Task 009 interaction implementation and canonical
`Ctrl++` test remain.

This is a positive human gate with explicit Product Owner clarification and
one disclosed non-blocking deferred item. It is not an automated or fabricated
PASS.

Actual UI was reviewed live with a private Source and the real segmented editor.
No screenshot is committed because the reviewed state exposed private Source
metadata. The non-versioned owner review is the privacy-safe visual-target
evidence; no mockup or test harness was substituted.

## R12, RF-1, residuals, and rollback

- `R12-01 — INHERITED UNCHANGED`.
- `R12-02 — MITIGATED_FOR_WINDOWS_L1-M05 / RESIDUAL_MONITORING_OPEN`.
- `RF-1 — MATERIALIZED; FROZEN ESTIMATES UNCHANGED`.

R12-02 is mitigated for this milestone because navigation retains the 4,096
range/channel hard bound, performs no decode/read/rebuild, stays responsive
during the reviewed interaction matrix, preserves the Task 008 seam fix, and
does not reproducibly disrupt playback. Monitoring remains open for later
multi-waveform/Gold/UI growth. The earlier baseline conflict did not trigger
R12-02.

The only accepted residual is the Product Owner deferred active-mouse-capture
`Alt+F4` edge case described above. The non-US `Ctrl+=` alias is explicitly not
required and is not residual risk.

The complete Task 009 diff is one source-control-local rollback unit from
`17c36204846560d247602e786b1851f624c255fd`. Reverting it restores the accepted
Task 008 tree and removes the Task 009 view-model, navigation/region/editor,
narrow private forward correction, additive tests, and evidence. No migration,
machine change, dependency uninstall, or user-data rollback is required.

Final intended state:

```text
CODEX TASK 009 — READY FOR REVIEW
BASELINE — 17c36204846560d247602e786b1851f624c255fd
BRANCH — task/L1-M05-waveform-navigation-audition-region
MAIN — UNCHANGED / NOT MERGED
RESUME WORKTREE — TASK_009_PRE_DELTA_CHANGES PRESERVED
INPUT FORMAT SCOPE — WAV_ONLY
BC12-L1M05-EXPLICIT-SEEK-LOOP-001 — RESOLVED_BY_FD12-L1M05-EXPLICIT-SEEK-LOOP-001
FD12-L1M05-EXPLICIT-SEEK-LOOP-001 — MATERIALIZED_FORWARD_IN_CODEX_TASK_009
TASK 007 HISTORICAL REPORT / TELEMETRY / RF-1 — UNCHANGED
R12-01 — INHERITED UNCHANGED
R12-02 — MITIGATED_FOR_WINDOWS_L1-M05 / RESIDUAL_MONITORING_OPEN
RF-1 — MATERIALIZED; FROZEN ESTIMATES UNCHANGED
CODEX TASK 010 — NOT PREPARED / NOT STARTED
```
