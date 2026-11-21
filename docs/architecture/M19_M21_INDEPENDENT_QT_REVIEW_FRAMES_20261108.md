# L1 M19/M20/M21 independent Qt review frames — staged interface contract

Date: 8 November 2026. Purpose: bridge the **already-tested** typed C++ private M21 evidence pipeline toward the three **independent** M19/M20/M21 QML analysis panels (separate graphics PR #91) without merging the two preproduction branches or activating Main.qml.

## Previous qualified baseline

The focused Linux + Windows CI #12 run https://github.com/casaferla/RGS-MasterLab/actions/runs/42462584210 completed SUCCESS on commit `86739aad44bcd5904789c365092fbf88c38d446c`. This is not a Full Qualification and not actual Product owner authority.

## New review-only interface

- `ReviewEpochStore::review_revision()` increments after successful generation begin, result receive, or changed selected analysis column, but not after rejected/replayed callbacks or redundant selection. `is_current_review(revision)` refuses empty or obsolete review IDs. This is a PRIVATE display revision, not a canonical processing generation or authentication proof.
- `m21_independent_panels_review_candidate.hpp` creates **one typed snapshot and one selected scope** per capture, then three separately tagged maps `m19`, `m20`, `m21`. Values are permitted only via explicit `ExplicitTestFixture`, always tagged DEMO/test-only, never published to Product.
- M19 receives only its five level fields and per-metric quality labels. True Peak is withheld until separate canonical M16 publication authority exists. M20 receives stereo width and P10/P50/P90 with their individual statuses. M20 spectrum and tonal bands remain EMPTY/UNAVAILABLE: never infer spectra or log-magnitude shapes from level readings. M21 receives nine metric rows, Gold-minus-Current deltas, quality labels and independent PREPARED/GOLD/PROCESSED lifecycles, not a false time-alignment proof.
- Three maps are least-privilege: there is no unnecessary cross-panel measurement payload. Existing graphical component styling (Input Gain 36px Consolas for M19) is maintained in GUI PR #91 and is NOT copied or rewritten into this branch.
- A queued review frame must pass `review_frame_current(store,frame)` before any UI consumer adopts it; a new producer generation, accepted result or changed scope selection makes older frames unusable. This is a test-only concept: real production Main.qml wiring requires canonical epoch and owner proofs.
- `unpublished_panels()` is fail-closed even if the private store has accepted a demonstration receipt. No normal release path returns numeric values.

## Regression scenarios

The focused QtTest now checks revision monotonicity, rejected callback idempotence, module selection, stale frames on reanalysis, late completions, independent GOLD, processed measurements, M21 column/delta provenance, M19/M20 per-metric qualities, mono NOT APPLICABLE, missing/STALE sources, non-generated spectra, panel map isolation and tamper resistance. The end-to-end review case generates a **real** 48 kHz mono 440 Hz AudioBuffer, computes actual M19/M20 analysis, imports it through the real typed M21 metrics port, validates expected owner receipt, and checks displayed RMS within rounding precision while production remains fully masked.

## Strict release blockers

- NO `Main.qml` integration or physical Windows UI acceptance. NO QML Test Harness import from the GUI Draft PR #91 into this backend branch.
- Owner-receipt booleans are *test assertions*, not canonical Product evidence. `productionAuthorized` stays false.
- NO assumption that GOLD has a shared sample timeline with PREPARED/PROCESSED.
- NO modification of the main branch, M16/M17, FIRs, M18 export, DSP, or released M19–M22 algorithms.
- M18 A01–A15, real Product epoch/owner end-to-end, live UI 4K+FHD, cross-milestone integration Gate and final FQ remain outstanding.

**Acceptance for this macro-lot:** exactly one focused Windows + Linux CI of tests for new review contracts and existing real M21 port, and no merge without explicit PO authorization.
