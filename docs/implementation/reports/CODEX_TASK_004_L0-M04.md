# CODEX TASK 004 / L0-M04 implementation report

## Delivery status

- Status: `READY_FOR_REVIEW`
- Branch: `task/L0-M04-platform-contracts`
- Delivery subject: `CODEX TASK 004 / L0-M04`
- Integration: `NOT_MERGED_TO_MAIN`
- Next task: `TASK_005_NOT_PREPARED_OR_STARTED`

The delivery commit identifier is intentionally reported externally after the
commit is created, avoiding a circular self-reference in this report.

## Git preflight and preserved worktree

The mandatory preflight completed before editing with the following verified
state:

| Evidence | Verified value |
|---|---|
| initial branch | `main` |
| baseline and initial HEAD | `84d97d5e80295893f20f39e81d733693b07071d4` |
| baseline subject | `CODEX TASK 003 / L0-M03` |
| baseline parent | `f44d15bb5a187e7119151e940f0114dd1b5c5770` |
| baseline tree | `5a64cd6bef018edaab1e3c81323630f377356e04` |
| Task 003 delivery commit | `1b0ec0a9deeec47eef9949c96182b9d5d0fca8b4` |
| Task 003 delivery tree | `5a64cd6bef018edaab1e3c81323630f377356e04` |
| task branch | created directly from the baseline |

`initialGitStatus` and `PREEXISTING_WORKTREE_CHANGES` are both exactly:

```text
?? RGS_MasterLab_CODEX_TASK_001-2.md
```

Its initial and final SHA-256 is:

```text
3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF
```

The file remained untracked, unstaged, and unchanged. `main` remained at the
baseline throughout implementation.

## Implemented contracts

All runtime symbols are owned by `rgsml::core`; future concrete adapters depend
inward on these contracts. No production resource implementation or playback
state machine was added.

| Public interface/artifact | Header | Ownership and dependencies | Contract tests |
|---|---|---|---|
| `ResourceReference`, `ResourcePermissions` | `resource_reference.hpp` | `rgsml_core`; C++20 standard library only | validation, opaque preservation, permission/display-independent identity, conservative output-distinctness |
| `ResourceCapability`, `ResourceCapabilities` | `resource_io.hpp` | `rgsml_core`; permissions remain a separate type | supported/unsupported seek, known size, resize and flush |
| `IResourceReader` | `resource_io.hpp` | inward-facing abstract byte port | EOF, empty/full/short/partial-before-failure, cursor, permission, seek, close and invalid state |
| `IResourceWriter` | `resource_io.hpp` | inward-facing abstract byte port | empty/full/partial/no-progress, overwrite, resize, flush, permission, close and invalid state |
| `PlaybackState`, `PlaybackSnapshot`, `IAudioPlaybackService` | `audio_playback_service.hpp` | inward-facing resource-based control port using Task 003 frame types | prepare/failure preservation, play/pause/resume/stop/clear, seek and loop validation |
| `rgsml_assert_target_dependencies` | `RgsmlDependencyRules.cmake` | reusable transitive CMake target-graph rule | allowed graph plus positive and expected-negative fixtures |

`providerId` uses the documented ASCII grammar
`[a-z][a-z0-9._-]*`. The locator is stored and compared opaquely; core performs
no path, URI, separator, case, encoding, filesystem, or handle interpretation.
Logical resource identity is exactly provider identifier plus locator after any
provider-owned canonicalization. Display metadata and permissions do not alter
identity, while a mismatching identity does not prove physical-document
distinctness.

Permissions express authorization (`read`, `write`). Capabilities independently
describe an opened endpoint (`CanSeek`, `HasKnownSize`, `CanResize`, `CanFlush`).
Reader/writer ports are blocking, sequential-first, serialized-owner contracts.
Short transfers report exact progress; non-empty writer no-progress is a
categorical failure. Failed seek/resize operations preserve state. Close is
idempotent, and post-close fallible operations report invalid state. Flush and
close do not promise durability, atomic commit, rename, validation, or
publication.

Playback is a control-plane contract only. Its snapshot contains only Task 003
value types; the test-only fake proves `NO_SOURCE`, `STOPPED`, `PLAYING`, and
`PAUSED` transitions, state preservation on failure, absolute non-negative
seek, and non-empty half-open loop semantics. No decoder, sample format, PCM
storage, audio device, timer, callback, or render authority was introduced.

## Additive error taxonomy

The existing Task 003 error semantics were not renamed or changed. These five
categories and canonical round-trip tokens were appended:

