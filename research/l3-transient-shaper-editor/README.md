# Isolated Transient Shaper editor — research only

Source branch: `research/l3-transient-shaper-editor-isolated-20261122`
Frozen/green parent: `9ca89e0b6fed6618bb8dd91e5071cba49a6d03f3` (Win/Linux green).
Product owner decision: **finish L3 AutoMatch first**. This branch neither
changes the active L3 writer nor enables manual fine-tuning during L3.

## Changes
- `ui/qml/TransientShaperEditor.qml`: physically matches P2's two large
  readouts/sliders and 120 x 32 Consolas numeric wells; Attack and Sustain
  exactly -12..+12 dB, 0.1 dB increments, wheel, drag/release, reset.
- `ui/qml/TransientActivityScope.qml`: five-second scrolling applied-gain
  visualization with genuine-sample min/max per bucket and last applied gain
  per bucket, two distinct lanes.
- `research/l3-transient-shaper-editor/Demo.qml`: **synthetic-only** UX
  sandbox, conspicuously labeled; never publish fake values as DSP output.

- `research/l3-transient-shaper-editor/transient_activity_collector.hpp`:
  opt-in, zero-allocation-after-construction, bounded full-track 20 ms
  min/max/last **observation** of the existing C00's two applied gains.
  The producer must pass the actual C00 gain states, not recompute them.
- `research/l3-transient-shaper-editor/test_transient_activity_collector.cpp`:
  standalone native test for all supported rates, frame continuity,
  peaks, finite/bounded values, capacity, seeks and future-data exclusion.

Collector validation (local C++20, independent of the Product target):
```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic \
 research/l3-transient-shaper-editor/test_transient_activity_collector.cpp \
 -o /tmp/transient_collector_test && /tmp/transient_collector_test
```
Result on the research host: `transient activity collector: all tests passed`.
This is a collector semantic test, **not** proof of actual C00 telemetry
integration or a Qt GUI test.

No CMake, host, module registry, project serialization, Qt application
composition, or production CI workflow files are modified. No pull request or merge.
The new editor is intentionally not wired into the production app yet.

## Independent Qt/QML materialization proof

`transient_activity_qt_bridge.hpp` translates a fully validated
`PlaybackActivityProjection` into the editor's existing QML properties,
only within safe JavaScript integer bounds. Invalid/stale render evidence
clears the chart without asking the DSP to recompute its envelopes.
`transient_research_controls.hpp` starts locked, validates Attack/Sustain
with the exact typed parameter factory and loads accepted C00 parameter
snapshots read-only, separate from manual edits.

`RealC00QtHarness.qml` is a standalone actual Qt scene: a generated PCM
fixture is processed by the actual C00 engine and produces authentic
observed gain samples, while the playback snapshot is deliberately
SIMULATED and clearly labeled. `test_transient_qml_bridge.cpp` checks
real Qt/QML editor creation, locks, actual-gain values, stale revision,
stale realization and pause. The isolated Qt 6.8 CI runs an offscreen
QtQuick load and uploads its rendered research screenshot as a run artifact.

The Qt screenshot fixture spans six seconds of ACTUAL C00 processing
of deterministic 48 kHz, beat-rich generated PCM, with a five-second
display window and 250 genuine 20-ms Attack/Sustain buckets. Only the
playback position is simulated for this automated test; the animated
applied-gain traces are not generated from an invented UI curve.

These green gates do NOT claim an audible physical listening test or a
Product-accepted visual editor. Do not merge with the active L3 writer.

## Authentic playback owner safety projection (research)

The new `transient_activity_playback_projection.hpp` checks the real
`PlaybackSnapshot`'s `AudibleRealizationState` (stable `NEW` phase),
its `RealizationId`, the accepted chain revision, exact frame domain,
sample rate, render completion, actually auditioned PROCESSED, and
bypass disposition. Only then does it expose the completed 5-second
sampled gain history. PAUSED freezes the cursor, seeking backwards
reselects history from the immutable sidecar. No SOURCE↔OUTPUT rate
conversion is guessed; unavailable data stays unavailable.

The caller must obtain the sidecar RealizationId and provenance from
the real accepted render/publisher; this research projection does not
invent a value or attach itself to Product/AutoMatch. Tests verify
wrong realization, old revision, handoff transition, stop/seek, wrong
timebase/sample rate, bypass, and pause.

## Future production adapter / observation contract

