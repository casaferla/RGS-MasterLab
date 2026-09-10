# CODEX TASK 007 / L1-M03 implementation report

## Delivery status

- Status: `READY_FOR_REVIEW`
- Branch: `task/L1-M03-windows-wav-playback`
- Delivery subject: `CODEX TASK 007 / L1-M03`
- Integration: `NOT_MERGED_TO_MAIN`
- RF-1: `MATERIALIZED`; decision `KEEP_BASELINE`; frozen estimates unchanged.
- R12-01: `MITIGATED_FOR_WINDOWS_L1-M03_44100_48000` with residual risk open.
- Next task: `TASK_008_NOT_PREPARED_OR_STARTED`

The delivery commit identifier is reported externally after commit creation so
this file contains no circular self-reference.

## Authoritative inputs and Git preflight

The consolidated Task 007 packet and the Playback Rate Adaptation Delta were
read in full. The delta changes only the former playback-SRC prohibition and
the directly related device-selection, allow-list, test, and evidence clauses.
All other frozen contracts remain authoritative.

| Evidence | Verified value |
|---|---|
| baseline / current pre-commit HEAD | `d6b265f96a2d50440fb0631914c6b372878aa4e9` |
| baseline subject | `CODEX TASK 006 / L1-M02` |
| baseline parent | `8a366ccd1f2c29fd07e48dae6eafd8a4da7cc621` |
| baseline tree | `0d2fd8b28d57a01ce289007cc88cc4fb4bca380e` |
| Task 006 delivery branch | `task/L1-M02-windows-source-resource` |
| Task 006 delivery commit | `3c6be62b7f3d224086aa62d9fbbac3b472f1614e` |
| Task 006 delivery tree | `0d2fd8b28d57a01ce289007cc88cc4fb4bca380e` |
| Task 007 branch | `task/L1-M03-windows-wav-playback` |
| merge-base with `main` | exact Task 007 baseline |

`main` remained on the baseline throughout implementation. The preserved Task
006 branch was neither changed nor rewritten and its tree remained identical
to the baseline tree.

### Worktree attribution

`PREEXISTING_WORKTREE_CHANGES` is exactly:

```text
?? RGS_MasterLab_CODEX_TASK_001-2.md
```

It remained untracked and unstaged. Initial and final SHA-256:

```text
3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF
```

`TASK_007_PRE_DELTA_CHANGES` contains the already-green concrete playback
service, private state/pump/PCM implementation, application composition,
transport view-model/QML, platform/device/composition/UI tests, dependency
rules, and build/deploy wiring present at `resumeGitStatus`. The later
dedicated-worker scheduling correction is attributed to this category because
it corrects that original playback pump; it does not change rate adaptation.

`TASK_007_RATE_ADAPTATION_CHANGES` contains only the authorized Qt-free
`PlaybackSampleRateAdapter`, frozen coefficient assets, generator/verifier,
SRC golden/listening fixtures, SRC tests, paired-rate selection and timeline
mapping changes, plus consolidated evidence. Files shared by both categories
are classified at hunk/behavior level rather than falsely assigning the whole
file to one phase.

The combined Task 007 diff is confined to the consolidated allow-list. The
root `CMakeLists.txt` change is the allowed centralized Qt Multimedia discovery
needed by the platform target. No core file, frozen Phase document, Task 008
artifact, or non-allow-listed product schema changed.

## Architecture and artifact mapping

