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
