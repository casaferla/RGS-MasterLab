# CODEX TASK 008 / L1-M04 — Implementation report

## Outcome

`CODEX TASK 008 / L1-M04 — Real Source Waveform Overview and Minimal Peak
Pyramid` is complete on `task/L1-M04-source-waveform-overview` and ready for
owner review. `main` remains unchanged and Task 009 was neither prepared nor
started.

The delivered waveform is derived only from
`WindowsResourceReader -> WavReader -> canonical AudioBuffer`. It is a real,
bounded, deterministic full-Source overview; it introduces no alternative
decoder, full-track PCM preload, DSP, SRC, remix, normalization, Dither,
persistence, or Task 009 interaction.

## Git preflight and differential accounting

The mandatory preflight completed before the task branch was created:

| Check | Observed | Result |
|---|---|---|
| authoritative baseline / `main` | `4469677b3f9b6511dfa84148fa63738c68d9b2e0` | PASS |
| baseline parent | `d6b265f96a2d50440fb0631914c6b372878aa4e9` | PASS |
| baseline subject | `CODEX TASK 007 / L1-M03` | PASS |
| baseline tree | `c378260dcb1fa234a388938781ab2d83cf8120dd` | PASS |
| preserved Task 007 branch | `69c61a05602b18d68bccbec093b71131c4185ca7` | PASS |
| preserved Task 007 tree | `c378260dcb1fa234a388938781ab2d83cf8120dd` | PASS |
| required Task 008 branch initially absent | confirmed | PASS |
| initial status | one historical untracked file only | PASS |
| historical file SHA-256 | `3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF` | PASS |

Final accounting is deliberately separated:

- `PREEXISTING_WORKTREE_CHANGES`: only
  `RGS_MasterLab_CODEX_TASK_001-2.md`, untracked and unstaged, with the same
  SHA-256;
- `TASK_008_CHANGES`: only the allow-listed L1-M04 files enumerated below;
- no Task 009 branch, packet, code, test, fixture, report, telemetry, design, or
  placeholder was created.

## Files and repository mapping

Modified:

- `app/CMakeLists.txt`
- `app/src/main.cpp`
- `app/src/source_selection_view_model.cpp`
- `app/src/source_selection_view_model.hpp`
- `audio/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/cmake/dependency_rules_fixture/CMakeLists.txt`
- `tests/ui_smoke/test_source_metadata_panel.cpp`
- `ui/CMakeLists.txt`
- `ui/qml/Main.qml`
- `docs/architecture/README.md`
- `docs/implementation/task-log.md`

Added:

- `audio/include/rgsml/audio/waveform_summary.hpp`
- `audio/src/waveform_summary.cpp`
- `audio/src/internal/waveform_summary_builder.hpp`
- `app/src/source_waveform_view_model.hpp`
- `app/src/source_waveform_view_model.cpp`
- `ui/src/waveform_presentation.hpp`
- `ui/src/waveform_presentation.cpp`
- `ui/src/waveform_item.hpp`
- `ui/src/waveform_item.cpp`
- `ui/src/internal/waveform_geometry.hpp`
- `ui/src/internal/waveform_geometry.cpp`
- `tests/unit/audio/test_waveform_summary.cpp`
- `tests/integration/audio/test_waveform_wav.cpp`
- `tests/integration/app/test_source_waveform_view_model.cpp`
- `tests/unit/ui/test_waveform_geometry.cpp`
- `docs/implementation/telemetry/L1-M04.json`
- `docs/implementation/reports/CODEX_TASK_008_L1-M04.md`

The packet's conceptual application coordinator maps to
`SourceWaveformViewModel`; the Qt presentation bridge maps to
`WaveformPresentation`; bounded native rendering maps to `WaveformItem` plus
private `internal/waveform_geometry`. These names follow the accepted
repository conventions rather than creating parallel models or targets.

No `core/`, `platform/`, Task 007 playback/SRC asset, frozen phase document,
RF-1 artifact, dependency/license manifest, or toolchain-manifest generator was
changed. README changes were not needed.

## Public surface and ownership

The one new authoritative cross-layer artifact is
`rgsml::audio::WaveformSummary`, algorithm id
`rgsml.waveform.summary.minmax-pyramid`, version `1.0.0`. Its public API exposes
immutable metadata, immutable per-level views, `PeakRange`, exact payload size,
and `build_waveform_summary(WavReader&, std::stop_token)`. The public header is
Qt-, QML-, Win32-, platform-, application-, and UI-free.

Ownership remains one-way:

```text
application worker/cache -> rgsml_audio WaveformSummary
UI presentation/renderer -> shared immutable WaveformSummary
rgsml_audio -> rgsml_core
```

The application owns the persistent `std::jthread`, cancellation source,
generation number, stale-publication guard, and single current immutable
summary. Every analysis opens its own `WindowsResourceReader` and `WavReader`;
the playback session and pump remain independent. The one-entry cache exists
only in memory for the current Source and is never persisted.

QML owns layout, state bindings, accessibility labels, and retry intent only.
It receives no PCM container or track-proportional list. C++ selects the level
and builds capped scene-graph geometry. The read-only playhead has no pointer,
touch, scrub, seek, zoom, pan, or region behavior.

## Deterministic algorithm and hard bounds

For `N` Source frames and `Kmax = 65,536`:

```text
B0 = nextPowerOfTwo(max(1, ceil(N / Kmax)))
K0 = ceil(N / B0)
```

Base buckets cover exact half-open frame ranges. Each higher level pair-combines
adjacent children and copies an odd final child. Construction uses one forward
decode pass in blocks of at most `Dmax = 4,096` frames. It keeps no full-track
`AudioBuffer` and performs no second decode pass.

The enforced bounds are:

- base buckets: at most 65,536 per channel;
- all levels: at most 131,071 buckets per channel;
- UI selection: at most `Umax = 4,096` ranges per channel;
- stereo peak payload: at most
  `131,071 * 2 * sizeof(PeakRange) = 4,194,272` bytes;
- decoded work block: at most 4,096 frames.

Minimum/maximum preserve all finite canonical binary64 values. Comparisons
prefer `-0` for a minimum and `+0` for a maximum, preserve subnormals and values
outside `[-1,+1]`, and reject non-finite input deterministically. Render-only
clipping does not change summary values.

Whole-build and partitioned-build tests with block sizes 1, 2, 7, 255, 4,095,
4,096, and an irregular deterministic partition are bit-identical. Independent
test oracles validate level zero and recurrence rather than using production
code to generate expected values.

## Lifecycle and concurrency evidence

Deterministic semaphore/condition-variable tests cover:

- `EMPTY -> BUILDING -> READY` and failure/retry;
- replacement clearing the prior visible summary before candidate publication;
- slow Source A followed by Source B, including cancellation and stale-result
  rejection;
- picker cancellation/failure preserving current Source and summary;
- worker execution off the GUI thread;
- playback progress while analysis is deliberately blocked;
- independent reader sessions and zero waveform-issued playback/device/SRC
  commands;
- shutdown join and absence of callback-after-destruction.

Cancellation is checked before/between bounded decode blocks and before
publication. Construction publishes either a complete immutable summary or an
accepted error/cancel outcome; partial levels never cross the worker boundary.

## Renderer seam finding and correction

The first human gate found reproducible full-height dark vertical seams whose
frequency changed with viewport width. The summary and audio landmarks were
otherwise plausible. Diagnosis isolated the issue to centered one-pixel
`DrawLines` at fractional logical coordinates: rasterization did not guarantee
that adjacent bucket columns covered adjacent physical pixels.

The renderer now maps every bucket to a deterministic span. Internal boundaries
are snapped by the same physical-pixel boundary function, while the endpoints
remain exactly `0` and the logical viewport width. Consequently, the right
boundary of bucket `i` is bit-identical to the left boundary of bucket `i+1`.
Each min/max interval is emitted as two triangles in one opaque geometry draw;
there is no arbitrary line-width increase and no overlap used to hide the
defect.

The geometry unit test exercises 640 through 1,920 logical pixels, fractional
and high DPR values from 1.0 through 2.5, irregular bucket counts, first/last
positions, strict positive span width, physical boundary snapping, and exact
adjacent-boundary equality. `WaveformSummary` and all peak data remain
unchanged. The post-fix human gate passed at large, intermediate, and reduced
window sizes and during playback.

## Toolchain and commands

Observed toolchain:

- Windows 10 Pro x64, version `10.0.19045`, build `19045`;
- MSVC `19.44.35228.0` from Visual Studio Build Tools 2022 x64;
- CMake `3.31.6-msvc6`;
- Qt `6.8.3` MSVC 2022 64-bit, including Qt Multimedia.

The accepted process normalized only the current process/child environment to
handle the host `PATH`/`Path` collision and expose the already installed local
toolchain. It changed no machine, user, repository, profile, antivirus setting,
or Git configuration. Nothing was downloaded or installed.

Representative acceptance commands, with process-local environment setup
omitted:

