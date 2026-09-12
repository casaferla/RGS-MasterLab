# CODEX TASK 011 / L1-M08 — Implementation report

## Outcome

`CODEX TASK 011 — READY FOR REVIEW`

The accepted Gain Forward Delta is materialized exactly. `rgsml_dsp` now has
one executable production module, Gain-v1, and the canonical registry moves
from 11 descriptors / 0 factories / 0 processors to 11 / 1 / 1. The new
Qt-free/platform-free `rgsml_render` library provides bounded synchronous
same-rate identity/Gain preview over an immutable Source and chain snapshot.
All automated gates and the required human Listening Gate passed.

## Git and authority evidence

- Baseline and parent: `2e1bca98c589264c3a1b51e5367f19b425bd2975`
- Task branch: `task/L1-M08-gain-render-preview`
- Delivery subject: `CODEX TASK 011 / L1-M08`
- Packet SHA-256: `2E57E54C9B0DDAB8B5385C9B7903BA8B7BF898309537B1BDEBDFDE6E6FC6522C`
- Gain Forward Delta SHA-256: `758B0FE256BF4D43C30648CF32867436AAB38969065754556829F5F6644AF96F`
- Authority status: `MISSING_AUTHORITATIVE_DSP_INPUT — RESOLVED_BY_L1-M08_GAIN_FORWARD_DELTA`
- Historical untracked file SHA-256: `3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF`
- `main` remained at the baseline throughout task implementation.

`PREEXISTING_WORKTREE_CHANGES` contains only
`RGS_MasterLab_CODEX_TASK_001-2.md`; it remained untracked, unstaged, and
byte-identical. Every other changed path is a `TASK_011_CHANGE`.

## Actual path and API mapping

| Contract/artifact | Materialized path |
|---|---|
| `GainParameters` | `dsp/include/rgsml/dsp/gain_parameters.hpp`, `dsp/src/gain_parameters.cpp` |
| `GainModule` | `dsp/include/rgsml/dsp/gain_module.hpp`, `dsp/src/gain_module.cpp` |
| Gain factory/availability | `dsp/src/module_registry.cpp` |
| DSP target registration | `dsp/CMakeLists.txt` |
| `RenderRequest` | `render/include/rgsml/render/render_request.hpp`, `render/src/render_request.cpp` |
| `RenderResult` | `render/include/rgsml/render/render_result.hpp`, `render/src/render_result.cpp` |
| Render entry point | `render/include/rgsml/render/render_preview.hpp`, `render/src/render_preview.cpp` |
| Static render target | `render/CMakeLists.txt`, root `CMakeLists.txt` |
| Unit/integration tests | `tests/unit/dsp/`, `tests/unit/render/`, `tests/integration/render/` |
| Golden/oracle | `tests/audio_golden/gain/`, `tests/oracles/gain/` |
| Listening harness | `tests/manual/gain_preview/gain_listening_gate.cpp` |
| Dependency fixtures | `tests/cmake/dependency_rules_fixture/` |

Existing Task 010 public headers for `IModule`, `ModuleDescriptor`,
`ModuleInstance`, and `ProcessingChain` were not changed. The accepted registry
availability seam required only private factory wiring in
`dsp/src/module_registry.cpp`.

## Gain-v1 contract evidence

- `typeId = rgsml.dsp.gain`
- `algorithmVersion = 1.0.0`
- `parameterSchemaId = rgsml.dsp.gain.parameters/1.0.0`
- canonical binary64 `gainDb`, inclusive range `[-24.0, +24.0] dB`
- `-0.0` canonicalized to `+0.0`; NaN, infinity, and out-of-range rejected
- factor `10^(gainDb/20)` computed once per immutable parameter/module state
- 0 dB exact factor `1.0` and bit-exact copy after finite-input validation
- non-zero processing performs exactly one multiplication per sample
- mono and stereo use the same scalar; signed zero and subnormals are preserved
- finite samples outside `[-1,+1]` are accepted
- non-finite input/product returns `InvalidAudioSample` / `invalid_audio_sample`
- no clamp, fallback, clipping, normalization, limiter, dither, saturation,
  M/S, per-channel Gain, smoothing, ramp, state, latency, lookahead, pre-roll,
  post-roll, tail, or prepass
- signature includes type/version/schema/canonical `gainDb` and excludes the
  derived factor.

## Registry transition

| State | Descriptors | Production factories | Production processors |
|---|---:|---:|---:|
| Task 010 baseline | 11 | 0 | 0 |
| Task 011 delivery | 11 | 1 | 1 |

Only Gain is executable. All ten non-Gain descriptors remain exact and
unavailable.

## Render Preview evidence

`RenderRequest` validates and owns the immutable execution description while
borrowing a read-only canonical Source view: contained absolute Source range,
copied chain revision/order/state, owned Gain bindings, positive bounded block
size, and Preview purpose. `RenderResult` owns its canonical `AudioBuffer`,
Source-domain range, chain revision, and ordered Gain signatures.

The single private execution path supports empty identity, bypassed Gain,
active Gain, and multiple ordered Gain instances. Active unavailable non-Gain
modules fail rather than being skipped. Processing is synchronous, bounded by
the requested maximum block size, same-rate, and Source-domain preserving.
There is no SRC, cache, cancellation API, export, writer, UI, persistence,
Gold, Reference Match, or other DSP execution.

