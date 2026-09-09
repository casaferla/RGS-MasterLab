# CODEX TASK 005 / L1-M01 implementation report

## Delivery status

- Status: `READY_FOR_REVIEW`
- Branch: `task/L1-M01-wav-decode-audio-buffer`
- Delivery subject: `CODEX TASK 005 / L1-M01`
- Integration: `NOT_MERGED_TO_MAIN`
- Next task: `TASK_006_NOT_PREPARED_OR_STARTED`

The delivery commit identifier is reported externally after commit creation so
this file does not contain a circular self-reference.

## Git preflight and preserved worktree

The mandatory preflight completed before editing and verified:

| Evidence | Verified value |
|---|---|
| initial branch | `main` |
| baseline and initial HEAD | `04306e664b9c1add58645cbbc681e725bddc485f` |
| baseline subject | `CODEX TASK 004 / L0-M04` |
| baseline parent | `84d97d5e80295893f20f39e81d733693b07071d4` |
| baseline tree | `be3ab18201511d6337b3c2882ba1eeea465bb453` |
| Task 004 delivery commit | `6e123f46aac10a745d8c42fcb369dd30596ca24a` |
| Task 004 delivery tree | `be3ab18201511d6337b3c2882ba1eeea465bb453` |
| task branch | created directly from the baseline |

`initialGitStatus` and `PREEXISTING_WORKTREE_CHANGES` are exactly:

```text
?? RGS_MasterLab_CODEX_TASK_001-2.md
```

The file remained untracked and unstaged. Its initial and final SHA-256 is:

```text
3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF
```

`main` remained at the baseline throughout implementation. `TASK_005_CHANGES`
are the production audio directory, additive core error/rule integration,
audio tests/fixtures, and the allow-listed documentation artifacts described
below. No pre-existing worktree content is part of the delivery.

## Public artifacts and ownership

All new runtime symbols are in `rgsml::audio`. The only new production target
is the static C++20 library `rgsml_audio`; it links publicly only to
`rgsml_core` and has target-local strict floating-point and warning policy.

| Public artifact | Header | Ownership/dependency | Primary evidence |
|---|---|---|---|
| `ChannelLayout`, `AudioChannel`, `AudioFormat` | `audio_format.hpp` | `rgsml_audio -> rgsml_core` | valid rates, mono C, stereo L/R, invalid layout/channel |
| `FrameDomainId`, `AudioTimebase` | `audio_format.hpp` | exact Task 003 `SampleRate` | source/output domains and exact frame/time reuse |
| `AudioBuffer` | `audio_buffer.hpp` | move-only aligned planar storage | empty/odd/multi-frame, alignment, move and overflow |
| `AudioBufferView`, `MutableAudioBufferView` | `audio_buffer_view.hpp` | non-owning standard spans | checked channel/range, absolute subviews and bit preservation |
| `WavSampleFormat`, `WavContainerKind`, `WavStreamInfo` | `wav_format.hpp` | immutable decoded metadata | supported RIFF/RF64 matrix and exact contract identifiers |
| `WavReader` | `wav_reader.hpp` | owns one Task 004 `IResourceReader` | bounded parsing, exact decode, random access, lifecycle/failure |

The contract identifiers are exactly:

```text
rgsml.wav-decoder/1.0.0
rgsml.source-pcm-conversion/1.0.0
```

Before L1-M01, the production graph was:

```text
rgsml_app -> rgsml_core
rgsml_app -> rgsml_ui -> Qt6::Quick + Qt6::QuickControls2
rgsml_app -> Qt6::Core + Qt6::Gui + Qt6::Qml + Qt6::Quick
```

After L1-M01, the only added production edge is:

```text
rgsml_audio (STATIC, C++20) -> rgsml_core
```

There is no reverse `core -> audio` edge and no audio edge to Qt, QML,
platform, DSP, UI, codec, filesystem, Win32, or Android/JNI code. Standalone
consumer builds compile each of the five public audio headers without Qt.

## Canonical buffer contract

