# Dynamic EQ C01 — post-L3 integration handoff

**Status:** isolated research editor, standalone Qt 6.8/Release smoke qualified.
**Product status:** NOT registered, NOT installed into DSP Chain, NOT a physical Windows FQ.
**Governance:** operational L3 AutoMatch remains Single-Writer; no merge until its owner explicitly schedules integration.

## Authoritative implementation

- Native frozen DSP: `dsp/include/rgsml/dsp/dynamic_eq_parameters.hpp`, C01 engine/coefficients in `dsp/src/internal`. Import the isolated, opt-in C01 observation hook only after preserving PCM equivalence and existing C01 contract.
- Single accepted parameter owner: `research/l3-dynamic-eq-editor/dynamic_eq_research_dynamics_model.hpp`.
- Parametric-derived common presenter: `dynamic_eq_research_common_view_model.hpp`; both views share ONE accepted typed band vector and selected band index, including stable UUIDs.
- QML: `ui/qml/DynamicEqEditor.qml`, `DynamicEqGraph.qml`, `DynamicEqDynamicsCompanion.qml`, `DynamicEqDynamicsStrip.qml`, `DynamicEqActivityCompanion.qml`, `DynamicEqActivityHistoryGraph.qml`.
- C01 Activity: `dynamic_eq_activity_history.hpp`, `dynamic_eq_activity_playback_projection.hpp`, `dynamic_eq_activity_qt_bridge.hpp`. Completed 20 ms gain buckets only; no QML detector, synthetic timer or static Overall curve.

## Nonnegotiable integration rules

1. Keep inherited Parametric EQ graphic/controls and Spectrum in their qualified coordinates. C01 has only Bell, Low Shelf and High Shelf; no Notch/pass filters and no combined static Overall.
2. The six C01 dynamic parameters are signed Range, Threshold, Ratio, RMS Time, Attack and Release. The WAV Export-style Dynamics panel is a separate, nonmodal `Qt.Tool`; never overlay the EQ frequency axis or shrink its graph. Activity is also a separate optional real-telemetry view.
3. New Product adapter must resolve one accepted `DynamicEqParameters` instance and Product-owned unique band UUID generator, edit permission, Undo/Redo and project persistence. Never use the test UUID generator in Product.
4. Load accepted parameter snapshots with Product-owned lifecycle/epoch. A manual edit MUST invalidate the old render's eligibility until its revised PCM/sidecar is accepted. Do not infer readiness from GUI parameters alone.
5. Activity requires **real** `PlaybackSnapshot` (stable NEW audible realization), accepted PCM `RealizationId`, accepted chain revision, matching source frame domain and sample rate, PROCESSED audition, no bypass, and complete immutable C01 history. On wrong identity, stop, transition, stale revision, seek outside history or missing data: hide stale gain and clear the bridge.
6. Wire module selection, docking/floating and close semantics through the shared host conventions as they exist **when integration starts**; do not copy obsolete L3 docking internals. Check companion positioning at 1184×688 and on Windows Full HD/dual-screen, DPI 100/150%.
7. Original L1 Parametric EQ, Export TP Limiter, SRC/render path and AutoMatch writer remain untouched by this handoff. Product CI, native Windows FQ, actual playback and persistence must be qualified separately.

## Existing isolated checks

- `.github/workflows/research-dynamic-eq-qt.yml`: Qt 6.8 Release smoke, structural positions, unsupported-filter exclusion, real QML two-window controls, C01 Activity from true six-second DSP fixture, full screenshot artifacts.
- `test_dynamic_eq_common.cpp`: typed band selection, decimal-comma input, filter Q/Shelf, Reset Flat idempotence, Undo/Redo and Add/Remove.
- `test_dynamic_eq_dynamics.cpp`: six-field owner, actual QML controls, read-only/manual gate, selected-band synchronization, WAV-family panel, close/reopen and screen-position model.
- `test_dynamic_eq_activity_qml.cpp`, `test_dynamic_eq_activity_playback_projection.cpp`: native gain observations, genuine sidecar and rejected/stale frame behavior.
- Validated research baseline CI: https://github.com/casaferla/RGS-MasterLab/actions/runs/45074220359

## Acceptance after Product integration

Produce a new Windows artifact and physically verify source/PROCESSED/REFERENCE switching, all six fields for each band, filter/routing selection, Undo/Redo/Reset, window min size/docking, close/reopen, true Spectrum, actual C01 Activity during Play/Pause/Stop/seek/loop/bypass, accepted sidecar invalidation on edit/re-render, save/reopen, channel/sample-rate changes and no regressions to Parametric EQ. None of these Product-level results is claimed by the research branch.
