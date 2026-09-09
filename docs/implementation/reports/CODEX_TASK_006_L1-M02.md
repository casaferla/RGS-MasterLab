# CODEX TASK 006 / L1-M02 implementation report

## Delivery status

- Status: `READY_FOR_REVIEW`
- Branch: `task/L1-M02-windows-source-resource`
- Delivery subject: `CODEX TASK 006 / L1-M02`
- Integration: `NOT_MERGED_TO_MAIN`
- Next task: `TASK_007_NOT_PREPARED_OR_STARTED`

The delivery commit identifier is reported externally after commit creation so
this file does not contain a circular self-reference.

## Task packet and Git preflight

The exact `RGS_MasterLab_CODEX_TASK_006.md` packet was found in the connected
Google Drive `Codex` folder and read in full before implementation. Its required
baseline matched `8a366ccd1f2c29fd07e48dae6eafd8a4da7cc621`.

The mandatory preflight verified:

| Evidence | Verified value |
|---|---|
| initial branch | `main` |
| baseline and initial HEAD | `8a366ccd1f2c29fd07e48dae6eafd8a4da7cc621` |
| baseline subject | `CODEX TASK 005 / L1-M01` |
| baseline parent | `04306e664b9c1add58645cbbc681e725bddc485f` |
| baseline tree | `6089b40c5341f49d2c1c97ba9198181cbc24b2c6` |
| Task 005 delivery commit | `0fe8c8366f3cea3fddaa5427d88e56cad1b83cda` |
| Task 005 delivery tree | `6089b40c5341f49d2c1c97ba9198181cbc24b2c6` |
| task branch | created directly from the baseline |

`initialGitStatus` and `PREEXISTING_WORKTREE_CHANGES` were exactly:

```text
?? RGS_MasterLab_CODEX_TASK_001-2.md
```

The file remained untracked, unstaged, and unchanged. Its initial and final
SHA-256 is:

```text
3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF
```

`main` remained at the baseline throughout implementation.

## Host environment handling

The inherited Windows environment exposed duplicate case-insensitive
`PATH`/`Path` entries and did not inherit `RGSML_QT_ROOT`. Every configure,
build, test, and install command normalized only its own process environment:
the duplicate process entries were consolidated, the selected CMake and Qt
`bin` directories were prepended, and `RGSML_QT_ROOT` was set for that process
and its children. No machine, user, repository, shell-profile, or global CMake
configuration was changed.

The accepted local toolchain remained CMake `3.31.6-msvc6`, MSVC
`19.44.35228.0` x64, Visual Studio Build Tools `17.14.39`, Qt `6.8.3`, Windows
SDK `10.0.26100.0`, and Git `2.53.0.windows.3`. No dependency was installed or
downloaded.

## Production implementation

### Windows read-only resource adapter

The only new production target is the C++20 static library `rgsml_platform`:

```text
rgsml_platform --PUBLIC--> rgsml_core
rgsml_platform --PRIVATE-> Qt6::Core
```

`rgsml::platform::windows::WindowsResourceReader` is a final implementation of
the frozen `rgsml::core::IResourceReader` interface. Its public header contains
only core and C++ standard-library types; a private implementation hides
`QFile`, `QFileInfo`, `QDir`, encoding, and operating-system details.

The provider identifier is exactly:

```text
rgsml.windows.local-file
```

The provider accepts strict UTF-8 absolute local drive paths, rejects empty,
relative, URL, UNC, embedded-NUL, invalid-UTF-8, directory, and missing-file
inputs with stable categories, normalizes separators and lexical dot segments,
and uppercases the drive letter. The resulting `ResourceReference` is read-only
and uses provider identifier plus canonical locator as logical identity.

Opening validates provider, read permission, absence of write permission,
canonical locator, existence, and regular-file status before opening with
read-only access. The endpoint reports only `CanSeek` and `HasKnownSize`.
Empty reads succeed without cursor movement, short reads and EOF report exact
counts, failed/out-of-range seeks preserve position, seek to EOF is valid,
close is idempotent, and fallible operations after close report
`InvalidState`. No writer or native handle is exposed.