`AudioBuffer` owns one or two disjoint planes of IEEE binary64 samples. Standard
C++20 aligned allocation provides 64-byte alignment for every non-empty plane,
with RAII cleanup and fallible allocation. Checked arithmetic covers total
sample bytes and the absolute end frame. A new buffer is deterministically
filled with the `0x0000000000000000` positive-zero representation.

Buffer and views carry `AudioFormat`, `AudioTimebase`, and an absolute half-open
range. Subviews preserve format/timebase, advance absolute origin exactly, and
never transfer ownership. Checked APIs reject invalid channel/range access.
Compile-time and runtime tests prove move-only ownership. Explicit span copy,
view, subview, and move preserve these binary64 patterns:

```text
-0.0                    8000000000000000
minimum +subnormal      0000000000000001
minimum -subnormal      8000000000000001
finite outside range    4004000000000000  (+2.5)
```

No clipping, normalization, interleaving, channel conversion, or `RGSDAU1`
signed-zero canonicalization occurs in runtime PCM.

## WAV supported matrix

The structural parser explicitly reads little-endian fields, uses checked
64-bit offset/size arithmetic, bounds `fmt` allocation to 64 bytes, and skips
unknown payloads by seek. It continues after `data` to detect duplicates and
never eagerly reads the complete audio payload.

| Container/encoding | Layouts exercised | Rates exercised | Variants |
|---|---|---|---|
| RIFF PCM16 | mono C | 44.1/48 kHz | golden anchor and boundary vector |
| RIFF PCM24 | mono C, stereo L/R | 44.1/48 kHz | sign extension, extensible PCM, random ranges |
| RIFF PCM32 | mono C, stereo L/R | 48/96 kHz | boundary vector |
| RIFF IEEE F32 | mono C, stereo L/R | 48/192 kHz | direct tag and extensible float |
| RIFF IEEE F64 | mono C, stereo L/R | 96/192 kHz | direct tag |
| RF64 IEEE F64 | mono C | 48 kHz | canonical `ds64`, negative-zero golden |
| RF64 PCM16 | mono C | 48 kHz | canonical size/sample-count checks |
| unknown chunks | supported | matrix-wide | valid even and odd payload/padding |

Direct IEEE-float inputs exercise standard 18-byte `WAVEFORMATEX` with
`cbSize == 0`; the compact 16-byte form remains accepted by the checked-in RF64
anchor. `WavStreamInfo` tests verify kind, encoded format, logical layout, rate, frame
count, block alignment, data offset/size, extensible flag, valid bits, and
contract IDs. Extensible inputs require `cbSize == 22`, valid bits equal to
container bits, the PCM/float GUID, and exactly `FRONT_CENTER` for mono or
`FRONT_LEFT | FRONT_RIGHT` for stereo.

## Exact conversion and golden evidence

Independent oracles cover, for every PCM width, the integer codes:

```text
-S, -S+1, -S/2, -1, 0, 1, S/2, S-2, S-1
```

The expected binary64 value is independently computed as `q * 2^-(B-1)` for
`B = 16, 24, 32`; comparisons have no tolerance. This proves little-endian
decoding, PCM24 sign extension, `-S -> -1.0`, `S-1 < +1.0`, and no clamp.

F32 inputs are widened exactly to binary64 and F64 values preserve their exact
bits. Both widths cover positive/negative zero, minimum positive/negative
subnormal, minimum positive/negative normal, `+/-0.5`, `+/-1.0`, finite values
outside `[-1,1]`, and positive/negative maximum finite. Key evidence includes:

```text
F32 -0 widened          8000000000000000
F32 min +subnormal      36A0000000000000
F64 -0                  8000000000000000
F64 min +subnormal      0000000000000001
```

Quiet NaN, signaling NaN, positive infinity, and negative infinity for both
F32 and F64 return `InvalidAudioSample`; bounded staging leaves the complete
destination bitwise unchanged.

The checked-in golden pack is hand-authored, contains no recorded audio, uses
the repository license, and is milestone conformance evidence only:

| Asset | Purpose | SHA-256 |
|---|---|---|
| `tests/audio_golden/wav/golden_vectors.hpp` | literal RIFF PCM16 and RF64 F64 byte assets | `8E0DE7938F65B5805A16A13CFAAC6C7232E6C4FEBDEA793CD0A0586EAB929B99` |
| `tests/audio_golden/wav/expected_bits.hpp` | independent binary64 expected bits | `E92B01E96811C62251BB4AA0E2C772916A8C01B558A0B48A31CDE2CC947BA2E4` |
| `tests/audio_golden/wav/README.md` | provenance/license/qualification boundary | `CEE1A26B5F891FFDFE6882654664D36BDE95B39DF05912D8B8587F985B37EA09` |

The production parser is not used to create any expected value.

## Random access, short reads, and bounded parsing

One 17-frame stereo PCM24 source is decoded whole and with exact partitions
`1, 2, 3, 7, 4`; every output bit matches. Non-ordered ranges beginning at
frames `9, 0, 14, 3` match independent full-source positions. Reader transfer
limits of 1, 3, and 5 bytes exercise exact-read loops and yield identical
samples. EOF tests prove zero frames at `start == frameCount`, range failure
after it, exact short-tail count, and unchanged destination tail.

An injected failure after a partial encoded transfer returns the original I/O
category while both output planes remain bitwise unchanged. The decoder stages
at most 256 frames per encoded batch and publishes only after all requested
frames validate.

Opening a WAV with a 4 KiB payload reads less than 128 bytes of metadata and
never requests more than 16 bytes in the measured RIFF case. A sparse reader
declares a valid 16 MiB unknown chunk without allocating it; open seeks across
the span, reads less than 128 bytes, and finds the following `fmt`/`data` chunks.
Readers without known size remain valid; readers without seek or read
permission are rejected categorically. Close is idempotent and post-close
decode reports `InvalidState`.

## Rejected matrix and error taxonomy

| Rejected condition | Verified category |
|---|---|
| RIFX; PCM8; compressed/unknown tags; packed valid bits; bad GUID | `UnsupportedAudioEncoding` |
| more than two channels; incompatible/ambiguous channel mask | `UnsupportedAudioLayout` |
| malformed form; missing/duplicate `fmt` or `data`; bad pad/boundary | `MalformedAudioContainer` |
| bad `blockAlign`, `byteRate`, partial frame, bad extensible/standard `cbSize` | `MalformedAudioContainer` |
| RF64 missing/bad/duplicate `ds64`, bad sentinel or sample count | `MalformedAudioContainer` |
| short header/resource shorter than declared form | `TruncatedAudioData` |
| RF64 `ds64` offset addition overflow | `IntegerOverflow` |
| F32/F64 quiet/signaling NaN or positive/negative infinity | `InvalidAudioSample` |

The five additive `ErrorCode` values and exact round-trip tokens are:

| `ErrorCode` | Canonical token |
|---|---|
| `UnsupportedAudioEncoding` | `unsupported_audio_encoding` |
| `UnsupportedAudioLayout` | `unsupported_audio_layout` |
| `InvalidAudioSample` | `invalid_audio_sample` |
| `MalformedAudioContainer` | `malformed_audio_container` |
| `TruncatedAudioData` | `truncated_audio_data` |

No existing Task 003/004 code/token was renamed or reinterpreted.

## Dependency and public-header gates

The Task 004 transitive rule was extended with the `AUDIO` categorical layer.
The rule excludes only the root target's own layer, then audits every reachable
dependency. Both expected-negative configurations fail only on their verified
canonical diagnostics:

```text
RGSML_DEPENDENCY_RULE_VIOLATION: target 'rgsml_audio' reaches forbidden dependency 'fixture_forbidden' in layer 'QT'
RGSML_DEPENDENCY_RULE_VIOLATION: target 'rgsml_core' reaches forbidden dependency 'rgsml_audio' in layer 'AUDIO'
```

Positive graph/consumer fixtures, all standalone audio headers, and the
forbidden include/type scan pass. The scan found no Qt/QML, Win32, Android/JNI,
filesystem/file-stream, platform, DSP, or UI surface in audio. Existing core
dependency fixtures remained unchanged in meaning and green.