| Artifact | Header/path | Owner | Dependency direction | Principal verification |
|---|---|---|---|---|
| frozen playback port | `core/include/rgsml/core/audio_playback_service.hpp` | core, unchanged | no outward dependency | inherited contract tests |
| Windows playback adapter | `platform/windows/include/rgsml/platform/windows/windows_audio_playback_service.hpp` | platform | public core only; private audio/Qt | public-header, device, manual gate |
| device selection / PCM / pump | `platform/windows/src/internal/playback_support.*` | platform private | inward to core/audio | state, PCM, pump, dependency tests |
| Qt Multimedia implementation | `platform/windows/src/windows_audio_playback_service.cpp` | platform private | inward to Qt Core/Multimedia | probe, deploy, listening gate |
| playback SRC | `audio/include/rgsml/audio/playback_sample_rate_adapter.hpp` | audio | core/audio only; Qt-free | math/asset/signal/invariance tests |
| SRC implementation/assets | `audio/src/playback_sample_rate_adapter.cpp`, `audio/src/internal/playback_src_kernel_*` | audio private | no platform/Qt | generator check and independent oracle |
| transport view-model | `app/src/playback_transport_view_model.*` | application composition | exact playback port | composition/UI smoke |
| Source/playback transaction | `app/src/source_selection_view_model.*` | application composition | existing Source plus playback port | stale-source failure test |
| transport UI | `ui/qml/Main.qml` | UI | presentation only | QML/UI smoke and manual gate |
| generator/oracle | `tools/qualification/playback_src/` | offline qualification | external MPFR environment only when explicitly run | deterministic generation/check/oracle |

Target graph:

```text
rgsml_audio    --PUBLIC--> rgsml_core
rgsml_platform --PUBLIC--> rgsml_core
rgsml_platform --PRIVATE-> rgsml_audio
rgsml_platform --PRIVATE-> Qt6::Core + Qt6::Multimedia
rgsml_app      ----------> core + audio + platform + UI/Qt composition
```

There is no `audio -> platform` dependency. The core and audio public surfaces
contain no Qt or Win32 types. `QAudioDevice`, `QAudioFormat`, `QAudioSink`, the
bounded `QIODevice`, device state, and the dedicated Qt worker are private to
the Windows implementation. QML contains no decode, device, SRC, or playback
state-machine authority.

## Frozen port mapping and state behavior

The implementation uses the exact accepted `IAudioPlaybackService`; no second
port or extended public interface exists.

| Operation | Implemented behavior |
|---|---|
| initial / `clear` | `NO_SOURCE`, position 0, no duration/loop; clear is idempotent in its valid domain |
| `prepare` success | opens `WindowsResourceReader` read-only, creates `WavReader`, negotiates output, establishes `STOPPED` at source frame 0 with no loop; never autoplays |
| `prepare` failure | candidate construction fails before installation, preserving the previous service snapshot as required |
| `play` | idempotent while playing; resumes from pause; from stopped re-prepares the current Source to revalidate the current default device, preserving position/loop |
| `pause` / resume | valid playback is suspended/resumed without changing the source timeline contract |
| explicit `stop` | stops output, clears its queue, sets `STOPPED`, and returns source position to 0 |
| natural EOF | drains scheduled bytes, sets `STOPPED` at source duration; next Play restarts at 0 |
| `seek` | checked source-frame range; preserves paused/stopped state and restarts atomically if previously playing |
| `set_loop` | accepts only non-empty absolute half-open ranges within known duration; output boundaries use exact rate mapping |
| `snapshot` | returns source-rate position/duration/loop; backend or pump failure returns the existing categorical error |

Qt `OpenError` maps to `AccessDenied`; underrun, I/O, fatal backend, scheduler,
or worker invocation failures map to `IoFailure`. Invalid control states,
unsupported format/layout, out-of-range seek, invalid frame range, integer
overflow, non-finite sample, and decode/resource errors retain existing core
categories. Failed control-plane commands preserve the prior snapshot.

Source replacement remains transactional. A new accepted Source stops and
clears prior playback before preparing the new reference; a prepare failure
cannot leave stale prior audio playable under new metadata. Cancel does not
change the session.

## Authoritative decode, bounded streaming, and scheduling