### Audio-layer Source aggregate

`rgsml::audio::SourceResource` accepts ownership of one
`IResourceReader`, probes it through the existing frozen `WavReader`, closes the
reader after the probe, and retains exactly:

- the immutable `ResourceReference`;
- the immutable `WavStreamInfo`.

It owns no open stream, decoded samples, `AudioBuffer`, checksum, revision,
project state, playback state, DSP state, or mutable metadata. Existing RIFF,
RF64, signed-zero, subnormal, accepted/rejected WAV, and bit-exact decoder
contracts are reused without modification.

### Transactional Qt Source selection

`SourceSelectionViewModel` is application-layer composition. It converts a
local `QUrl` to the Windows provider reference, opens the adapter, probes a
candidate `SourceResource`, and publishes only presentation-ready metadata.
The replacement is transactional:

- cancel is a complete no-op;
- invalid URL, missing file, access, or WAV failure publishes an actionable
  sanitized error and preserves the previous accepted Source;
- a later valid candidate atomically replaces the Source and clears the error.

The stable view state is `NO_SOURCE`, `SOURCE_READY`, or `SOURCE_ERROR`.
Metadata includes display name, RIFF/RF64 container, frozen PCM/IEEE sample
format label, exact sample rate, mono/stereo channel layout and count, frame
count, rounded display duration, and an explicit read-only flag. No private
locator is surfaced in the UI error text.

The QML shell adds a single-file `QtQuick.Dialogs.FileDialog` filtered to
`*.wav`/`*.wave`, empty/ready/error presentation, and a read-only metadata
panel. There are no transport, playback, waveform, checksum, analysis, writer,
save, or persistence controls.

## Dependency contracts

The reusable dependency rule now permits a Qt edge only when Qt is not listed
among the caller's forbidden layers. Existing core and audio rules continue to
forbid Qt. The new platform rule allows `rgsml_platform -> rgsml_core` plus its
private Qt Core implementation edge, while forbidding Audio, DSP, QML, and UI.

Five platform-specific dependency tests cover the allowed graph, standalone
public header, forbidden public include/type scan, positive fixture, and the
canonical expected failure for `rgsml_audio -> rgsml_platform`. Existing core
and audio dependency tests remain unchanged in meaning and green.

The root CMake file was allow-listed primarily for adding the platform
subdirectory. One strictly necessary integration extension was also made there:
`QuickDialogs2` was added to the already centralized Qt component discovery.
This is required by the packet's deployed `QtQuick.Dialogs.FileDialog`; keeping
discovery centralized avoids a directory-scope imported-target failure. The UI
target links that exact module, so staged deployment includes its QML and DLL
runtime. No other allow-list deviation was made.

## Automated test inventory and acceptance

The baseline inventory was 19 enabled CTests. The final inventory is 28; all 19
pre-existing tests remain present and enabled. The nine additions are:

```text
rgsml_source_resource_tests
rgsml_windows_resource_reader_tests
rgsml_source_selection_tests
rgsml_source_metadata_panel_smoke
platform_dependency_graph_allowed
platform_public_headers_standalone
platform_forbidden_public_contract
platform_dependency_rule_negative_fixture
platform_dependency_rule_positive_fixture
```

The platform tests cover Unicode and spaces, lexical canonicalization,
read-only permissions/capabilities, missing/foreign/writable references,
empty/full/short/EOF reads, seek and failed-seek preservation, idempotent close,
post-close failures, absence of write API, and byte/hash immutability.
`SourceResource` tests cover RIFF/RF64 metadata, representative accepted
encodings, bounded probe behavior, reader release/close, and categorical
failures. Selection tests cover all frozen PCM16/24/32 and IEEE F32/F64
mono/stereo labels plus success, cancel, failure preservation, and retry. The
QML smoke covers empty/ready/error states, metadata, read-only presentation,
and a live window resize.