| `ErrorCode` | Canonical token |
|---|---|
| `ResourceNotFound` | `resource_not_found` |
| `AccessDenied` | `access_denied` |
| `IoFailure` | `io_failure` |
| `UnsupportedOperation` | `unsupported_operation` |
| `InvalidState` | `invalid_state` |

No operating-system or framework error value is a canonical category.

## Dependency-rule evidence

The final CTest inventory increased from the five accepted tests to eleven; all
five previous names remain present and enabled. The six additions are the port
contract executable and the five required dependency cases:

```text
rgsml_platform_contract_tests
core_dependency_graph_allowed
core_public_headers_standalone
core_forbidden_include_contract
dependency_rule_negative_fixture
dependency_rule_positive_fixture
```

The graph rule recursively inspects target dependencies and rejects framework
target names or targets explicitly classified in the forbidden Qt, QML,
platform, DSP, or UI layers. The synthetic negative fixture was accepted only
after configure failed with the verified diagnostic:

```text
RGSML_DEPENDENCY_RULE_VIOLATION: target 'dependency_consumer' reaches forbidden dependency 'fixture_forbidden' in layer 'QT'
```

The positive fixture compiled and linked standard-only fake implementations of
all three ports against `rgsml_core`. Each public core header was also compiled
as the sole project header in an independent minimal translation unit. The
directive/type scan passed. Source, include, and target-graph audits found no
Qt, QML, native Windows, Android/JNI, platform implementation, DSP, or UI
dependency in `rgsml_core`. Configuring with `RGSML_BUILD_TESTS=OFF` succeeded
without resolving `Qt6::Test`.

## Final acceptance

The accepted local toolchain remained CMake `3.31.6-msvc6`, MSVC
`19.44.35228.0` x64, Visual Studio Build Tools `17.14.39`, Qt `6.8.3`, Windows
SDK `10.0.26100.0`, and Git `2.53.0.windows.3`.

| Command group | Exit/result |
|---|---|
| Git baseline/branch/hash preflight | PASS |
| fresh `windows-msvc` configure | exit 0 |
| Debug test inventory | 11 enabled tests |
| clean Debug build, warnings-as-errors | exit 0, zero RGSML warnings |
| complete Debug CTest | 11/11 PASS |
| clean Release build, warnings-as-errors | exit 0, zero RGSML warnings |
| Release test inventory | 11 enabled tests |
| complete Release CTest | 11/11 PASS |
| explicit positive/negative dependency fixtures | PASS |
| install to the L0-M04 Release staging tree | exit 0 |
| staged `RGSMasterLab.exe --rgsml-deploy-smoke` | exit 0; no QML error or RGSML warning |
| `RGSML_BUILD_TESTS=OFF` configure probe | PASS; `Qt6::Test` absent |
| `git diff --check` | PASS |

The build and installed `ToolchainManifest.json` files were byte-identical and
both had the accepted SHA-256:

```text
49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11
```

The deploy tool repeated the accepted non-blocking diagnostics for optional
shader compiler DLLs and an unexported Visual C++ installation variable. They
did not affect required deployment artifacts or runtime exit status.

No network access, download, dependency installation, host configuration
change, machine-specific versioned path, frozen-document change, new runtime
dependency, or new production target occurred.

## Change inventory and review

`TASK_004_CHANGES` consists only of the following allow-listed areas:

- the three new public port headers and the `ResourceReference` implementation;
- additive error categories/mappings and `rgsml_core` source/rule integration;
- the reusable dependency-rule module, unit tests, CMake driver, and two fixture
  consumers;
- test registration, architecture note, task-log entry, telemetry, and this
  report.

There were no allow-list deviations. App, UI, QML, install/deploy rules,
toolchain manifest files, presets, dependency inventory, and accepted Task 003
primitive implementations were not modified.

Three implementation defects were found and corrected before the final clean
matrix: a heterogeneous test initializer list that MSVC rejected, diagnostic
matching that did not initially tolerate CMake line wrapping, and provisional
resource seek/resize method names that were then aligned to the packet's
`seek_bytes`/`resize_bytes` names. The deliberately failing negative fixture is
not counted as a defect. No regression was found.

The host process again exposed the accepted `PATH`/`Path` case collision and did
not inherit the Qt root variable. Only child-process environment keys were
normalized and the accepted local Qt prefix was supplied process-locally. No
host or repository workaround was persisted, and blocked time remained zero.

Telemetry records five implementation iterations, thirteen
configure/build/test cycles, three corrected defects, zero regressions, and
zero blocked hours. Owner and review hours remain zero because the product
owner supplied no measurements.

The final staged diff is reviewed before delivery commit. The branch is left
for review, not merged to `main`, and CODEX TASK 005 is neither prepared nor
started.
