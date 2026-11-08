# M18–M22 + public REFERENCE GUI — isolated pre-integration bridge

Date: 2026-11-08. State: DRAFT / TECHNICAL PRE-INTEGRATION / NO MAIN MERGE.

## Immutable inputs
- M22 candidate PR #89, SHA `5a5b9b7e4b01779832e32662d5c6b66b23087ce9`: Focused CI #12 Windows/Linux SUCCESS. This history already contains sequential M18→M19→M20→M21→M22 code.
- Public REFERENCE and standalone M19–M21 Qt GUI PR #95, SHA `b7a30ad2fb22a9d718a3e75adbf8d337bbccf6dd`: Focused CI #6 Windows/Linux SUCCESS.

## Import protocol
Single writer: `staging/m18-m22-reference-ui-convergence-no-ci-20261108`. Candidate for CI only: `staging/m18-m22-reference-ui-bridge-candidate-20261108`. Imported 32 immutable Git blobs exactly from PR #95, with NO merge/cherry-pick of divergent branch histories. Root `tests/CMakeLists.txt` is the only overlapping modified path: compose the 34-line standalone M19–M21 test registration into frozen M22 CMake, leaving all M22 Product targets unchanged. No old branch-specific REFERENCE workflow is imported because its fixed base guard is inapplicable here; bridge guard independently verifies source blob provenance and M22 invariants.

## Boundaries
- Strictly non-Product M19–M21 QML preview; fixture/demo map, `productionAuthorized=false`, no Main.qml analyzer registration and no fabricated spectrum/true-peak/owner status.
- The public user interface says REFERENCE. Technical `GOLD` tokens, project schema, API, and gold-color names remain frozen.
- Do NOT claim canonical Product owner/Phase-6 Gate, physical Windows acceptance, M18 A01–A15, final FQ, or M18–M22 closure merely because focused CI passes.
- M17 GR HISTORY redesign remains separate PR #85 and **follows** M18–M22 in PO sequence.
- No merges to main or predecessor branches without explicit PO approval. No automatic CMake alteration outside isolated typed Qt target registration.

## Gate
Run the isolated provenance checker, real C++ M19/M20/M21 and M22 whole-Master parity tests on the appropriate OS, canonical real Qt module REFERENCE smoke and standalone 24-shot Qt UI harness. Preserve bounded failure artifacts; keep candidate frozen while CI runs. Subsequent work after this gate: connect genuine M19/M20/M21 typed Product signals to independent QML wrappers under canonical owner/provenance authorization, then resolve remaining M22 scheduler/Phase-6/real M18 export gates and PO physical acceptance.

## Follow-up macro-lot — real M18 Product readiness to independent Qt panels (after CI #1 PASS)

Source candidate PR #96 @ `064a8dd6c433c8313e7e75c6175c839a3bbdf953`: Focused Bridge CI #1 [Windows/Linux SUCCESS](https://github.com/casaferla/RGS-MasterLab/actions/runs/42482702607). The PO requested to continue M18–M22 completion before separate M17 GR HISTORY.

New private header `app/src/internal/m21_product_qt_readiness_bridge.hpp` consumes the **real**, Qt-owner-validated `M21ProductPublicationInbox::view_current` and M18 Complete Binding `Fanout::OwnerSnapshot`. Three independent M19/M20/M21 routes select PREPARED/REFERENCE/PROCESSED without mutating or merging the underlying evidence; source role REFERENCE is a user-facing label for the persisted internal GOLD column. The identity fingerprint contains the M18 epoch, route revision, independently selected column, original result identity, producer generation, scope, frame domain and lifecycle. A delayed frame is rejected if any changes. A worker call returns static empty maps before touching routing or Product state.

**Important: this is not Product GUI activation.** All three Qt maps are always `ready=false`, `fixtureMode=false`, `productionAuthorized=false`, with no LUFS, TP, PSD, tonal bands or metrics numbers. Their additional `privateEvidenceStatus` reflects the actual private inbox as CURRENT/STALE/MISSING but cannot confer canonical Phase-6 authorization. The original private typed snapshots remain owned and revalidated by M21 Product, not QML.

The real Windows `rgsml_project_session_tests` now validates:
- Actual verified M17/M18 ProductSession PREPARED/PROCESSED analysis → distinct private panel roles/current owner with M16 TP withheld.
- Independent true WAV REFERENCE selection → both selected REFERENCE panels CURRENT while absent PREPARED/PROCESSED are MISSING; clearGold revokes both.
- A per-panel source routing revision makes a previous queued Qt frame stale without changing audio data.
- Parameter-only Gain change revokes both M17 verified Master and old panel epoch; no state inherited from the merely unchanged structural chain revision.
- Worker thread is denied access before any routing/Qt owner inspection and receives only empty maps.
- Earlier M22 whole-Master Preview/Full bitwise parity and M18 WAV export tests are **preserved without modification**, not replaced by these new assertions.

The bridge provenance checker now permits only additive tests in `test_project_session.cpp`, never deleting/changing original M22 test lines; the source M22 Product baseline remains frozen. Subsequent work MUST obtain real Phase-6 status authority, implement a release-qualified Product presenter, and then connect it to the independent docked/floating Qt wrappers with physical PO verification. Do not reclassify the private bridge as production.
