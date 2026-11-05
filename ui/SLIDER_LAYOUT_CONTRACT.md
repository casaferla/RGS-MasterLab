# RGS MasterLab — Canonical DSP numeric-field / slider layout

Status: Product Owner accepted working rule, 2026-10-10.
Token: `RGS_DSP_NUMERIC_SLIDER_ALIGNMENT = MANDATORY`.

When a DSP parameter has both a numeric input box and a corresponding slider, use `StudioNumericSliderPair.qml` for **new controls**. It wraps the existing `StudioNumericField.qml` and `StudioParameterSlider.qml`; it does **not** replace or restyle the underlying components.

**Exact alignment invariant** (at every responsive size):

- Slider wrapper starts at the left edge of the numeric **value box**.
- Slider wrapper ends at the right edge of that same **value box**.
- Slider wrapper width = numericField.fieldWidth (not numericField.implicitWidth, which includes optional units).
- No expansion into the suffix `%`, `Hz`, `dB`, etc.; no expansion to fill the column.
- Labels, units, white knobs/semantic halos, fills, and draft/commit interactions are unchanged.

The canonical pair enforces this using `Layout.fillWidth: false`, `Qt.AlignLeft` and equal min/preferred/max widths bound directly to `numericField.fieldWidth`. Author numeric fields and sliders through the pair instead of separately specifying these constraints.

**Legacy protection**: Input Gain, Parametric EQ, and Compressor already have accepted numeric/slider positioning. Do not migrate or refactor them merely to use the pair, and do not change shared component behavior. For a non-paired slider (no associated numeric box), preserve its established module-specific layout instead of applying this rule blindly.

**Evidence**: Every new paired slider must add/extend a native Qt geometry smoke assertion at minimum-window and tall-window sizes checking both endpoints mapped to the editor in the same coordinate system; include PNG evidence where the milestone calls for visual review. The M15 Stereo/M-S three-slider test in `tests/ui_smoke/test_source_metadata_panel.cpp` is the reference implementation.

**Coding continuity**: All successor chats, Jules/Codex instructions, and future DSP editor work must recover this rule. Comments should explain WHY the numeric box width excludes the suffix, without adding unrelated refactors or comment-only tests.
