# Transient Shaper C00 — post-L3 integration handoff

**Status:** isolated research editor, native C00 observation and Qt 6.8/Release smoke green.
**Product status:** NOT registered, NOT installed into DSP Chain, NOT a physical Windows FQ.
**Governance:** operational L3 AutoMatch is Single-Writer; no merge until its owner explicitly schedules integration.

## Authoritative implementation

- Frozen C00 DSP: `dsp/include/rgsml/dsp/transient_shaper_parameters.hpp`; isolated branch's **opt-in** real-gain observer in `dsp/src/internal/transient_shaper_c00_engine.{hpp,cpp}`. Default PCM processing must remain byte-identical without observer.
- Read-only/manual-gated research control adapter: `research/l3-transient-shaper-editor/transient_research_controls.hpp`.
- QML: `ui/qml/TransientShaperEditor.qml` and `TransientActivityScope.qml`. Two large copper-accented Attack/Sustain sliders and 120×32 numeric fields, -12..+12 dB with 0.1 dB increments, external Reset, and truthful five-second two-lane gain history.
- C00 observer, collector and projection: `transient_c00_activity_sink.hpp`, `transient_activity_collector.hpp`, `transient_activity_playback_projection.hpp`, `transient_activity_qt_bridge.hpp`. 20 ms absolute half-open buckets, genuine min/max/last gain for Attack/Sustain; no duplicated detector or QML clock.
- `Demo.qml` uses SYNTHETIC telemetry and must never be registered or shipped as an authentic Product Activity source.

## Nonnegotiable integration rules

1. `transientModule` adapter supplies `attackAmountDb`, `sustainAmountDb`, `setAttackAmountDb(v)`, `setSustainAmountDb(v)`, `resetToDefault()`, and a truthful `manualEditable` gate. During L3, research manual editing remains disabled.
2. Bind sliders, numeric wells and Reset to the **same** Product-owned accepted C00 parameters; never introduce a shadow DSP or preview engine. Numeric input accepts ordinary dot/comma decimals, rejects blank/hex/exponent/out-of-range drafts, quantizes 0.1 dB, commits on Enter/blur and cancels on Escape/lock.
3. The opt-in gain observer must be explicitly requested on a render, consume the actual C00 applied gain once a block commits, never change PCM and never block audio on UI/sidecar failure. Aggregate a bounded immutable full-track history (max 60,000 20 ms buckets, about 20 min).
4. Only a Product-authoritative accepted PCM `RealizationId` + chain revision, SAME `FrameDomainId` and sample rate, PROCESSED audition, not bypassed and stable NEW `PlaybackSnapshot` can expose Activity. Pause/seek/loop use the playback owner's cursor; mismatched or incomplete history must clear/hide the graph, never animate invented values.
5. Every accepted manual parameter edit MUST invalidate old PCM/sidecar eligibility until newly accepted render/provenance matches. Do not rely on a merely editable GUI to qualify old gain curves.
6. Register the editor with the current common host/window/docking contract after L3; use the same family headers and module lifecycle as existing DSP editors. Verify min-size 1184×688 and independently test Windows scaling and resizing.
7. Product project serialization, bypass, chain ordering, automatic DSP choice, UI icon/state and native Windows FQ are operational integrations, not part of this isolated research branch.

## Existing isolated checks

- `.github/workflows/research-transient-qt-smoke.yml`: true C00 PCM fixture in Qt 6.8 Release, 250 five-second buckets, genuine QML scene, manual lock, numeric/parser/Reset/slider signals, screenshot at normal and compact sizes.
- `.github/workflows/research-transient-observation.yml`: authentic C00 observer on four rates, mono/stereo, rollback/failure isolation and byte-identical PCM.
- `test_transient_activity_collector.cpp`, `test_transient_activity_playback_projection.cpp`: frame continuity, seek/loop, provenance, pause, rejected state.
- Validated research Qt CI: https://github.com/casaferla/RGS-MasterLab/actions/runs/45074293202
- Validated native observation CI: https://github.com/casaferla/RGS-MasterLab/actions/runs/45074293239

## Acceptance after Product integration

Create a Windows artifact and physically test Attack/Sustain edits, decimal keyboard input and wheel/drag release, Reset/lock, bypass and persistence, Play/Pause/Stop, seek/loop, SOURCE↔PROCESSED↔REFERENCE changes, rerender epochs, true applied-gain observation, callback failures, device/SRC rate mismatch, close/dock/resize and no extra render on rejected draft. These are **not** yet proved by the isolated tests.
