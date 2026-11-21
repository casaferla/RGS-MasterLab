# M19–M21 typed Qt/QML convergence — isolated, non-FQ (2026-11-08)

## Integration source and immutable prerequisites

- PR #92 C++ typed review, Windows + Linux CI #14 SUCCESS, SHA `72c1da5993bf0ec13768a6b66f311f6420ead81c`, is the **branch base**.
- PR #91 standalone M19/M20/M21 graphic preview, Windows Qt CI #13 SUCCESS, SHA `0b0a617eabfba3e3a31905bad819a6ff98bc33b6`, is a **source of selected file content only**.
- Both PRs remain Draft and untouched. This branch imports nine verified GUI/QML and screenshot files via immutable Git blobs rather than merging the diverged histories. This is essential: PR #91's separate history contains unrelated M22 content and must NOT overwrite the PR #92 M21 backend.
- Writer branch: `staging/m19-m21-qt-typed-convergence-no-ci-20261108`. Candidate branch: `staging/m19-m21-qt-typed-convergence-candidate-20261108`. One focused CI per candidate promotion; no merge until PO approval.

## New real C++ → actual QtQuick acceptance

`tests/unit/app/test_m21_qml_typed_convergence.cpp` allocates a REAL 48-kHz mono PCM sine wave, invokes real M19 level analysis and M20 stereo/tonal analysis, imports into the private M21 typed comparison, supplies an exact test owner receipt, and captures the independent M19/M20/M21 data maps. Qt loads the **actual** `ui/qml/M19M21AnalysisWorkbenchPreview.qml` and checks:

1. A measured RMS reaches the **actual** M19 read-only QQuickItem with Input Gain-like 36-px Consolas display, no editable control. Disabling `fixtureMode` masks even the already populated map.
2. M20 still displays a valid (mono NOT APPLICABLE) stereo status, **but explicitly displays spectrum and tonal-profile UNAVAILABLE**. A level or stereo measurement must never generate an imaginary PSD or claim measured spectral data.
3. M21 9-row comparison preserves missing GOLD as UNAVAILABLE and cannot show an old table after a new PREPARED generation. Source/routing revisions are validated prior to fixture delivery.

These are **preproduction TEST FIXTURES**, never canonical Product data. `productionAuthorized` stays false; the standard `unpublished_panels()` map contains no numeric content. The actual app `Main.qml` is NOT registered or changed.

## GUI and CI

The isolated Windows screenshot harness from PR #91 still checks three independent QQuickViews, a single persistent item through dock↔float transitions, min floating dimensions, MISSING/STALE states and 24 real Qt captures (1440×900 and 1184×688). The new QML change separates `spectrumAvailable` and `tonalBandsAvailable` from overall `available`, so a real M20 stereo result never falsely marks the static spectral plot as valid.

One Windows+Linux GitHub Actions workflow `m19-m21-qml-typed-convergence.yml` builds/tests the typed M21 regressions and new true PCM→QtQuick test. The Windows job additionally runs the original 24-photo GUI regression and retains artifacts. Failure diagnostics are **bounded** (max last 55–65 lines), never full job logs.

## Blocks remaining after a green CI

This focused green would mean **preproduction convergence verified**, NOT a release-ready GUI. Mandatory before Main activation: canonical PREPARED/REFERENCE/PROCESSED owner receipts issued by Product rather than test claims; authenticated per-cell M19/M20/M21 ViewModels; actual PSD and tonal-band source publication; window-docking integration in app; M18 A01–A15 physical tests, Windows 4K+FHD acceptance, ordered M18–M22 integration/qualification, final FQ and explicit PO merge authorization. Existing internal `GOLD` identifiers are deliberately preserved; a display name change to Reference must not break project/API compatibility.
