# M19–M21 — GUI + typed C++ convergence on isolated review-only branch

Date: 2026-11-08. **Not a Product release; no `main` merge.**

## Parentage and immutable inputs

- Based on PR #92 (typed candidate): `72c1da5993bf0ec13768a6b66f311f6420ead81c`, dual-platform focused CI #14 Windows+Linux SUCCESS.
- The six actual QtQuick components are ported **by matching Git blob SHA** from GUI Draft PR #91, head `0b0a617eabfba3e3a31905bad819a6ff98bc33b6`. This deliberately avoids merging unrelated PR91 ancestors, old M17 history, M18 and DSP trees.
- Legacy 24-shot Qt6 Design Review harness also copied exactly from PR #91; remains a separate visual regression.
- Both original PRs remain Draft and untouched. This new branch alone is the integration candidate.

## What is integrated

- A new executable `rgsml_m19_m21_typed_qml_convergence_tests` loads genuine independently instantiated M19, M20 and M21 QtQuick panels with a `QGuiApplication` (offscreen backend). It measures a **real** 48 kHz mono 440 Hz AudioBuffer using the actual public M19/M20 analyzers, passes the output through the typed M21 Product-import candidate, the private owner/epoch store, independent routing, and the private QVariantMap review adapter. QtQuick receives only explicit, watermarked TEST data.
- Explicit GUI safety changes: `fixtureMode` defaults **false**, even for the preview. A `ready` presentation without explicit fixture authority never becomes visible. On stale/missing/invalid lifecycle or revoked fixture permission, previously visible values are hidden. No QML numeric input/editing is introduced.
- M20 availability is granular. A valid stereo number does not permit displaying a fake PSD or tonal shape. At least two demo spectrum points and an eligible test status are required; unknown/STALE/UNAVAILABLE status blocks plots. Bands require a non-empty verified demo series. M21 comparison rows are independently guarded against an empty matrix.
- User-facing wording **REFERENCE** replaces GOLD without renaming the internal `gold` data fields, API or object IDs; no hazardous mechanical migration of project state.
- Independent panel selection remains per M19/M20/M21 in the typed review adapter. An old revision is rejected after a result/generation or selection change; PREPARED changing does not invalidate an independently current reference, and M16 True Peak numeric remains unavailable pending canonical release.
- Windows CI creates the legacy 24 PNG screenshots plus the QtQuick-test screenshots of M19/M20/M21 with **actual computed M19 RMS** and a STALE M19 panel. Linux and Windows compile/run both this GUI test and existing M21 typed + real Product-port regression. Bounded diagnostics only, no full job logs.

## Known gates remaining

The typed envelopes use `ExplicitTestFixture` (intentionally not Product-authenticatable). Neither `Main.qml` nor app C++ wiring owns these panels. The official Phase-6 FeatureDefinitionRegistry, real resource/source/epoch owners, per-metric M20 PSD publisher and associated support validity, M21 partial-comparison semantics, and live GUI signals are not implemented here. The prototype's `productionAuthorized` remains **false** and cannot become true by a test receipt claim.

M18 A01–A15 physical tests, Windows 4K+FHD docking/listening acceptance, ordered milestone M18→M22 merges, complete M22 render execution and final shared FQ remain mandatory. **No auto-merge.** The sole CI outcome is a focused non-FQ UI/back-end convergence gate.

## Development control

Use one no-CI single-writer staging branch `staging/m19-m21-gui-typed-convergence-no-ci-20261108`. Collect a substantial macro-lot, then create/promote one candidate `staging/m19-m21-gui-typed-convergence-candidate-20261108`, one Windows+Linux CI, and freeze the candidate until terminal. Do not fast-forward PR #91/#92 or `main`.
