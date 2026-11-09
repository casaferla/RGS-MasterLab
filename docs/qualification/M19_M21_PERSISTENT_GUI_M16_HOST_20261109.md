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