## Test inventory and final acceptance

The baseline inventory was 11 enabled CTests. The final inventory is 19; all 11
pre-existing tests remain present and enabled. The eight additions are:

```text
rgsml_audio_format_tests
rgsml_audio_buffer_tests
rgsml_audio_wav_reader_tests
audio_dependency_graph_allowed
audio_public_headers_standalone
audio_forbidden_include_contract
audio_dependency_rule_negative_fixture
audio_dependency_rule_positive_fixture
```

The accepted local toolchain remained CMake `3.31.6-msvc6`, MSVC
`19.44.35228.0` x64, Visual Studio Build Tools `17.14.39`, Qt `6.8.3`, Windows
SDK `10.0.26100.0`, and Git `2.53.0.windows.3`.

| Command group | Exit/result |
|---|---|
| Git baseline/branch/tree/hash preflight | PASS |
| fresh `cmake --preset windows-msvc --fresh` | exit 0 |
| Debug test inventory | 19 enabled tests |
| clean Debug build, warnings-as-errors | exit 0, zero RGSML warnings |
| complete Debug CTest | 19/19 PASS |
| clean Release build, warnings-as-errors | exit 0, zero RGSML warnings |
| Release test inventory | 19 enabled tests |
| complete Release CTest | 19/19 PASS |
| Debug label `audio` | 3/3 PASS |
| Debug label `audio_golden` | 1/1 PASS |
| Debug label `dependency_contract` | 10/10 PASS |
| tests-off configure/build/audit | PASS; `Qt6::Test` and audio test targets absent |
| install to `stage/L1-M01/Release` | exit 0 |
| staged `RGSMasterLab.exe --rgsml-deploy-smoke` | exit 0; no QML/RGSML error |
| final manifest/hash checks | PASS |
| `git diff --check` | PASS |

The generated and installed `ToolchainManifest.json` files are byte-identical
and both have the frozen SHA-256:

```text
49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11
```

The deploy tool repeated the accepted non-blocking warnings for optional
shader compiler DLLs and an unset Visual C++ installation variable. They did
not affect required artifacts or exit status.

## Change inventory, findings, and telemetry

`TASK_005_CHANGES` are limited to the primary allow-list:

- top-level audio subdirectory integration and the new `audio/**` target/API;
- five additive audio errors and the existing dependency-rule extension;
- test registration, unit/golden tests, and audio extensions to the existing
  dependency fixture (including a cohesive test-only support header);
- the minimal README/architecture/task-log updates, telemetry, and this report.

There are no allow-list deviations. App, UI, QML, platform implementations,
presets, install/deploy logic, manifest generation, dependencies, and frozen
documents were not modified. No network/download, concrete reader, source
picker, `SourceResource`, metadata panel, writer, playback, DSP, checksum,
sample-rate conversion, or Task 006 artifact was introduced.

Two implementation defects were corrected before the final matrix: generated
audio test registration initially referenced the wrong WAV test source name,
and direct IEEE float initially omitted the valid 18-byte `WAVEFORMATEX` form.
A deliberately invalid no-read test resource was separately classified as
`FIXTURE_INVALID` and corrected to retain the required write-only permission;
it is not counted as a product defect. No algorithm-contract violation or
regression was found.

The host process again exposed the accepted `PATH`/`Path` case collision and
did not inherit `RGSML_QT_ROOT`. Only child-process environment keys were
normalized and the accepted local Qt prefix was supplied process-locally. No
machine/user setting or repository workaround was changed, and blocked time is
zero. Optional deploy warnings are host/deployment-tool observations, not
product defects.

Telemetry records eight implementation iterations, twenty observed
configure/build/test command cycles (including development and host-environment
retries), two corrected implementation defects, zero regressions, and zero
blocked hours. Owner/review hours remain zero because none were supplied; agent
elapsed time was not independently instrumented and is not invented.

The final staged diff is reviewed before the delivery commit. The branch is
left ready for review and is not merged to `main`. CODEX TASK 006 is neither
prepared nor started.
