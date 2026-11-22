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

No CMake, DSP, host, module registry, project serialization, Qt application
composition, or CI workflow files are modified. No pull request or merge.
The new editor is intentionally not wired into the production app yet.

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
it scrolls the five-second view, and pause freezes it. When identities
differ, bypass is active, or the sidecar is missing, the graph says
TELEMETRY UNAVAILABLE. No independent animation clock in production.

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