Tests checksum and bit-compare Source storage before and after successful and
failing renders. Source mutation was not observed. Result ownership and
lifetime are independent of request, Source, and chain objects.

## Oracle, chunks, and deterministic tests

Golden constants were generated independently with Python 3.12 and
`gmpy2 2.3.1` / MPFR 4.2.2 at 256-bit precision, then checked in as binary64
bit patterns. No machine-specific interpreter path is committed. Literal
factor and sample-product checks satisfy the packet's maximum 4-ULP predicate;
0 dB uses exact-bit identity.

Covered block sizes include 1, 7, 64, 257, and a value greater than or equal
to the requested range. The 64- and 257-frame outputs for 0 dB, +6 dB, and
-12 dB are bit-identical. Tests also cover signed zero, subnormal values,
non-finite input/product, mono/stereo, finite out-of-unity samples, descriptor
mismatch, missing/extra bindings, disjoint views, empty/bypassed/active/
multiple Gain, immutable snapshots, partial Source windows, unavailable active
modules, result lifetime, and WAV integration at 44.1/48 kHz.

## Automated acceptance

| Gate | Result |
|---|---|
| Fresh configure / inventory | PASS / 59 tests |
| Clean Debug build / complete CTest | PASS / 59 of 59 |
| Clean Release build / complete CTest | PASS / 59 of 59 |
| Focused Gain, Debug / Release | PASS / 4 of 4 each |
| Focused Render, Debug / Release | PASS / 2 of 2 each |
| Focused golden + WAV, Debug / Release | PASS / 2 of 2 each |
| Dependency contracts in complete suites | PASS / 27 each |
| Focused Render dependency rules | PASS / 5 of 5 |
| Public-header positive/negative checks | PASS |
| Tests-off Release | PASS; no test tree, listener target, or `Qt6::Test` |
| Staged deploy smoke | PASS / exit 0 |
| QML errors / RGSML warnings | 0 / 0 |
| ToolchainManifest build/install | byte-identical / accepted SHA-256 |

The accepted ToolchainManifest SHA-256 is
`49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11`.
No dependency was downloaded or installed.

## Listening Gate record

- Result: `PASS`
- Windows device: `Altoparlanti (High Definition Audio Device)`
- Physical connection: headphones
- Source fixture: versioned stereo 48 kHz PCM16 listening fixture with about
  -24 dBFS peak, satisfying the minimum 6 dB headroom requirement
- Gain sequence: 0 dB, +6 dB, -12 dB
- Chunk sizes: 64, 257 frames
- Automated 64/257 output identity: PASS before audition
- Human observation: all three levels were correct; no issue was reported
- Finding classification: none

No private Source path/content or user data was committed.

## Scope and protected-surface audit

All 35 Task 011 paths map to the packet allow-list. Protected production
surfaces (`core`, `audio`, `platform`, `app`, `ui`, project/analysis/reference/
dna/third-party packages), frozen DSP public contracts, accepted Task 001-010
evidence, RF-1, ToolchainManifest inputs, Phase/delta documents, and Task 012
artifacts are unchanged. Repository scans found no forbidden Qt/platform/
filesystem/network dependency in `dsp` or `render`, no new input format, and no
other DSP implementation.

## Telemetry and RF-2

Canonical telemetry is recorded in
`docs/implementation/telemetry/L1-M08.json`. RF-2 records the available actual
core/render integration evidence without reconstructing unavailable elapsed
times or changing the frozen 2/3/5 PDE and Phase-12 estimates.

## Defects, deviations, and residuals

Two defects were corrected before final verification: an initial non-zero Gain
guard performed a redundant multiplication and was replaced by exactly one
stored multiplication per sample; a test-only non-owning-view comparison
helper ambiguity was corrected. Final regressions: 0. Contract deviations: 0.

A bounded non-gate `--help` diagnostic experienced a host-side loader wait.
The required process-local Qt environment, CLI smoke, Release harness, deploy,
and all product gates passed; no process remained and no Avast detection or
quarantine occurred. The inherited Alt+F4 residual remains
`PO_ACCEPTED_DEFERRED / NON-BLOCKING` and was not reopened.

## Risk, rollback, and stop boundary

- `R12-01 — INHERITED UNCHANGED`
- `R12-02 — MITIGATED_FOR_WINDOWS_L1-M05 / RESIDUAL_MONITORING_OPEN`
- `RF-1 — MATERIALIZED; FROZEN ESTIMATES UNCHANGED`
- `RF-2 — MATERIALIZED_AT_L1-M08; FROZEN PHASE-12 ESTIMATES UNCHANGED`
- `INPUT FORMAT SCOPE — WAV_ONLY`
- `CODEX TASK 012 — NOT PREPARED / NOT STARTED`

The rollback unit is the complete Task 011 delivery commit. Removing it
restores the 11/0/0 registry and removes only Gain-v1, Render Preview, additive
tests/harness/dependency fixtures, and Task 011 evidence. It does not modify
the accepted baseline, Source/project/user data, RF-1, or historical untracked
file.