```text
cmake --preset windows-msvc --fresh
cmake --build --preset windows-msvc-debug-clean
ctest --preset windows-msvc-debug --output-on-failure
cmake --build --preset windows-msvc-release-clean
ctest --preset windows-msvc-release --output-on-failure
ctest --preset windows-msvc-debug --output-on-failure -L waveform
ctest --preset windows-msvc-debug --output-on-failure -L ui_smoke
ctest --preset windows-msvc-release --output-on-failure -L waveform
ctest --preset windows-msvc-release --output-on-failure -L ui_smoke
cmake --preset windows-msvc --fresh -B build/windows-msvc-tests-off -DRGSML_BUILD_TESTS=OFF
cmake --build build/windows-msvc-tests-off --config Release --clean-first
cmake --install build/windows-msvc --config Release --prefix build/windows-msvc/stage/L1-M04/Release
build/windows-msvc/stage/L1-M04/Release/bin/RGSMasterLab.exe --rgsml-deploy-smoke
```

The final seam fix was followed by an incremental Debug build, focused Debug
tests, a clean Release build, focused Release tests, complete Debug and Release
CTest runs, a clean tests-off rebuild/audit, a staged reinstall, deploy smoke,
and a repeated human gate.

## Automated acceptance results

| Acceptance group | Debug | Release |
|---|---:|---:|
| complete CTest after final fix | 38/38 PASS | 38/38 PASS |
| pre-existing inventory retained | 34/34 | 34/34 |
| label `waveform` | 4/4 PASS | 4/4 PASS |
| label `waveform_golden` | 1/1 PASS | 1/1 PASS |
| label `ui_smoke` | 2/2 PASS | 2/2 PASS |
| label `audio` | 7/7 PASS | 7/7 PASS |
| label `audio_golden` | 3/3 PASS | 3/3 PASS |
| label `source_resource` | 5/5 PASS | 5/5 PASS |
| label `playback` | 4/4 PASS | 4/4 PASS |
| label `playback_src` | 1/1 PASS | 1/1 PASS |
| label `playback_device` | 1/1 PASS | 1/1 PASS |
| label `platform` | 4/4 PASS | 4/4 PASS |
| label `dependency_contract` | 17/17 PASS | 17/17 PASS |

Additional acceptance:

- fresh configure: PASS;
- clean Debug and clean Release builds with warnings-as-errors: PASS;
- tests-off clean Release build: PASS, `RGSML_BUILD_TESTS=OFF`, zero test
  executable artifacts, no production `Qt6::Test` requirement;
- staged install/deploy smoke: PASS, exit code 0, stdout/stderr empty, no Source,
  device, playback, or waveform side effect;
- build/install `ToolchainManifest.json`: byte-identical, both SHA-256
  `49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11`;
- final staged manual stdout/stderr: zero bytes; zero QML errors, binding loops,
  RGSML errors, or relevant warnings;
- `git diff --check`, allow-list, protected-path, dependency, and Task 009
  audits: PASS.

Qt deployment emitted only the previously accepted external notices about
optional `dxcompiler.dll`/`dxil.dll` discovery and an unset `VCINSTALLDIR`.
They did not omit a required runtime. Avast/CyberCapture analyzed newly built
executables but did not block a command, quarantine a file, or report a
detection; measured host overhead from it was zero minutes.

## Manual Windows functional and visual gate

Host context: Windows 10 Pro `10.0.19045` build `19045`, system DPI 96 (100%
display scale). The reviewer exercised a large/maximized window, an intermediate
window, and a reduced window approximately 640x360. The private Source was an
accepted real stereo RGS WAV. Its exact rate, frame count, numeric duration, and
build elapsed time were not supplied to the agent and are deliberately recorded
as not available rather than fabricated. Metadata and duration displayed by the
application were explicitly judged coherent. Analysis/playback overlap was
observed.

| # | Human check | Result |
|---:|---|---|
| 1 | truthful no-Source empty state, no fake waveform | PASS |
| 2 | select real RGS WAV through accepted picker | PASS |
| 3 | bounded building state, responsive shell | PASS |
| 4 | full-track structure and landmarks plausible | PASS |
| 5 | metadata/rate/channels/duration/transport coherent | PASS |
| 6 | playback coexistence during/after analysis | PASS |
| 7 | advancing read-only, non-interactive playhead | PASS |
| 8 | Play/Pause/Resume/Stop/natural EOF unchanged | PASS |
| 9 | picker cancel preserves Source and waveform | PASS |
| 10 | valid replacement clears old result; no stale flash | PASS |
| 11 | invalid/truncated fixture semantics | PASS |
| 12 | deterministic failure/retry path | PASS |
| 13 | resize/DPI/minimize/restore responsiveness | PASS POST-FIX |
| 14 | state accessibility and keyboard traversal | PASS |
| 15 | real landmark comparison | PASS |
| 16 | no zoom/pan/seek/region/Task 009 affordance | PASS |
| 17 | normal close, exit 0, no hang or continuing audio | PASS |
| 18 | private Source size/hash unchanged | PASS, values withheld |
| 19 | zero QML/RGSML errors or relevant warnings | PASS |
| 20 | actual staged UI visual target | PASS, non-versioned human evidence |

