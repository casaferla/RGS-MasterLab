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
