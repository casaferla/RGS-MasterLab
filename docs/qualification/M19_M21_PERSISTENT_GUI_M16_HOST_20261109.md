# RGS MasterLab — M19–M21 persistent analysis pages, M16 host contract

Date: 2026-11-09. Nonrelease implementation. Product Owner instruction:
continue after the genuinely double-green focused CI #32, remove temporary
createObject/destroy navigation, prefer permanently instantiated inactive
panels exactly as already qualified for M16 and Master Audition.

## Frozen reference and proven prerequisite

- Qualified GUI baseline FQ #99: `5e4871d5849e4121b0612c2142d1e051d6c9a3d2`.
- Most recent focused CI #32 GREEN Windows+Linux:
  `4a0b0458d14771d40ccecf8f894eea55d59d10d5`.
- #31 controlled native Windows diagnostics showed both immutable FQ99 AND
  M19–M21 candidate pass 16/16 Qt smoke with PowerShell->Qt or
  CMake->PowerShell->Qt but fail the same EQ containment as direct CMake
  children. #32 corrected only Windows CTest process launch; the unchanged
  strict EQ assertion then PASSed. The previous hypothesis that a hidden
  analysis page inevitably breaks legacy layout was **not demonstrated**.

## Product integration selected for this macro-lot

Only additive QML changes in `ui/qml/DspEditorHost.qml`, with NO changes to
any original FQ99 control or existing ColumnLayout child:
- Insert THREE direct `AnalysisStatusEditor` sibling pages, ordered between
  the original `TruePeakLimiterEditor` (M17) and `MonitorEditor` (Master
  Audition). Product selectors already contain M17, M19, M20, M21, Monitor.
- Each page is permanently constructed for the lifetime of its OWN host:
  `m19LevelProductAnalysisEditor`, `m20TonalStereoProductAnalysisEditor`,
  `m21MetricsDeltaProductAnalysisEditor`; fixed panelIndex 0, 1, 2.
- A page is visible iff its corresponding `editorContentKey` is selected.
  `enabled: visible` prevents hidden pages from receiving interaction.
- Exactly one qualified ColumnLayout is geometry owner. Each new page uses
  `Layout.fillWidth/fillHeight`, `minimumWidth/minimumHeight: 0` and
  `preferredHeight: 0` — no root overlay anchors, hardcoded header offsets,
  extra `LayoutItemProxy`, timers, scaling, clipping, resize forcing.
- Bind all three pages to the existing `analysisStatusViewModel` owner.
  Their read-only unavailable-state is truthful until Phase-6 publishes
  canonical authenticated results. No fixture/demo data in Product.
- Remove the experimental `productAnalysisSurface` property, lifecycle
  function/handlers, `Component` factory, `createObject(root)` and
  `destroy()` entirely. Switching between DSPs and analyzers no longer
  constructs or destroys analysis views.

The existing DspWorkspace/Main hosting and qualified
DspFloatingModuleWindow are **unchanged**. Each host has its own persistent
three-page set; when a host is transferred through the existing qualified
dock/floating proxy, its view instances accompany it. Hosts that the
qualified existing Main legitimately destroys on redock are an exception:
permanent is **host lifetime**, not application lifetime. Durable roles,
epoch and source result authority stay in canonical ViewModels/backend.

## Source and runtime gates

1. `tools/qualification/m23/guard_fq99_frozen_gui.py` is updated to
   **subtract only the exact three new QML children**, not to alter or
   normalize any existing qualified control. Independent source compare
   after allowed subtraction MUST equal FQ99 byte-for-byte.
2. `tools/qualification/m19_m21_gui/test_product_navigation_guards.py`
   enforces exactly three pages, identity, order, selection exclusivity,
   correct binding/geometry, no dynamic factory and no second overlay.
3. The original native Qt test
   `tests/ui_smoke/test_source_metadata_panel.cpp` gains additional
   assertions while EQ is selected: all three page instances exist as
   peers of EQ in its ColumnLayout, are hidden, and are disabled. The
   original FQ99 EQ-in-host containment assertion remains unchanged.
4. Focused CI Windows must PASS real all 11 CTests; Linux portable source,
   static and render/project gates must PASS. No CTest bypass and no
   self-certification of M23/FQ.

## Explicit non-qualifications

- Icons currently mapped in `DspChainSelector.qml` for M19/20/21 use
  temporary existing pictograms. The previously approved distinct icon
  design must be retrieved and integrated separately; do not invent it.
- Product `AnalysisStatusEditor` remains a read-only status/source selector,
  NOT the demonstration preview and NOT authoritative numerical visualization.
- The GUI needs later real Windows visual review: all three selections,
  minimum 1184x688, scroll/clipping, 100/125/150/200% DPI, rapid M16/M17/
  M19/M20/M21/Monitor switching, independent SOURCE/REFERENCE/PROCESSED
  roles, floating/redock including two simultaneous windows, identity,
  keyboard focus, and memory stability. User currently not at PC.
- No full FQ, release ticket, main merge, numerical M19-M21 qualification
  or M23 closure is implied by a focused green CI. PR #99 remains Draft.

## Native Product-Main visual gate — next scoped step after green #33

PO operating rule: immediately initiate the next feasible step after each
completed output, without requiring another "prosegui"; never bypass formal
PO gates, physical checks, or single-writer CAS.

A focused post-#33 visual capture check is prepared on the isolated staging
writer. **Unlike the pre-existing M19-M21 screenshot workflow, which captures
a demonstration-only QML workbench**, this new step captures the actual
`Main.qml` + `DspWorkspace` + `DspEditorHost` + three permanently inserted
`AnalysisStatusEditor` objects by selecting real
`ProductAnalysisModuleAdapter` identities
`analysis:rgsml.m19`, `analysis:rgsml.m20`, `analysis:rgsml.m21`.

The existing giant `tests/ui_smoke/test_source_metadata_panel.cpp` remains
unchanged through its historical FQ99 legacy checks; only after they finish
it binds a fail-closed `UnavailableAnalysisVisualFixture` (read-only, real
Product QML but synthetic status ONLY, no numeric Product data), then adds
the three real analysis rows to the existing adapter model. For each:
- verify corresponding active editorContentKey and exactly one visible
  enabled new panel, other two hidden/disabled;
- verify read-only Product authority=false, lifecycle=MISSING;
- capture native Windows Main at 1184x688 and 1440x900;
- assert the actual selected panel stays within its original host;
- roundtrip to M19 and back to the frozen Parametric EQ, verifying persistent
  QQuickItem identity and legacy EQ geometry after navigation.
The screenshots are explicitly titled
`PRODUCT_REAL_MAIN_m19-levels|m20-tonal-stereo|m21-metrics-delta_WIDTHxHEIGHT_UNAVAILABLE.png`;
they are NOT generated-demo artwork, not screenshots of a fully authorized
Phase-6 result, and not a physical user's PC.

The workflow first executes ALL original strict M23 Windows CTests with no
screenshot environment (unchanged), then performs a SEPARATE strict CTest
run of only the already-existing complete 16-case native Qt smoke with
`RGSML_GUI01_EVIDENCE_DIR` enabled. It uploads ONLY the six Product Main
PNG images, never the many pre-existing additional capture files or complete
job logs. Linux and original FQ99 byte-freeze controls remain intact.
Any failure is RED, not silently translated into an automatic approval.
No source change to app, Main, DSP, FIR, product analysis authority, Qt
geometry or CTest wrapper. Focused green remains nonrelease; PO can review
real captured panels and separately authorize later icon/full-FQ/physical work.
