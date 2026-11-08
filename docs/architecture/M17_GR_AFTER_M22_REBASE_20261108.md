# M17 full-track GR HISTORY integrated above M18–M22 — 8 November 2026

**State: isolated, focused-CI pending, NO MAIN MERGE, no physical PO sign-off.**

## Exact frozen Product and UX inputs

- Last dual-OS successful M18–M22 focused bridge CI #7: [run 42492719918](https://github.com/casaferla/RGS-MasterLab/actions/runs/42492719918), commit `3aca7bdf4d96269d86e6690617642b876dcb3cbc`. This is a **technical preintegration** with unresolved Phase-6 canonical numeric UI authorization, generalized M22 scheduling, physical A01–A15 and final Full Qualification; it is not a false complete release claim.
- Isolated PO M17 full-master UX PR #85: `365c91924dc04a73a6eaa0981b880db5383e0731`, [original focused CI #1042 dual-platform success](https://github.com/casaferla/RGS-MasterLab/actions/runs/42124516533). The entire PR #85 is based on older `main`, so merging or bulk replacing its history would regress later M18–M22 and REFERENCE.
- New single-writer staging: `staging/m17-gr-after-m22-no-ci-20261108`. Separate candidate: `staging/m17-gr-after-m22-ci-20261108`. This is **not a main merge** and does not alter the original PR #85 or PR #96.

## Audited graft

Five Qt/QML files from PR #85 plus its original PO UX contract are imported as **exact immutable Git blobs**, because the current base carried the same pre-M17 versions of those five files. `ui/qml/Main.qml` and `tests/ui_smoke/test_source_metadata_panel.cpp` could **not** be replaced with the older PR #85 versions: each contains later user-facing REFERENCE and M18–M22 integration tests. The PR #85 unified changes were therefore applied as **six verified, uniquely matching hunks** (Main: 2; test: 4). The existing M18–M22 and REFERENCE lines remain intact.

The M17 User Experience is **one full-Master sampled OFFLINE gain-reduction curve only**, without live GR graph or toggle. It preserves legacy Windows consumed-output signal witnesses and M16 live TP meter internally but does not mislabel historical M17 applied GR as live consumed audio. Cursor position is a read-only fraction from the main full-track waveform, never a second timer; SOURCE/Master scope is explicit. Copper saturation grows to the synchronized yellow playhead; no synthetic gr values, stale frame gaps, invalid history or detached graph semantics. BYPASS/no current Master is empty. NON-CONFORMING remains explicit.

## Automated gate

- A new isolated read-only guard checks exact Git blob identities of all eight transferred files (including the two rebased files), insists on the immutable M18–M22 dual-green base ancestry and allows **only** eight PO GR files plus this document, its guard and dedicated workflow.
- Windows CI builds native full `RGSMasterLab`, actual `Main.qml` Qt module and the real `rgsml_source_metadata_panel_smoke`, runs direct `truePeakLimiterLayoutCandidate` QtTest with synthetic 25%/75% GR cursor PNGs and real REFERENCE labels; includes M18 ProductSession and other retained regressions.
- Linux CI retains portable typed Qt and analysis smoke from the previous dual-green bridge, with no Windows-only exporter binary.
- Focused success permits the **isolated GR GUI preintegration**; it does not certify physical Windows audio or DSP, nor act as final FQ. PO explicit approval is still needed to merge to `main`.

M18–M22 unresolved release gates (canonical Phase-6 FeatureDefinitionRegistry / product numeric UI, generalized M22 scheduler, physical Windows PO matrix, final combined FQ) are not silently cleared by this UX overlay. The separate PR #85 may remain Draft as historical evidence.

## Follow-up after CI #1 green: complete M17 editor screenshots without physical PC

The PO correctly pointed out that software development must NOT stop while awaiting Windows physical acceptance. CI #1 was completed **SUCCESS on both Windows/Linux**, but its M17 screenshot artifact showed **only** two narrow 54px GR stripes at 25%/75%, insufficient to review the editor layout. Retain original two real Qt raster captures and extend the SAME true `truePeakLimiterLayoutCandidate` Qt fixture with two additional **whole-M17-editor** `QQuickItem::grabToImage()` captures at positions 25% and 75%, at its actual 710px-class layout size. These images are authentic Qt software-rendered test output using synthetic sample GR evidence, NOT photorealistic mockups or a physical Product/DEVICE screenshot.

Four required PNGs, same immutable fixture with on/off/bypass, cursor, owner and geometry checks:
- `m17_gr_history_SYNTHETIC_cursor25.png`
- `m17_gr_history_SYNTHETIC_cursor75.png`
- `m17_gr_history_EDITOR_SYNTHETIC_cursor25.png`
- `m17_gr_history_EDITOR_SYNTHETIC_cursor75.png`

The candidate CI must refuse missing, empty or invariant full-editor rasters; the provenance guard pins the revised Qt regression's exact blob; original DSP/Product/Phase-6 and project formats remain unchanged. Use one Windows+Linux focused CI after a scoped batch, no merge to main. Then continue open nonphysical development, particularly canonical Phase-6 release semantics, M21 Product numeric presentation eligibility and the generalized M22 planner. Physical A01–A15 and human waveform/audio/dual-monitor judgment are deferred to PO's first available PC.
