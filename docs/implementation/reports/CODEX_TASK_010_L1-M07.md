# CODEX TASK 010 / L1-M07 — Implementation report

## Outcome

`CODEX TASK 010 / L1-M07 — Canonical Module Registry, Module Instance, and
Dynamic Mastering Chain` is complete on
`task/L1-M07-module-registry-dynamic-chain` and ready for owner review.
`main` remains at the authoritative baseline and is not merged. Task 011 was
neither prepared nor started.

The delivery adds structural DSP contracts only: a static Qt/platform-free
`rgsml_dsp` library, the exact `IModule` boundary, immutable descriptors, one
canonical eleven-entry descriptor-only registry, stable module-instance
identity, and an atomic deterministic processing chain. It contains no DSP
mathematics, Gain execution, production factory, production processor, UI,
persistence, render/export, or format expansion.

## Git and authority evidence

| Check | Observed | Result |
|---|---|---|
| baseline and resume `HEAD` | `830b8b9b54ce4a46d0b41a2705f87e6446f37c55` | PASS |
| `main` | `830b8b9b54ce4a46d0b41a2705f87e6446f37c55` | PASS |
| branch | `task/L1-M07-module-registry-dynamic-chain` | PASS |
| preserved Task 009 branch | `798a36ba8037d154156bf64b59110eb7f0b23a84` | PASS |
| historical untracked SHA-256 | `3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF` | PASS |
| Task 010 packet SHA-256 | `A8438DF22589544BB64F087DD696F36B5EE4253C7B771D86932C1280FEBBB845` | PASS |
| forward-delta SHA-256 | `729962B4F7BC3A7EE1949A851302D04631C0643378B41EE23A59A67DBB85B5D4` | PASS |

The worktree remained classified as:

- `PREEXISTING_WORKTREE_CHANGES`: only
  `RGS_MasterLab_CODEX_TASK_001-2.md`, untracked, unstaged, and unchanged;
- `TASK_010_CHANGES`: only the L1-M07 allow-listed paths enumerated below.

No reset, restore, stash, clean, rebase, destructive checkout, branch
recreation, dependency installation, or history rewrite occurred.

The authoritative status is retained exactly:

```text
BASELINE_CONFLICT_DISCOVERED — RESOLVED_BY_TARGETED_FORWARD_DELTA
TARGETED_DELTA_REQUIRED — MATERIALIZED
NEW_PRODUCT_DECISION_REQUIRED — NO
```

## Changed paths and allow-list mapping

