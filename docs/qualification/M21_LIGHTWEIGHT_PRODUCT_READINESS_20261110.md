# RGS MasterLab — Lightweight M19–M21 Product Readiness
**Date:** 10 November 2026
**Scope:** nonrelease, status-only private evidence; focused CI required.

## Problem and goal

Before this lot `M21ProductQtReadinessBridge::capture` called `M21ProductPublicationInbox::view_current()`, which built a complete M21 comparison and copied M19/M20 `Snapshot` values including potentially thousands of PSD samples and tonal profiles. The Qt GUI only consumes source identity, scope, lifecycle and nine *quality enum* flags; it does not display private numerical values. Copying all sample arrays on frequent status notifications is unnecessary GUI-thread work and may disturb responsive minimum-window behavior.

## Typed status-only path

- `m21_private::ReadinessSnapshot` holds the exact COLUMN, generation, resource identity, absolute scope, frame count, sample rate, lifecycle and nine `Quality` enum flags, never `Cell`, `double`, spectrum, tonal-band list, or audio buffer. All nine default to `MISSING` (enum zero would otherwise imply `PRESENT`).
- `PrivatePublicationGuard::readiness_columns()` projects from the authoritative private published slots in O(9) work per column plus identity string copies. Full `Snapshot` and `Comparison` evidence stay in the inbox, unchanged.
- `M21ProductPublicationInbox::view_current_readiness()` performs the exact same Qt-owner SOURCE/PREPARED/PROCESSED/REFERENCE retirement checks used by full `view_current()`. A single templated `retire_stale` method prevents the two paths from drifting.
- `M21ProductQtReadinessBridge::capture` consumes only `view_current_readiness()`, maps all nine quality flags to existing textual labels, and still marks M16 true peak as unqualified.
- All three independent analysis panels and SOURCE selection remain as previously qualified. No QML shapes or layout changes, numeric measurement publication or new FeatureDefinitionRegistry.

## Evidence and safeguards

Nine focused native tests exercise: a smaller non-numeric receipt, independent PREPARED/GOLD/PROCESSED scope/rate/identity, full 8,193-bin PSD retention in the private publisher with a tiny readiness receipt, nine distinct quality flags, per-column STALE transitions, malformed replacement fail-closed, unknown producer role, copy independence and empty-receipt default MISSING. Existing M18 epoch/realization checks and Qt `same_visible_frame` equality remain unchanged.

This **does not** establish Phase-6 producer authority, Gate S/U/G/M, numeric QML publication, physical Windows acceptance, M18 Export B01 approval or Full Qualification. Existing private M21 numerical analysis and reference-minus-current delta code remain available only for future authorized Gate G integration.

## Next step

Run the frozen Windows+Linux M23 focused workflow. If both pass, retain this as the nonrelease status-only optimization and proceed with canonical Phase-6 authority planning without unlocking numbers. The user-approved UI label remains REFERENCE; internal GOLD APIs and file tokens must not be renamed.
