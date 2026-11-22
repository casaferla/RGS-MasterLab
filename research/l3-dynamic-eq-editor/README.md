# Dynamic EQ editor: isolated structural clone

Branch `research/l3-dynamic-eq-editor-isolated-20261122` from green L3 `9ca89e0b6fed6618bb8dd91e5071cba49a6d03f3`.
**No Product integration and no user-facing fine tuning until AutoMatch L3
is finished.** This separate research branch does not touch the L3 writer.

The first working copy directly clones the existing 656-line
`ParametricEqEditor.qml` and 498-line `ParametricEqGraph.qml`, retaining
the same graph geometry, interactive band handles, band colors, frequency
controls, Q/Shelf Slope, routing strip, validation layout, and the
SAME Live Spectrum ViewModel/FFT owner. The L1 Parametric EQ itself is
unchanged, and no new DSP, spectrum or numerical algorithm has been made.

Frozen C01 Dynamic EQ v1 permits precisely BELL, LOW_SHELF, HIGH_SHELF.
Notch and pass filters are removed from the visible selector. In the
former Overall toolbar location, Activity is reserved and disabled
until actual accepted band-gain telemetry is present. Combined static
Overall curve is removed because C01 evolves its coefficients per frame.
During L3 the graph handle mutations are disabled without an
explicit future `viewModel.manualEditable === true`.

This commit is a VISUAL / STRUCTURAL START, **not** a completed
Dynamic EQ editor. Follow-up on this branch: dedicated C01 typed
ViewModel; Threshold, Ratio, signed Range, RMS Time, Attack and Release
controls; genuine band dynamic-gain curve and envelope history
bound to the accepted audio realization; isolated Qt smoke/visual test.
No separate QML detector, duplicate C01 math or fictional live telemetry.
All common controls must stay in the exact L1 locations.

## Qt geometry parity test

The invisible `RESERVED_NOT_A_FILTER` layout slot replaces only the
Notch BUTTON AREA, preserving the original exact coordinates for Bell,
Low Shelf, High Shelf, Spectrum, Frequency, Base Gain, Q and graph.
It cannot be selected and never reaches a Dynamic EQ parameter setter.
The research-only dual-editor Qt harness asserts geometric equality
of these controls at 1184x688 and ensures no unsupported filter buttons.
The widget is still fully manual-locked until after L3; Activity
is disabled until authoritative DSP telemetry exists.

The dedicated CI produces a Qt screenshot in `dynamic-eq-structural-layout`;
this is a geometry smoke, NOT a running C01 DSP or accepted editor.

The High Pass and Low Pass filter slots are likewise kept as **empty,
non-selectable spatial placeholders**. Six equal geometric positions
therefore persist while only three supported filter types appear.
This is a strict no-drift layout decision, not an exposed DSP mode.

## Six frozen C01 dynamic controls (research)

A separate typed QObject reads accepted DynamicEqParameters with original
band identity and validates every edited value via DynamicEqBandParameters.
Range is signed, and Base Gain + Range endpoints are checked by the C01 factory.
The six numeric/slider pairs reuse StudioNumericSliderPair, drawn as an overlay
within the flexible graph so all shared Parametric EQ controls remain fixed.
Manual mutation is locked during L3. An isolated Qt test checks bounds,
accepted snapshots, manual lock, QML loading, and a screenshot artifact.
No DSP calculation or Product integration is introduced.

## Detached C01 inspector, no graph occlusion

The first overlay obscured the plotted curve and the frequency labels.
Now the six C01 controls live in a separate transient `Qt.Tool` window
(`DynamicEqDynamicsCompanion.qml`). The original DynamicEqGraph is never
occluded or resized, and all L1 shared controls retain their exact slots.
The test requires the companion to have the main playback/editor window as
transient parent and checks that its strip is not a child of the graph.
The isolated Qt screenshot job captures TWO real images: the unobstructed
main EQ editor and the actual C01 controls window. The companion starts
closed unless the research harness explicitly requests it; there is no
Product integration or manual fine tuning during AutoMatch L3.

## Dedicated Dynamics button (post-L3 research)

A right-aligned, fixed-width `Dynamics / Hide Dynamics` button sits in the
unused trailing part of the existing band strip, leaving six band selectors,
Spectrum, Filter, Routing and Frequency/Gain/Q at their original positions.
It opens/closes the separate C01 controls window and cannot become a Product
manual editor before the end of L3. Qt tests toggle the window twice, confirm
it is a child of the real editor host window, and verify the manual lock.

## Authentic C01 Activity tap — isolated offline research

The research branch adds an optional **success-only** observer directly to
its own copy of `C01FrameEngine::process_block`. Every enabled band exposes
true C01 detector linked power, tagged detector level (undefined for P=0),
unsigned target, signed targetDynamicGain, smoothed applied dynamic gain,
and effective per-frame gain, with persistent UUID and routed execution order.
It uses caller-preallocated per-frame scratch, not a QML duplicate detector.
The old four-argument engine call is unchanged; Product and AutoMatch do not
request observation. The sink receives a callback only after the whole PCM
block and DSP states commit, never for a rejected/rolled-back block.

The dedicated Linux native test compares bit-identical PCM, checkpoint state,
band identity and frame continuity across 44.1/48/96/192 kHz, mono/stereo,
and error/overflow paths. This is **not** yet a QML Activity animation,
an accepted playback-sidecar identity, or a post-L3 editor integration.

## Per-band 20ms C01 full-track history (research)

