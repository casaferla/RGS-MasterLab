# CODEX TASK 003 / L0-M03 implementation report

## Delivery

- Baseline: `f44d15bb5a187e7119151e940f0114dd1b5c5770`
- Verified baseline parent: `e032441a9df2e70a94b83c2a20d121f89baf5f97`
- Verified baseline subject: `CODEX TASK 002 / L0-M02`
- Branch: `task/L0-M03-core-primitives`
- Delivery commit subject: `CODEX TASK 003 / L0-M03`
- State: READY_FOR_REVIEW; NOT_MERGED_TO_MAIN;
  TASK_004_NOT_PREPARED_OR_STARTED

The delivery commit SHA is intentionally reported externally after the commit,
so this report can remain part of that single delivery commit.

## Git preflight and attribution

The initial Git evidence was:

```text
git status --short
?? RGS_MasterLab_CODEX_TASK_001-2.md

git branch --show-current
main

git rev-parse HEAD
f44d15bb5a187e7119151e940f0114dd1b5c5770

git rev-parse main
f44d15bb5a187e7119151e940f0114dd1b5c5770

git show -s --format="%H%n%P%n%s" HEAD
f44d15bb5a187e7119151e940f0114dd1b5c5770
e032441a9df2e70a94b83c2a20d121f89baf5f97
CODEX TASK 002 / L0-M02
```

No local or remote branch named `task/L0-M03-core-primitives` existed. The task
branch was created directly from the verified baseline, and the merge-base
ancestor check passed.

- PREEXISTING_WORKTREE_CHANGES:
  `RGS_MasterLab_CODEX_TASK_001-2.md` remains untracked and unstaged. Its SHA-256
  before and after implementation is
  `3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF`.
- TASK_003_CHANGES: only the source, tests, CMake registration, architecture
  note, task log, telemetry, and report listed below.

## Public API and frozen-contract mapping

All runtime symbols are owned by `rgsml::core`.

| Public type/function | Header | Frozen contract | Test evidence |
|---|---|---|---|
| `ErrorCode`, `Error`, `ErrorDetail` | `error.hpp` | Stable categorical tokens; deterministic ordered details; no success code | Token round-trip, invalid token, code propagation, detail order |
| `Result<T>`, `Result<void>`, `Status` | `result.hpp` | Exactly one value/error; explicit factories; pointer observers; move-only support; no default state | Success/failure exclusivity, const/non-const observers, move-only and type traits |
| `Uuid` | `uuid.hpp` | Exact 16-byte value; strict hyphenated parse; lowercase format; no random generation | Known/nil/mixed-case/00/ff round-trip, ordering, byte order, invalid matrix |
| `StrongId<Tag>` | `strong_id.hpp` | Compile-time tag separation; non-nil UUID; underlying UUID is persisted identity | Nil rejection, value exposure, cross-tag `static_assert` |
| Checked integer free functions | `checked_integer.hpp` | Checked signed add/subtract/multiply/negate/increment and frozen floor/ceil/mod/next-even | Sign matrix, boundary vectors, invariants, invalid inputs, overflow adjacency |
| `Rational` | `rational.hpp` | Reduced signed numerator, positive denominator, exact ordering/arithmetic, strict canonical text | Normalization, `INT64_MIN`, parser matrix, overflow-safe comparison, wide cancellation, small property grid |
| `SampleRate`, `FrameIndex`, `FrameCount`, `FrameRange`, `RationalTime` | `frame_time.hpp` | Strong units, half-open ranges, checked length, exact frame/count-to-time | Rates, signed indices, count validation, empty/reverse/overflow ranges, exact conversions, type traits |
| `ProcessingState` | `processing_state.hpp` | Exactly `RAW`, `PREPARED`, `PROCESSED`, `GOLD`; case-sensitive tokens | Four-token round-trip and empty/lowercase/`MASTERED`/unknown rejection |

The foundational `ErrorCode` tokens are:

```text
invalid_argument
out_of_range
integer_overflow
division_by_zero
parse_failure
invalid_uuid
invalid_rational
invalid_frame_range
```

## Failure and arithmetic semantics

Expected validation, parse, range, overflow, and division failures return
`Result`/`Status`; public accessors return conditional pointers and do not throw
for normal domain failure. Messages and ordered details are diagnostic only;
the `ErrorCode` token is the stable categorical identity.

Checked signed operations test bounds before evaluation, so they do not wrap,
saturate, or trigger signed overflow. Floor/ceiling/modulo use a positive-divisor
quotient/remainder formulation that never negates `INT64_MIN`. `next_even`
rejects negative values and reports overflow for odd `INT64_MAX`.

`Rational` normalization computes unsigned magnitude without `abs(INT64_MIN)`.
Ordering uses continued fractions, avoiding cross products. Addition/subtraction
uses a portable two-limb unsigned product only for exact intermediate magnitude,
then removes the shared denominator GCD before the final 64-bit range checks.
Multiplication/division cross-cancel before multiplying. This implementation
uses no floating point, `__int128`, compiler intrinsic, or multiprecision
dependency. Overflow and division by zero remain categorical failures.

## Files changed and allow-list review

