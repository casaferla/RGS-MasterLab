# M19 Input Gain readout adaptation + CI #9 diagnostic, 7 November 2026

PO asked whether the prominent numerical measurement readings on the first analysis panel should adopt the Input Gain readout's font and size. Decision: yes for M19 five primary metric tiles, subject to real screenshot readability at 1184×688/1440×900; preserve smaller numeric hierarchy for M20 and M21.

Verified `InputGainEditor.qml`: 11px bold Segoe UI label and 36px bold Consolas gain reading. The new `heroReadout` switch in `StudioReadOnlyMeasurement.qml` uses exactly these typography tokens for M19, preserves a small 11px unit in an adjacent label to avoid overflowing five relatively narrow columns, and suppresses the rectangular editable-looking well only for large readings. When unavailable, show a large dash with UNAVAILABLE text in the global status—not a fabricated number. M20 keeps existing compact readonly field; M21 tabular numeric styling remains Consolas.

M19 tile height is increased to 162–180px within the existing proportional workspace. At the smallest design resolution 1184×688 the final height, wrapping, unit alignment and readability still require manual screenshot review. No real PCM result is authorized by this change.

CI #9 run 42421518624 was RED after successful compilation and generation of all 12 authentic Qt PNGs. Bounded failure artifact (1498 bytes) gave `RGSML_GUI_SCREENSHOT_FAILURE ANALYSIS_STUDIO_READOUT_MISSING`. The previous QObject::findChild test was not suitable for a dynamically instanced Repeater delegate following LayoutItemProxy visual reparenting. The test now recursively walks QQuickItem::childItems() for the displayed field, checks readonly/unauthenticated values and verifies M19 is exactly 36px in its declared readout. If CI remains red, use only bounded diagnostics; do not read full GitHub job logs.

Still a design prototype: NO Main.qml integration, no Product owner measurements, no validation of M16 True Peak publication, no merge and no FQ. The GUI must remain three independently dockable/read-only analysis panels.