The editor accepts `transientModule` with:
- `attackAmountDb`, `sustainAmountDb` (read-only QML-visible numerics);
- `setAttackAmountDb(v)`, `setSustainAmountDb(v)`, `resetToDefault()`;
- `manualEditable === true` (gated OFF during L3).

The future render-sidecar supplies `activityHistory`, each bucket:
`{frameBegin,frameEndExclusive,attackMinDb,attackMaxDb,attackLastDb,
sustainMinDb,sustainMaxDb,sustainLastDb}` in dB. The same accepted
audible `RealizationId` must be visible via two identity properties.
`audibleFrame` comes ONLY from the authentic playback snapshot; changing
it scrolls the five-second view, and pause freezes it. Only fully consumed telemetry buckets are displayed;
no unseen portion of the current bucket may leak into the playback scope.
When identities
differ, bypass is active, or the sidecar is missing, the graph says
TELEMETRY UNAVAILABLE. No independent animation clock in production.

Full-track history uses 20 ms buckets, default maximum 60,000 buckets
(about 20 minutes at 50 buckets/s; approximately 3.84 MiB storage).
It is created only on explicit telemetry opt-in, and its immutable
5-second window can be requested after a seek. Raw audio/DSP never waits
for QML. Mapping the snake_case native buckets to the camelCase QML
carrier and authentic RealizationId is the later integration seam.

## Research-only authentic C00 observation hook

This isolated branch also modifies ONLY its own copy of
`dsp/src/internal/transient_shaper_c00_engine.{hpp,cpp}`. The default
4-argument create/process calls keep their original sonic execution path.
`create(..., true)` opts into a preallocated one-block observation buffer.
`process_block(..., &sink, firstFrame)` delivers exact per-sample
`g_attack_db` and `g_sustain_db` only after the block commits successfully.
An observation error invalidates telemetry, never PCM or C00 state.
No sink is retained between blocks. The optional hook has no Product caller,
and no AutoMatch code is changed on this branch.

An additional no-Qt gate compiles and executes the ACTUAL C00 source:
`test_transient_c00_observation.cpp` confirms byte-identical observed
versus unobserved PCM, four rates, mono/stereo, chunk partitioning,
failed-block rollback, observer failure isolation, and absolute frame overflow.
A dedicated research-only GitHub Actions workflow runs this native gate.
This is NOT a release qualification or proof of UI/audio playback integration.

The C00 engine already computes `g_attack_db` and `g_sustain_db` per
sample; a future bounded telemetry tap must sample/aggregate those exact
states from the executing processor (not recompute a second DSP) and bind
to actual PCM and source frame domain. Efficient sidecar and source-clock
binding, QML/Qt smoke tests, audio listening and physical QA remain later
work. History and current last samples are observations, not new controls.

The demo uses a Timer and synthetic values **only in Demo.qml**. It is not
audio playback, and the fake demo identity must never enter production.

## How to view
If Qt 6 QtQuick tooling is available locally, launch the standalone demo
with `qmlscene research/l3-transient-shaper-editor/Demo.qml` (or your
Qt-provided `qml` launcher). A local Qt runtime is needed; this branch
does not add a GitHub Actions job and is not a qualified UI artifact.

## Hard boundaries
Single-Writer Epoch: do not touch the operational staging, main, active
AutoMatch code, or L3 C01 CI branch. Do not merge during L3. No fabricated
telemetry, additional transient controls, spectrum-only detectors, or
edit-side mutation of protected AutoMatch ownership. The shared frozen
DSP truthful-visualization contract remains authoritative.

## Release-mode Qt smoke safety

The QML/C00 research test uses an always-evaluated `CHECK` gate rather than
`assert()`, because `NDEBUG` must never remove the actual C00 PCM render or
its real-gain telemetry checks. The isolated Qt workflow builds in Release
mode and checks 250 complete 20-ms C00 buckets, the five-second playback
projection, editable/read-only lock, stale provenance and a Qt screenshot.
No production module or L3 AutoMatch branch is changed.

## Research editor lock/recovery and malformed telemetry gate

A revoked research-only edit gate now discards any uncommitted Attack/Sustain
numeric text, whether or not the input retains focus; accepted parameters
remain unchanged and the activity graph is independent of manual edit rights.
The Qt bridge refuses nonfinite, out-of-range or unordered C00 min/last/max
triplets before advertising history as valid. A rejected/stale frame clears
history, identity and playback position, and a later genuine matched C00
projection restores the scope without inventing gain or timing. The Release
Qt smoke exercises those transitions. Production L3 remains untouched.