| Command group | Exit/result |
|---|---|
| Git baseline/branch/tree/hash preflight | PASS |
| fresh `cmake --preset windows-msvc --fresh` | exit 0 |
| Debug test inventory | 28 enabled tests |
| clean Debug build, warnings-as-errors | exit 0, zero RGSML warnings |
| complete Debug CTest | 28/28 PASS |
| clean Release build, warnings-as-errors | exit 0, zero RGSML warnings |
| Release test inventory | 28 enabled tests |
| complete Release CTest | 28/28 PASS |
| Debug label `platform` | 2/2 PASS |
| Debug label `source_resource` | 2/2 PASS |
| Debug label `ui_smoke` | 1/1 PASS |
| Debug label `audio` | 4/4 PASS |
| Debug label `audio_golden` | 1/1 PASS |
| Debug label `dependency_contract` | 15/15 PASS |
| tests-off configure/build/audit | PASS; 0 tests and no test targets |
| install to `stage/L1-M02/Release` | exit 0 |
| staged `RGSMasterLab.exe --rgsml-deploy-smoke` | exit 0; no QML/RGSML error |
| build/install manifest byte comparison | PASS |

The generated and installed manifests are byte-identical and retain the frozen
SHA-256:

```text
49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11
```

The deploy tool emitted only the previously accepted optional
`dxcompiler.dll`/`dxil.dll` and unset `VCINSTALLDIR` notices. Required runtime,
platform plugin, Quick Controls, Quick Dialogs, and QML artifacts were deployed;
the notices did not affect exit status.

## Manual functional gate

The actual staged Release executable was exercised through the Windows UI with
three disposable files under the ignored build tree. Pre/post evidence was:

| Fixture basename | Bytes | SHA-256 before and after |
|---|---:|---|
| `Ünicode Source.wav` | 50 | `FDA6A8995067C52E776F98A67C20E205868825F65E2575D5ED67E6D6B07C6D79` |
| `Malformed.wav` | 12 | `20A6E2470FD52AA8E1BC348E850C987592F1ADDE7521EE4723A770E18D82B76E` |
| `Retry RF64.wav` | 88 | `90134A54DC15E80A9310D70045F560BDE8C2873AF2ED523F493357A5519AAA33` |

Observed results:

- initial empty state and WAV-only picker: PASS;
- picker cancel leaves `No Source selected`: PASS;
- valid path with spaces and Unicode loads RIFF/WAVE, PCM 16-bit, 44100 Hz,
  mono C, 3 frames, duration `0:00.000`, and `Read-only`: PASS;
- malformed selection shows `Invalid or incomplete WAV file. Choose another
  Source.` while all previous valid metadata remains present: PASS;
- retry with a valid RF64 replaces the Source, clears the error, and displays
  RF64/WAVE, IEEE float 64-bit, 48000 Hz, mono C, and 1 frame: PASS;
- system resize leaves the shell responsive and its metadata intact: PASS;
- minimize and restore: PASS;
- normal close: PASS, exit code 0;
- separate normal launch with captured diagnostics: exit 0, stdout length 0,
  stderr length 0; no QML error or relevant RGSML warning.

## Findings, rework, and scope audit

Four implementation findings were corrected before final acceptance: eager
materialization of strict UTF-8 decoder output, C++ name hiding in helper calls,
explicit Quick Dialogs deployment linkage, and context-property lifetime during
QML engine teardown. The temporary Qt offscreen smoke backend hung on this
host; the final automated smoke uses the normal Windows backend and app-local
QML import path. These were implementation/test-harness corrections, not frozen
contract changes. No regression remains.

Final static and staged audits confirm:

- core and audio public/implementation boundaries remain free of Qt, Win32,
  filesystem, and file-stream dependencies;
- the platform public header contains no Qt, Win32, filesystem, stream, native
  handle, writer, or mutable API;
- there is no Task 007 playback/device/render/callback implementation;
- generated build, deploy, and manual fixtures remain ignored and unstaged;
- only allow-listed Task 006 files are staged;
- `git diff --check` passes;
- `main` is unchanged and the pre-existing untracked packet is intact.

Telemetry is recorded in `docs/implementation/telemetry/L1-M02.json`. Owner and
review hours remain zero because none were supplied; no blocked time is
invented. The branch is left ready for review and is not merged to `main`.
CODEX TASK 007 is neither prepared nor started.
