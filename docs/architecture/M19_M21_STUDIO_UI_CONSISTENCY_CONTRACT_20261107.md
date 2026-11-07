# M19–M21 Studio visual contract (PO-approved requirement) · 7 November 2026

Three independent M19/M20/M21 panels are the PO choice. All comparable UI primitives must match the earlier DSP modules, not only share a palette.

The independent dock host now uses the **actual preexisting StudioButton.qml** for Dock/Undock, retaining its focus, hover, border, radius and type. The newly added StudioReadOnlyMeasurement.qml precisely follows the existing StudioNumericField **visual** rules (Segoe UI headings, Consolas numeric values, navy #0E1F2E field, border #2C5A78 and compact sizing), while exposing no TextInput or editing pathway. M19 has five read-only fields; M20 has Width and P10/P50/P90; M21 tabular numbers use Consolas with Segoe headings. Common panes match EQ and Stereo M/S backgrounds and strokes. Colors retain established semantic meaning.

There are no configurable analysis parameters in this scope; editable StudioNumericField would suggest a false ability to change measured values. No fake buttons or editable text entries are added. Scope PREPARED/GOLD/PROCESSED remains observational; an interactive mode must later use real StudioSegmentButton connected to authenticated Product owners.

**Guardrails:** no Main.qml activation, no M19–M22 Product result binding. fixtureMode=false shows DATA UNAVAILABLE, source/scope unavailable, and never a made-up TP. Qt visual tests are not algorithm tests or Full Qualification. The next Windows CI must inspect 1184×688 and 1440×900 and certify Studio control instantiation plus dock/undock lifecycle before proceeding. Linux remains covered separately by analyzer-focused workflows.