Modified:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/cmake/dependency_rules_fixture/CMakeLists.txt`
- `docs/architecture/README.md`
- `docs/implementation/task-log.md`

Added:

- `dsp/CMakeLists.txt`
- `dsp/include/rgsml/dsp/imodule.hpp`
- `dsp/include/rgsml/dsp/module_descriptor.hpp`
- `dsp/include/rgsml/dsp/module_instance.hpp`
- `dsp/include/rgsml/dsp/module_registry.hpp`
- `dsp/include/rgsml/dsp/processing_chain.hpp`
- `dsp/src/internal/revision.hpp`
- `dsp/src/module_descriptor.cpp`
- `dsp/src/module_registry.cpp`
- `dsp/src/processing_chain.cpp`
- `tests/cmake/dependency_rules_fixture/dsp_allowed_consumer.cpp`
- `tests/cmake/dependency_rules_fixture/test_dsp_dependency_rules.cmake`
- `tests/unit/dsp/test_support.hpp`
- `tests/unit/dsp/test_module_descriptor.cpp`
- `tests/unit/dsp/test_module_registry.cpp`
- `tests/unit/dsp/test_processing_chain.cpp`
- `tests/unit/dsp/test_structural_integration.cpp`
- `docs/implementation/telemetry/L1-M07.json`
- `docs/implementation/reports/CODEX_TASK_010_L1-M07.md`

Conceptual-to-actual mappings:

| Packet concept | Actual path |
|---|---|
| structural DSP target | `dsp/CMakeLists.txt` |
| exact execution boundary | `dsp/include/rgsml/dsp/imodule.hpp` |
| descriptor schema | `dsp/include/rgsml/dsp/module_descriptor.hpp` |
| instance structural value | `dsp/include/rgsml/dsp/module_instance.hpp` |
| registry public contract | `dsp/include/rgsml/dsp/module_registry.hpp` |
| chain public contract | `dsp/include/rgsml/dsp/processing_chain.hpp` |
| descriptor validation | `dsp/src/module_descriptor.cpp` |
| catalog and registry | `dsp/src/module_registry.cpp` |
| topology and atomic mutations | `dsp/src/processing_chain.cpp` |
| focused private checked revision helper | `dsp/src/internal/revision.hpp` |
| typed instance ID | existing generic `core::StrongId`, specialized in `module_instance.hpp` |

No optional catalog implementation file or core typed-ID extension was needed.
Protected app/UI/audio/platform/render/analysis/project/reference/DNA/fixture
data/third-party/prior evidence/RF-1/ToolchainManifest input surfaces are
unchanged.

## Public API and dependency boundary

The public API uses only the newly frozen names `IModule`,
`ModuleDescriptor`, `ModuleInstanceId`, `ModuleRegistry`, and
`ProcessingChain`; no legacy alias or parallel API was added.

`IModule` and its support structures preserve the exact unique ownership,
borrowed-view lifetime, disjoint input/output, prepare/reset/process,
non-reentrancy, and steady-state allocation contracts from
`L1-M07-MODULE-CONTRACT-FORWARD-DELTA-001`. Public headers compile standalone
and expose no Qt, QML, Win32, platform, filesystem, network, loader, plugin,
serialization, or UI types.

`rgsml_dsp` is a static library with this production direction:

```text
rgsml_dsp -> rgsml_audio -> rgsml_core
rgsml_dsp -> rgsml_core
```

Reverse `core -> dsp` and `audio -> dsp` edges, plus DSP reachability to Qt,
QML, platform, UI, project, or render layers, are rejected by deterministic
negative fixtures.

## Exact descriptor-source map

The canonical registry is built only in `dsp/src/module_registry.cpp` from the
complete rows frozen by forward-delta Sections 8.1–8.3:

| Type ID | Source/status | Algorithm/schema |
|---|---|---|
| `rgsml.dsp.dc-offset` | Phase 5 identity | absent/absent |
| `rgsml.dsp.declip` | Phase 5 identity | absent/absent |
| `rgsml.dsp.dehum` | Phase 5 identity | absent/absent |
| `rgsml.dsp.gain` | Phase 5 identity; execution deferred | absent/absent |
| `rgsml.dsp.parametric-eq` | Phase 5 identity; L1-M11 execution | absent/absent |
| `rgsml.dsp.compressor` | Phase 5 identity; L1-M13 execution | absent/absent |
| `rgsml.dsp.stereo-ms` | Phase 5 identity; L1-M15 execution | absent/absent |
| `rgsml.dsp.true-peak-limiter` | Phase 5 identity; L1-M17 execution | absent/absent |
| `rgsml.dsp.dither` | Phase 5 identity; L1-M18 execution | absent/absent |
| `rgsml.dsp.dynamic-eq` | descriptor-only Phase 7 promotion | `1.0.0` / `rgsml.dsp.dynamic-eq.parameters/1.0.0` |
| `rgsml.dsp.transient-shaper` | descriptor-only Phase 7 promotion | `1.0.0` / `rgsml.dsp.transient-shaper.parameters/1.0.0` |

The registry canonicalizes set-like fields and records by unsigned-ASCII
ordering, validates stable IDs, duplicate-free required sets, enum values,
stage/segment/placement consistency, references, contradictions, and hard
cycles, then exposes immutable lookup/enumeration. Advisory cycles are valid.

Observed registry counts:

```text
registeredDescriptorCount = 11
registeredProductionFactoryCount = 0
registeredProductionProcessorCount = 0
```

Every canonical execution resolution returns
`MODULE_IMPLEMENTATION_UNAVAILABLE`; no identity/pass-through fallback audio
or hidden availability flag exists.

## ModuleInstance and ProcessingChain invariants

Each `ModuleInstance` owns a stable typed instance ID, a canonical descriptor
type ID, enabled/bypassed state, structural owner, and link state. Public
mutations create only manual/unlinked instances. Those facts remain
independent; they are not inferred from one another.

`ProcessingChain` binds a registry and exact stage/segment context. Add,
remove, move, duplicate, and bypass operations construct and validate a
candidate before publication. On failure the original sequence, instance
state, and revision remain unchanged. On success a checked `uint64_t`
revision advances exactly once; exhaustion is categorical and non-mutating.

Validation covers descriptor existence, stage/segment admission, inline versus
terminal placement, terminal-slot order, `must_be_last`, single-active
constraints, duplicability, bypassability, stable instance identity, hard
precedence/following edges, and ownership/link restrictions. Recommendations
remain advisory and cannot reject a chain.

## Deterministic test matrix

Nine tests were added to the 41-test baseline:

- four unit/integration executables for descriptor validation, registry,
  processing chain, and structural integration;
- five dependency-contract cases for graph allowability, standalone public
  headers, forbidden public contracts, positive consumer, and the complete
  negative layer matrix.

Deterministic coverage includes invalid descriptor fields and enum values,
canonical ordering, duplicates, references, hard/advisory cycles, exact
eleven-row catalog values, all-unavailable factories, stable instance IDs,
independent state, every chain mutation, atomic failure rollback, revision
overflow, stage/segment/placement constraints, terminal ordering,
must-be-last, single-active, non-duplicable/non-bypassable cases, and
public-header/dependency boundaries.

## Automated acceptance

| Verification | Result |
|---|---|
| fresh `windows-msvc` configure | PASS |
| registered test inventory | 50 |
| clean Debug warnings-as-errors build | PASS |
| complete Debug CTest | 50/50 PASS |
| clean Release warnings-as-errors build | PASS |
| complete Release CTest | 50/50 PASS |
| Debug focused `dsp` | 4/4 PASS |
| Debug focused `module_registry` | 2/2 PASS |
| Debug focused `processing_chain` | 2/2 PASS |
| Release focused `dsp` | 4/4 PASS |
| Release focused `module_registry` | 2/2 PASS |
| Release focused `processing_chain` | 2/2 PASS |
| dependency-contract Debug | 22/22 PASS |
| dependency-contract Release | 22/22 PASS |
| standalone public headers | PASS |
| positive/negative dependency fixtures | PASS |
| tests-off clean Release build | PASS |
| tests-off Qt6::Test request/link scan | PASS — absent |
| staged Release deployment | PASS |
| staged deploy smoke | PASS — exit code 0 |
| manual gate | NOT_REQUIRED_C_PLUS_U |

The complete matrices retain all 41 accepted baseline tests. No RGSML warning,
QML error, regression, dependency download, machine-specific path, or product
configuration change was introduced.

The ToolchainManifest evidence is:

```text
build SHA-256     = 49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11
installed SHA-256 = 49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11
byte comparison   = identical
```

## Scope, protected surfaces, and zero-implementation proof

Final searches and dependency fixtures confirm:

- no DSP mathematics, sample transforms, coefficients, parameters, Gain
  processing, production factory, or production processor;
- no Qt/QML/Win32/platform/UI/project/render/network/filesystem/plugin-loader
  dependency in `rgsml_dsp`;
- no alternate codec or AIFF/FLAC/MP3/AAC/M4A/FFmpeg/format dispatch;
- no `.rgsml`/`.rgsm1`, Gold, Reference Match, render/export, Final Output
  SRC, Quantizer, WavWriter, or Task 011 artifact;
- Task 007 playback/SRC, Task 008 waveform summary/pyramid, Task 009
  navigation/region/editor, all prior evidence, RF-1, and frozen phase
  material remain unchanged.

Input format scope remains `WAV_ONLY`.

## Defects, deviations, and host observations

Two implementation-phase defects were corrected before the final matrices:

1. a test comparison attempted to pass a non-owning `std::span` directly to a
   Qt comparison helper; it now performs an explicit range equality check;
2. factory identity mismatch now reports the required
   `MODULE_IMPLEMENTATION_UNAVAILABLE` category.

Additional deterministic assertions were added for advisory cycles, empty
context fields, terminal placement, non-duplicable/non-bypassable descriptors,
and duplicated-state independence. No product regression remains.

Overlapping host build processes once caused transient MSVC PDB contention.
The final clean builds were run serially with a process-local parallelism
setting. The command runner later required an out-of-sandbox invocation after
a setup-refresh failure. A Release dependency run spanning the usage
interruption timed out only the pre-existing audio/Multimedia negative fixture;
its targeted retry passed 1/1 in 5.72 seconds. None of these host events changed
machine, global, repository, or product configuration. No Avast detection or
quarantine was observed.

There are no authorized-scope deviations or residual Task 010 blockers.
Interrupted-session time data was not reconstructed; telemetry keeps those
fields null rather than fabricating values.

## Risk, rollback, and stop boundary

Risk states remain:

```text
R12-01 — INHERITED UNCHANGED
R12-02 — MITIGATED_FOR_WINDOWS_L1-M05 / RESIDUAL_MONITORING_OPEN
RF-1 — MATERIALIZED; FROZEN ESTIMATES UNCHANGED
ALT_F4_ACTIVE_CAPTURE_RESIDUAL — PO_ACCEPTED_DEFERRED / NON-BLOCKING
```

The rollback unit is the complete Task 010 delivery commit on
`task/L1-M07-module-registry-dynamic-chain`. Reverting it removes only
`rgsml_dsp`, additive Task 010 CMake/test/dependency registration, and Task 010
documentation/evidence, restoring the baseline tree while preserving the
historical untracked file and all branches. No project, audio, cache, or user
data migration exists.

Final state:

```text
CODEX TASK 010 — READY FOR REVIEW
MILESTONE — L1-M07
BASELINE — 830b8b9b54ce4a46d0b41a2705f87e6446f37c55
BRANCH — task/L1-M07-module-registry-dynamic-chain
MAIN — UNCHANGED / NOT MERGED
INPUT FORMAT SCOPE — WAV_ONLY
DSP PACKAGE V1 — 11 DESCRIPTORS / STRUCTURAL ONLY
PRODUCTION DSP FACTORIES — 0
PRODUCTION DSP PROCESSORS — 0
CODEX TASK 011 — NOT PREPARED / NOT STARTED
```