The callback now feeds a preallocated `C01ActivityHistoryCollector` with
50 true gain buckets per second, per-band UUID and route, exact minimum/
maximum/last smoothed dynamic gain, last effective gain, last signed target,
and detector power max/last with tagged detector level. Maximum 60,000
buckets (20 minutes) and bounded full-track seek support; no allocations
inside the callback, no FFT/QML detector, no output changes. The collector
fails closed on frame discontinuities, bad values, changed band identities
or capacity exhaustion; the PCM block remains committed. Native tests cover
all four supported rates, completed-bucket playhead exclusion, seek/loop
windows and full capacity. UI and actual audible-realization binding still
remain to be implemented post-L3.

The research-only Qt geometry workflow now triggers only for the QML/Qt
source files it actually tests, not every native C01 history experiment.

## Accepted audible PCM provenance (research-only)

The typed `C01ActivityRenderProvenance` and
`project_c01_activity()` insist on the authoritative Product
`PlaybackSnapshot` plus accepted PCM RealizationId, chain revision,
frame domain, sample rate, selected PROCESSED audition, and bypass state.
No chart is available during OLD/TRANSITION handoff, stop, stale revision,
wrong sample rate or frame domain, or before immutable history finishes.
PAUSED and seek/loop position reuse the same full-track actual DSP sidecar;
no QML timer or invented SRC conversion can authorize Activity.
This is a tested binding CONTRACT, not yet an installed Product adapter.

## Qt Activity surface — proven C01 output, simulated playback clock

The research-only Activity companion is a separate Qt tool window, so the
original Parametric EQ graph/axes and Spectrum are never obscured or moved.
Its five-second, per-band colored traces draw **only** true C01 smoothed
applied dynamic gain, min/max per completed 20ms bucket, from the approved
sidecar projection. The Qt bridge validates identity, monotonic sample
positions, finite gain bounds and fully played buckets; it supplies no
independent timer and cannot recompute the C01 DSP. Activity is disabled
and the window hides whenever the accepted audible realization becomes
stale, bypassed or unavailable.

The isolated Qt smoke processes a deterministic six-second PCM fixture
through the actual C01 engine and collects 300 true buckets. A simulated
PlaybackSnapshot at 5s selects exactly 250 actual gain buckets; the harness
is conspicuously labeled test playback, not a physical listening test.
It captures the unobstructed main editor and the real C01 Activity window.
No Product registration, manual fine tuning or merge during L3.

## Shared C01 state — common EQ read-only presenter

`DynamicEqResearchCommonViewModel` reads the exact accepted band vector
and selected index from the existing six-parameter C01 model, preserving
band UUID, filter, routing, frequency, base gain, Q and shelf slope. Its
257-point curve evaluates the **frozen C01 coefficients at static BASE
GAIN** — not an Overall and not a fabricated applied dynamic response.
It never owns duplicate band parameters or modifies Product during L3.
A separate Qt Core test confirms multi-band selection and true C01 curves.
The later QML binding and post-L3 typed manual transactions remain separate.

## Qt binding to one typed C01 store (no Product controls)

The main inherited EQ editor receives `DynamicEqResearchCommonViewModel`,
which observes the SAME `DynamicEqResearchDynamicsModel` object as the
separate six-control Dynamics window. Band selection is readable during L3;
all filter, routing, enable, reset, and frequency/gain/Q/shelf text/slider
mutations are explicitly disabled. The graph labels its actual C01
**BASE GAIN RESPONSE** and never labels it dynamic or Overall. The Qt smoke
now checks both QML context bindings and retains the previous geometry tests.

## Research-only typed common-control transactions

The common EQ presenter delegates Frequency, Base Gain, Q, Shelf Slope,
Bell/Shelf selection, routing, enabled toggle, graph drag/release and Reset
through the SAME C01 typed band owner that handles Range and detector timing.
The frozen `DynamicEqBandParameters` and audio-format validation gate every
commit, including signed Base Gain+Range endpoints and no Notch/Pass modes.
The six-controls window and selected-band base-curve update together.
Manual changes are **only** enabled by explicit research-test opt-in; L3
Product still contains neither editor and receives no changes.

## C01 band operations, typed Undo/Redo and stable draft text

Research-only manual authoring now supports Add Band (only with an explicitly
injected unique UUID generator), Remove Band, Reset Flat and up to 50 undo
snapshots. Each successful C01 typed edit or band operation records a bounded
snapshot; read-only selection and failed edits do not. Common numeric fields
reflect the actual pending text while typing and revert on Escape/cancel.
All operations fail closed when `manualEditable=false`, and the Production
AutoMatch branch is not involved. The isolated Qt Core test exercises these
branches before any Product editor acceptance.

## Two-mode real Qt editor screenshots

The isolated Qt test now captures both required states: L3 read-only and
explicitly unlocked **research-only** manual fine tuning. It verifies that
the same C01 Base Gain edit is accepted by the typed shared-band owner and
reflected in the Parametric-derived curve, while both windows' editability
switches together and locks again on completion. The artifact includes four
PNG files. This is not Product integration or physical audio qualification.

## Idempotent manual gate and filter selection (research)

Repeated research-only `manualEditable=true` notifications now preserve an
in-progress numeric draft, while an actual relock cancels all pending text and
graph drag state. Re-selecting BELL / LOW_SHELF / HIGH_SHELF no longer
reconstructs the shape: the accepted Q or Shelf Slope stays unchanged and no
phantom Undo snapshot is created. The Qt Core smoke checks filter response,
Undo/Redo and permission transitions. No Product or L3 integration.
