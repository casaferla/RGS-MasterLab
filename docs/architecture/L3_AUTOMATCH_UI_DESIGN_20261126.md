# L3 AutoMatch Product UI — approved control hierarchy and physical acceptance

Status: implementation pass on Writer, not a released FQ. Keep numerical solver and previously qualified six-rate DSP evidence untouched.

## Approved control choice (2026-10-05)

The five AutoMatch amounts are **rotary knobs**, not ordinary DSP faders. Four independent domains: Tonal, Dynamics, Stereo, Loudness. Total Match Amount is the fifth, more prominent global knob. All are 0–100% and support vertical drag, keyboard/wheel and an exact numeric-field fallback. The global knob scales each stored domain amount proportionally (effective domain amount = stored domain amount × Total Match Amount / 100), never rewrites the stored four values. The four ordinary DSP editor controls remain slider-first.

The ring has a continuous 270-degree lower-left to upper to lower-right path, with a 90-degree lower gap. At zero the progress arc is unlit; both arc brightness and outward phosphor glow rise monotonically to 100. Pointer angle must track the actual value. No fake audio or processing telemetry.

## Lower-left Adaptive Context

Use the actual SOURCE/REFERENCE and native AutoMatch owner; never invent success or candidate percentages. Retain the Reference identity card and the Prepare/Restoration tab. Total knob visually outranks the four equal-sized domain knobs. The five controls must update the exact native numeric fields and remain locked during SEARCHING or when a candidate is pending review. The search button opens the existing native-backed dialog; it does not authorize adoption. The small status and a real monotonic elapsed-search clock remain visible in the context after closing the dialog. At 1184x688, all controls and labels must fit without overlap or clipping.

The native review metrics remain the independent solver-phase durations (original/fixed/seed/search), trial counts, and first admissible timing where available. The new wall-clock is measured on the Product owner's Qt thread from accepted start until search reaches a terminal/review phase. M13 verification is **not** included in that search clock. Distinguish first admissible candidate from complete search and independently verified adoption.

## Shared DSP header

Keep the approved fixed 2x2 cluster left of the title: Dock/Undock, Reset, Open, Save. All four use the same StudioIcon Qt Canvas painter grammar and the actual selected module's `activeFamilyAccent` (including M17 bypass/active semantic accent); do not render every module cyan. Unavailable actions remain unavailable, without dummy controls. Use family color on glyphs, not a new full-panel background; retain visible focus and disabled states. The DSP Chain module glyphs remain the approved optical reference.

## AutoMatch dialog — separate production redesign gate

The existing narrow Qt Basic dialog is a **test surface**, not final Product design. Redesign it as a coherent RGS MasterLab dark panel matching the WAV Export family, with legible hierarchy, numeric values, an independent AutoMatch/M16 TP safety ceiling, real search/cancel/review/adopt/undo/redo controls and explicit candidate/verification diagnostics. Do not confuse the independent AutoMatch TP safety limit with the terminal M17 Export TP Limiter or the optional P2 Streaming TP Limiter. The dialog must not grant its own DSP authority or fake convergence.

## Qualification

1. Qt Product cold startup must pass with **zero QML warnings**; run on Windows artifact CI.
2. Verify all five knob pointers, numerical values and arc/glow at 0, 25, 50, 75, 100, keyboard/wheel/vertical drag; numeric fallback and knob must agree.
3. Verify no mutation during search or pending candidate, no loss of domain values on global knob change, and correct native start arguments.
4. Inspect at 1184x688, 1440x900, 100%/150% DPI, docked/floating, with all four family accent variants and M17 Active/Bypass.
5. Complete real musical full-track benchmark and physical Windows playback separately. No change to numerical six-rate acceptance or FQ status from a visual pass.