- Core build registration: `core/CMakeLists.txt`.
- Public headers: `error.hpp`, `result.hpp`, `uuid.hpp`, `strong_id.hpp`,
  `checked_integer.hpp`, `rational.hpp`, `frame_time.hpp`, and
  `processing_state.hpp` below `core/include/rgsml/core/`.
- Implementations: `error.cpp`, `uuid.cpp`, `checked_integer.cpp`,
  `rational.cpp`, `frame_time.cpp`, and `processing_state.cpp` below `core/src/`.
- Tests: `tests/CMakeLists.txt`, `tests/test_core_primitives.cpp`, and eight
  independent public-header compile probes below `tests/unit/core/`.
- Documentation/evidence: `docs/architecture/README.md`,
  `docs/implementation/task-log.md`, `docs/implementation/telemetry/L0-M03.json`,
  and this report.

`tests/test_bootstrap.cpp` is the only conditionally allowed file outside the
primary list. Its minimal change adds one QtTest slot that invokes the new core
suite inside the existing `rgsml_tests` executable; this was technically
necessary to extend, rather than replace or duplicate, the accepted runner.
No target was renamed and no new production or test framework dependency was
introduced. There are no other allow-list deviations.

## Test and acceptance evidence

Toolchain prerequisites were rechecked: CMake `3.31.6-msvc6`, MSVC
`19.44.35228.0` x64 from Visual Studio Build Tools 2022 `17.14.39`, Windows SDK
`10.0.26100.0`, Qt `6.8.3` MSVC 2022 64-bit, and Git
`2.53.0.windows.3` were available. No dependency was installed or downloaded.

The final commands and results were:

```text
git preflight/status/branch/HEAD/main/parent/subject             PASS
cmake --preset windows-msvc --fresh                             PASS (exit 0)
cmake --build --preset windows-msvc-debug-clean                 PASS (exit 0)
ctest --preset windows-msvc-debug --output-on-failure           5/5 PASS
cmake --build --preset windows-msvc-release-clean               PASS (exit 0)
ctest --preset windows-msvc-release --output-on-failure         5/5 PASS
cmake --install ... --config Release --prefix .../L0-M03/Release PASS
staged bin/RGSMasterLab.exe --rgsml-deploy-smoke                PASS (exit 0)
rgsml_tests corePrimitives                                      3/3 PASS (filter)
RGSML_BUILD_TESTS=OFF configure with Qt6Test disabled           PASS; 0 tests
git diff --check                                                PASS
```

The `RGSML_BUILD_TESTS=OFF` configure reported the defensive
`CMAKE_DISABLE_FIND_PACKAGE_Qt6Test` variable unused and generated no tests,
confirming that `Qt6::Test` was not requested. The test filter contained
`initTestCase`, `corePrimitives`, and `cleanupTestCase`, all passing.

CTest inventory was five tests before and five after in both Debug and Release:

```text
rgsml_tests
toolchain_manifest_synthetic_contract
toolchain_manifest_current_contract
warnings_policy_contract
install_deploy_smoke
```

The existing `BuildInfo` assertion remains in `rgsml_tests`; no pre-existing
test was removed, renamed, or weakened. All public headers are separately
compiled by dedicated translation units. Debug and Release clean builds used
warnings-as-errors and emitted zero RGSML warnings.

## Manifest, deployment, and dependency audit

The build and installed manifests were byte-identical. Both SHA-256 values were:

```text
49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11
```

The explicit staged smoke loaded the deployed application and QML imports and
exited `0`, with no QML error or RGSML warning. `windeployqt` repeated the
accepted Task 002 diagnostics for optional `dxcompiler.dll`/`dxil.dll` and an
unexported `VCINSTALLDIR`; these do not affect required artifacts or runtime.

Audits across `core` source/include and the `rgsml_core` target found no Qt,
QML, Win32, COM, Android, floating-point-authoritative, `__int128`, or platform
link dependency. `rgsml_core` remains C++20 standard-library-only. No FrameMap,
SRC, audio, resource/playback interface, domain-specific ID, I/O, persistence,
or Task 004 artifact was introduced.

## Findings and telemetry

The host process exposed both `Path` and `PATH`, and did not inherit
`RGSML_QT_ROOT`. Acceptance therefore normalized only child-process environment
keys and supplied the selected Qt kit prefix process-locally. No machine/user
environment, registry, Git configuration, antivirus policy, preset,
machine-specific path, or versioned workaround changed. These are host rework
findings, not product defects. The deployment diagnostics described above are
unchanged from Task 002.

Five implementation iterations and nine configure/build/test cycles were
recorded. Final review found and corrected two self-containment defects: public
headers using three-way comparison now include `<compare>` directly, and
including `error.hpp` alone now leaves `Result<ErrorCode>` complete for immediate
use of `parse_error_code`. Dedicated compile probes cover both corrections. The
complete clean matrix was rerun afterward. No regression or blocker was found,
and blocked time is zero. `actualOwnerHours` and `reviewHours` remain zero
because the product owner did not provide them. Telemetry status is
`READY_FOR_REVIEW`.

`main` remains at the official baseline. The work is not merged, and CODEX TASK
004 was neither prepared nor started.
