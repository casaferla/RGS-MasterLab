# M19–M21 GUI: Private single-writer review epochs — non-production gate

**Date:** 2026-11-08. **Source:** Draft PR #92; prior Windows/Linux focused CI #11 `42461104515` SUCCESS at `7adb514aef7b1d7c233dcc7a1fa1a4b40d17f366`.

The user selected **three independent M19/M20/M21 panels** with UI elements consistent with DSP modules, including Input Gain-style 36px M19 primary numbers. The isolated GUI remains Draft PR #91, not part of this PR #92 patch.

## What this macro-lot implements

New private header `app/src/internal/m21_review_epoch_store_candidate.hpp` manages one mutable, noncopyable **preproduction** store across independent PREPARED, GOLD and PROCESSED columns. Each begin(column,generation) requires a strictly advancing, nonzero producer generation and invalidates only the selected column. The GOLD reference and other analyzer columns are unaffected by source-specific invalidation. Receipt-based receive() checks exact owner claim, realization, column, generation, scope kind, begin/count, sample rate and frame-domain assertion, then delegates to the preexisting PrivatePublicationGuard to reject late results, duplicates and corrupt metrics. Selection changes the preview column, not the stored data or owner receipts.

The `snapshot()` method recomputes per-cell M19–M21 readonly comparisons each time; no stale Qt QVariantMap survives a generation transition. `unpublished()` is always a completely masked envelope, regardless of data. **Only** `test_preview(ExplicitTestFixture)` may provide watermarked review data, and this is NEVER canonical Product authorization or a `Main.qml` integration. Copy and move construction/assignment are deleted to prevent duplication of single-writer ownership.

## Tests

The expanded `tests/unit/app/test_m21_ui_readonly_projection_candidate.cpp` covers: empty store, begin→receive, supersede and late completion, duplicate first-current protection, GOLD and PROCESSED independence, selective stale deltas, fresh GOLD updating both differences, wrong source column/scope/owner, invalid frame domain, malformed numeric source, unsupported rate, invalid panel selection, unselected/pending columns, and unauthorized test-fixture denial. Existing real 48-kHz M19/M20 AudioBuffer RMS → QVariantMap review and granular quality/owner tests remain intact.

## Integration blockers

No `Main.qml` activation; no real Product owner publication; no Phase-6 status certification; no merge of M18–M22; no changes to M16/M17, FIR or DSP processing. The main thread must remain sole writer until real cancellation/worker dispatch is designed. These tests do not replace A01–A15, authentic PREPARED/GOLD/PROCESSED data lifecycle, 4K+FHD physical Windows testing, or one final Full Qualification. A successful focused CI is a preproduction code gate only.

## Single CI acceptance for this batch

One Windows/Linux focused `m19-m21-typed-ui-projection.yml` CI must compile the new header through the same QtTest target and pass the existing M21 real-port regression plus all private review state tests. The candidate branch remains frozen while that CI runs. No merge to `main` without the Product Owner's explicit GO.