The initial gate was `NOT PASS` only for the vertical seam artifact. After the
renderer correction the same real waveform was continuous without artificial
vertical gaps at large, intermediate, and reduced sizes, including during
playback. No reproducible dropout occurred; every other gate item remained
conformant.

Actual-UI evidence was reviewed live from the staged Release after the private
WAV traversed the authoritative path. No screenshot is committed because the
reviewed application state contained private Source metadata. The non-versioned
evidence is the explicit human gate record. A separate capture attempt was not
substituted with a mockup or test harness when the Windows capture helper was
unavailable.

## Bounded observations and Source immutability

Manual Source numeric profiling not supplied by the reviewer is `null` in
telemetry. Structural bounds are enforced and observed by deterministic tests.
For reproducible numeric evidence, the checked-in redistributable fixture
`tests/audio_golden/playback_src/listening_stereo_44100_pcm16.wav` has:

| Property | Value |
|---|---:|
| SHA-256 | `A70B9349806D0FC2AA8FD129A331ED37DF989D67D0E0BA636DC45D3A79651C11` |
| sample rate / channels / frames / duration | 44,100 Hz / 2 / 308,700 / 7 s |
| base frames / bucket | 8 |
| base buckets / channel | 38,588 |
| levels / total buckets per channel | 17 / 77,181 |
| peak payload | 2,469,792 bytes |
| maximum decode block observed | 4,096 frames |

The fixture hash remains unchanged after integration tests. The human reviewer
confirmed the private Source size/hash remained unchanged; the values and all
private identifying metadata are intentionally absent from versioned evidence.

## Risks, telemetry, defects, and residuals

- `RF-1 — MATERIALIZED; FROZEN ESTIMATES UNCHANGED` (inherited; artifact not
  edited).
- `R12-01 — INHERITED UNCHANGED` (Task 007 playback/SRC implementation and
  evidence not edited).
- `R12-02 — MITIGATION_MATERIALIZED_AT_L1-M04 / RESIDUAL_OPEN_FOR_L1-M05`.

`R12-02` receives the permitted positive status because hard decode, summary,
payload, and UI bounds pass; the overview remained responsive on the reference
machine; playback coexistence passed without reproducible disruption; repeated
resize/replacement remained bounded; and the post-fix visual gate passed.
The risk is not closed because zoom/pan remains future L1-M05 scope.

Three implementation findings were corrected: QML registration required the
`QQuickItem` subclass not to be `final`; deploy smoke exposed one missing QML
status id; and human review exposed the viewport-dependent seam fixed above.
No accepted-baseline regression remains. There was no scope, frozen-contract,
dependency, or estimate deviation.

Agent active time spans interrupted sessions and cannot be reconstructed
reliably. Human review duration and private Source timing were not supplied.
Those values remain `null` in telemetry rather than being inferred. Frozen
2/3/5 PDE estimates are unchanged.

## Rollback and delivery state

The complete Task 008 diff is one source-control-local rollback unit from
`4469677b3f9b6511dfa84148fa63738c68d9b2e0`. Reverting the delivery removes the
summary/builder, worker/cache, renderer/QML integration, additive tests, and
Task 008 evidence while restoring the accepted Task 007 tree. It requires no
data migration, registry change, machine configuration, dependency uninstall,
or rollback of Task 001–007, RF-1, R12-01, frozen documents, or user data.

Final intended state:

```text
CODEX TASK 008 — READY FOR REVIEW
BASELINE — 4469677b3f9b6511dfa84148fa63738c68d9b2e0
BRANCH — task/L1-M04-source-waveform-overview
MAIN — UNCHANGED / NOT MERGED
R12-01 — INHERITED UNCHANGED
R12-02 — MITIGATION_MATERIALIZED_AT_L1-M04 / RESIDUAL_OPEN_FOR_L1-M05
RF-1 — MATERIALIZED; FROZEN ESTIMATES UNCHANGED
CODEX TASK 009 — NOT PREPARED / NOT STARTED
```