`WindowsResourceReader` remains the only concrete resource opener and is used
read-only. `WavReader` remains the only WAV parser/decoder. Decoded blocks use
the canonical planar binary64 `AudioBuffer`; Qt performs neither decode nor
rate conversion. No QMediaPlayer, QAudioDecoder, codec fallback, full-track
preload, alternate buffer, writer, cache, or temporary output audio exists.

The pump is bounded by:

- 1,024 decoded/output frames per block;
- at most 16 pump iterations per tick/prefill;
- a maximum SRC source window of 1,412 frames for the frozen ratios;
- a 64 KiB device queue and a 32 KiB requested QAudioSink backend buffer;
- checked frame/channel/sample/byte arithmetic and explicit partial-write
  handling.

The QAudioSink callback only drains the mutex-protected bounded byte queue. It
does not decode, access files, allocate unbounded storage, run business logic,
or execute QML/UI work.

The first headphones listening attempt exposed a reproducible fractional
dropout during native resize/maximize/minimize: the 10 ms refill timer was on
the GUI/main event loop and Windows modal interaction could starve it. The
correction moved timer, reader/decoder, `PlaybackEngine`, QAudioSink, and pump
onto one dedicated Qt worker event loop. Public calls remain serialized and
synchronous through queued invocation. Complete Debug/Release/platform/UI
regression and the full two-output listening gate passed after this change.

## Device negotiation and PCM bridge

The deterministic candidate order is:

1. exact-rate Float32;
2. exact-rate PCM16;
3. paired-rate Float32;
4. paired-rate PCM16.

The paired candidate is available only for `44100<->48000` with unchanged
mono/stereo channel layout. Exact candidates set `srcApplied=false`; paired
candidates set `srcApplied=true`. Any other rate pair or unsupported unchanged
layout fails explicitly with `UnsupportedOperation` or
`UnsupportedAudioLayout`; there is no preferred-rate heuristic or channel
remix.

The device bridge interleaves canonical planar samples. Float32 conversion
preserves IEEE representation semantics allowed by the conversion and rejects
non-finite/unrepresentable values. PCM16 accepts only `[-1,1]`, scales by
32768, rounds ties-to-even, saturates only the positive endpoint to 32767, and
writes little-endian signed codes. It rejects rather than silently normalizing,
limiting, dithering, clipping out-of-range values, or changing channels.

## Playback SRC contract and qualification

Identity and policy identifiers:

```text
capability:       rgsml.audio.src.playback
algorithm:        rgsml.playback-src.polyphase-fir/1.0.0
kernel:           rgsml.playback-src.kernel.kaiser-sinc/1.0.0
rate policy:      rgsml.playback-src.rates.44100-48000/1.0.0
boundary policy:  rgsml.src.boundary.even-reflect/1.0.0
frame mapping:    rgsml.src.frame-map.half-open-ceil/1.0.0
manifest:         rgsml.playback-src.kernel-manifest/1.0.0
```

The output length and half-open boundaries use checked exact-rational
`ceil(inputFrames * L / M)`. Absolute output-frame mapping fixes phase across
whole/chunked reads, seek and loop boundaries. The 192-tap-per-phase
Kaiser-windowed sinc design uses passband edge 20,000 Hz, stopband edge 22,050
Hz, cutoff 21,025 Hz, 120 dB design attenuation, beta `613263/50000`, and
even-reflect finite-source boundaries. Scalar accumulation order is fixed.

| Direction | L/M | Coefficients | Group delay at high rate | Kernel SHA-256 | Ziv stability |
|---|---:|---:|---:|---|---:|
| 44.1→48 kHz | 160/147 | 30,721 | 15,360 frames | `ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4` | 512 bits |
| 48→44.1 kHz | 147/160 | 28,225 | 14,112 frames | `37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22` | 512 bits |

The offline generator uses Python 3.12 plus gmpy2 2.3.1 / MPFR 4.2.2,
round-to-nearest-ties-to-even, and a 256/512/1024... Ziv stability sequence.
Its external environment is neither committed nor referenced by machine path,
and ordinary configure/runtime does not invoke it. `generate.py --check`
reproduced both assets and the manifest. The independent response verifier
reported:

| Direction | Passband ripple | Stopband attenuation | Result |
|---|---:|---:|---|
| 44.1→48 kHz | 0.000012857 dB | 121.631422 dB | PASS |
| 48→44.1 kHz | 0.000012857 dB | 121.632201 dB | PASS |

Automated tests cover coefficient bit identity/symmetry, policy rejection,
constant and stereo independence, one-frame boundaries, frame/range mapping,
whole-vs-chunk exact equality, seek equality, loop phase continuity, memory
bound and oversize rejection. This artifact is playback-only; it neither
reuses nor impersonates Final Output SRC, render, mastering, or qualification
identity.

## Device probe and manual listening evidence

The separate sanitized probe enumerated three output devices. On the selected
default, exact 44.1 kHz stereo Float32/PCM16 were unsupported; exact 48 kHz
Float32 was supported and PCM16 was unsupported. Deterministic selection was
therefore 48 kHz Float32 with `srcApplied=true` for a 44.1-kHz Source and exact
48 kHz Float32 with `srcApplied=false` for a 48-kHz Source.

Redistributable fixture evidence:

| Basename | Provenance / expected format | Bytes | SHA-256 before and after |
|---|---|---:|---|
| `listening_stereo_44100_pcm16.wav` | deterministic L1-M03 generator; stereo PCM16 44.1 kHz | 1,234,844 | `A70B9349806D0FC2AA8FD129A331ED37DF989D67D0E0BA636DC45D3A79651C11` |
| `listening_stereo_48000_pcm16.wav` | deterministic L1-M03 generator; stereo PCM16 48 kHz | 1,344,044 | `A4BA63513146AE032C29B3E5CA065A908E9D5C96F4EF45B53F21DC33725F480A` |

The private musical Source is recorded only as `PRIVATE_RGS_TRACK`. No path,
name, hash, locator, or content is versioned. Owner evidence confirms
`SOURCE_BYTES_UNCHANGED=true`; the product opened it only through the accepted
read-only boundary and produced no writer/output/temp artifact.

| Sanitized output | 44.1→48 Float32 (`srcApplied=true`) | 48 exact Float32 (`srcApplied=false`) | Transport/EOF/source switch | Window interaction | Result |
|---|---|---|---|---|---|
| `HEADPHONES_PRIMARY` | PASS | PASS | PASS | PASS after scheduler fix | PASS |
| `DISPLAY_SECONDARY` | PASS | PASS | PASS | PASS | PASS |

The human gate confirmed immediate first audio, plausible pitch/speed/duration,
correct L/R order, Pause/Resume/Stop, Stop position 0, natural EOF/replay,
source replacement, responsive resize/minimize/restore, and normal close.
There was no autoplay, hang, crash, persistent crackle, clearly audible
aliasing, click, residual reproducible dropout, underrun, or backend error after
the correction. A separate staged normal launch/close returned exit code 0.
The gate is playback sanity only and makes no Q0–Q6, latency, device-class, or
bit-transparent DAC claim.

`R12-01` progressed from
`TRIGGERED / PLATFORM_LIMITATION CONFIRMED` to
`MITIGATED_FOR_WINDOWS_L1-M03_44100_48000`. Frozen probability/impact `M/H`
remain unchanged; residual risk stays open for other rates/devices/drivers,
driver processing, Android and later qualification.

## Automated acceptance and deployment

The accepted process normalized only the current process/child environment to
remove the case-colliding `PATH`/`Path` host condition and expose the already
installed Qt kit. It changed no machine, user, repository, shell profile, or
global configuration. Qt Multimedia and the offline generator dependency were
installed manually by the owner; the agent downloaded or installed nothing.

Representative commands, with the process-local environment omitted:

```text
cmake --preset windows-msvc --fresh
cmake --build --preset windows-msvc-debug-clean
ctest --preset windows-msvc-debug
cmake --build --preset windows-msvc-release-clean
ctest --preset windows-msvc-release
cmake -S . -B build/windows-msvc-tests-off -G "Visual Studio 17 2022" -A x64 -DRGSML_BUILD_TESTS=OFF
cmake --build build/windows-msvc-tests-off --config Release --parallel
cmake --install build/windows-msvc --config Release --prefix build/windows-msvc/stage/L1-M03/Release
build/windows-msvc/stage/L1-M03/Release/bin/RGSMasterLab.exe --rgsml-deploy-smoke
```

After the scheduler correction, the invalidated Debug and Release application,
platform and complete CTest suites, tests-off build, install/deploy smoke, and
full human gate were rerun.

| Acceptance group | Result |
|---|---|
| Git baseline/subject/parent/tree/branch/hash preflight and resume | PASS |
| prerequisite/toolchain/Qt Multimedia discovery | PASS |
| fresh configure and clean Debug build, warnings-as-errors | PASS |
| complete Debug CTest | 34/34 PASS |
| clean Release build, warnings-as-errors | PASS |
| complete Release CTest | 34/34 PASS |
| inherited baseline inventory | all 28 tests remain present and PASS |
| label `playback` | 3/3 PASS |
| label `playback_src` | 1/1 PASS |
| label `playback_device` | 1/1 PASS |
| label `platform` | 4/4 PASS |
| label `source_resource` | 3/3 PASS |
| label `ui_smoke` | 1/1 PASS |
| label `audio` | 5/5 PASS |
| label `audio_golden` | 2/2 PASS |
| label `dependency_contract` | 17/17 PASS |
| tests-off configure/build/audit | PASS; zero test artifacts and no Qt6::Test requirement |
| staged install/deploy smoke | PASS; exit 0; no audio/device side effect |
| manual launch/listening/functional gate | PASS on both outputs; normal exit 0 |
| QML/RGSML diagnostics | zero errors and zero relevant warnings |

The six additions to the 28-test baseline inventory are the playback SRC,
playback support, playback composition, playback device probe, platform
Multimedia link contract, and forbidden audio→Multimedia dependency tests.

The stage contains `Qt6Multimedia.dll`, the Windows/FFmpeg Multimedia plugins,
and the required Qt/QML runtime. The deploy tool emitted only the previously
accepted optional dxcompiler/dxil and unset developer-environment notices; no
required runtime was missing. Build/install `ToolchainManifest` files are
byte-identical with frozen SHA-256:

```text
49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11
```

## Findings, telemetry, and stop boundary

The host findings were the process `PATH`/`Path` collision, initially missing
Qt Multimedia, unavailable exact 44.1-kHz output, and initially unavailable
offline MPFR generator dependency. They were handled only through permitted
owner installation/actions and process-local environment normalization. No
host path or workaround is versioned.

One product defect was found by the first headphones gate and corrected:
GUI-event-loop refill starvation during native window interaction. It was not
an SRC algorithm defect or regression. Post-fix automated and human evidence
is green; zero regressions remain.

Telemetry records nine implementation/resume iterations, eighteen observable
configure/build/test command groups, one corrected product defect, zero
regressions, and no invented blocked duration. Owner and review hours remain
zero because measured values were not supplied. Rich checkpoint detail and
the evidence gaps are in `docs/implementation/reforecast/RF-1.md`, whose
decision is `KEEP_BASELINE`.

Final scope audit confirms that only Task 007 allow-listed changes are staged;
generated build/stage/log artifacts remain ignored; the pre-existing untracked
packet remains outside staging; `main` is unchanged; and no Task 008 file,
waveform, zoom/pan, Audition Region, Gold, DSP, render, persistence, or later
milestone artifact was prepared.
