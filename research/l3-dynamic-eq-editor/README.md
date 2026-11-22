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
