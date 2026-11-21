# L3 AutoMatch — first bounded offline search controller

Date: 2026-11-21
L2 minimum merged to `main` via PR #101 at `f77fb4a7`, Focused #1 Windows/Linux green.

## Scope of this first L3 lot

A deterministic **single-flight offline evaluation controller** consumes genuine, live, Product-rechecked M21 evidence through the L2 numerical bridge. The future Product adapter will apply the proposed exact typed DSP settings, run the *existing* whole-track renderer, await complete true measurements, and submit a fresh `M21ProductPublicationInbox::view_current()`. The controller does **not** itself build PCM, process callbacks, change UI or invent frequency response.

Its explicit `Objective` list includes a metric, caller-chosen unit tolerance and caller-chosen positive weight. **No default weights or perceptual assumptions are imposed** at this stage; different units are normalized only by explicitly approved tolerances. Missing or low-confidence measurements are not treated as zeros. The baseline score is preserved; later trials are compared deterministically against the same selected REFERENCE.

Each proposal holds an opaque fingerprint that the future Product adapter must bind to **complete validated DSP parameter bindings**, not merely a patch to the GUI. The controller issues one ticket at a time, accepts new current M21 measurements only after their PROCESSED producer generation advances, rejects changed REFERENCE identity/generation and mismatched source rates, and can be cancelled without installing any DSP setting. It returns the best parameter fingerprint and numerical score, not an executable preset.

Native tests use the real M21 PrivatePublicationGuard to exercise baseline, two successive measured candidates, independent 44.1/48k reference rates, no implicit low-confidence TP objective, and cancellation on changed reference/invalid tickets. The exact new SHA receives bounded Windows/Linux Focused CI; no FQ/physical test for this development-only C++ core.

## Next L3 lots (not yet implemented)

1. Actual Product adapter: on Qt owner capture active SOURCE/REFERENCE, bind the full validated 5-module DSP chain settings and submit them through the M18 protected mutation epoch.
2. Whole-song RenderPreview and existing M19–M21 numerical worker completion/retirement. Tie each `Ticket` to a verified output realization and full immutable DSP fingerprint, with no stale candidate publication.
3. Operator-approved explicit metric objectives and admissible DSP parameter search bounds, coordinated with P2/M17 roles and full output-rate selection.
4. Safe save/restore and revert/accept behavior, progress, cache keys and performance. No auto-apply or GUI until the end-to-end Product path proves cancellation, full chain correctness and bounded CPU/RAM.

The previously failed **FQ #102** remains separate from this PO-authorized L3 development path.
